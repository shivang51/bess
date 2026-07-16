#include "platform/x11/x11_window_drop_handler.h"

#include "common/logger.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Bess::Platform::X11 {
    namespace {
        constexpr long kXdndVersion = 5;
        constexpr auto kSelectionNotifyTimeout = std::chrono::milliseconds(750);
        constexpr unsigned long kMaxDropPayloadBytes = 4UL * 1024UL * 1024UL;
        constexpr long kMaxDropPayloadLongs =
            static_cast<long>((kMaxDropPayloadBytes + 3UL) / 4UL);
        constexpr std::size_t kMaxDroppedPaths = 4096;
        constexpr std::size_t kMaxPathBytes = 64UL * 1024UL;

        struct XdndAtoms {
            Atom xdndAware = None;
            Atom xdndEnter = None;
            Atom xdndPosition = None;
            Atom xdndStatus = None;
            Atom xdndLeave = None;
            Atom xdndDrop = None;
            Atom xdndSelection = None;
            Atom xdndProxy = None;
            Atom xdndFinished = None;
            Atom xdndTypeList = None;
            Atom xdndActionCopy = None;
            Atom textUriList = None;
            Atom xSpecialGnomeCopiedFiles = None;
            Atom utf8String = None;
            Atom textPlainUtf8 = None;
            Atom textPlain = None;
        };

        struct SelectionPayload {
            std::string data;
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long bytesAfter = 0;
        };

        Atom internAtom(Display *display, const char *name) {
            return XInternAtom(display, name, False);
        }

        XdndAtoms createAtoms(Display *display) {
            return {
                .xdndAware = internAtom(display, "XdndAware"),
                .xdndEnter = internAtom(display, "XdndEnter"),
                .xdndPosition = internAtom(display, "XdndPosition"),
                .xdndStatus = internAtom(display, "XdndStatus"),
                .xdndLeave = internAtom(display, "XdndLeave"),
                .xdndDrop = internAtom(display, "XdndDrop"),
                .xdndSelection = internAtom(display, "XdndSelection"),
                .xdndProxy = internAtom(display, "XdndProxy"),
                .xdndFinished = internAtom(display, "XdndFinished"),
                .xdndTypeList = internAtom(display, "XdndTypeList"),
                .xdndActionCopy = internAtom(display, "XdndActionCopy"),
                .textUriList = internAtom(display, "text/uri-list"),
                .xSpecialGnomeCopiedFiles =
                    internAtom(display, "x-special/gnome-copied-files"),
                .utf8String = internAtom(display, "UTF8_STRING"),
                .textPlainUtf8 =
                    internAtom(display, "text/plain;charset=utf-8"),
                .textPlain = internAtom(display, "text/plain"),
            };
        }

        bool hasAtom(const std::vector<Atom> &atoms, Atom needle) {
            return std::ranges::find(atoms, needle) != atoms.end();
        }

        void appendUniqueAtom(std::vector<Atom> &atoms, Atom atom) {
            if (atom != None && !hasAtom(atoms, atom)) {
                atoms.push_back(atom);
            }
        }

        int hexValue(char value) {
            if (value >= '0' && value <= '9') {
                return value - '0';
            }

            value = static_cast<char>(
                std::tolower(static_cast<unsigned char>(value)));
            if (value >= 'a' && value <= 'f') {
                return value - 'a' + 10;
            }

            return -1;
        }

        std::string percentDecode(std::string_view value) {
            std::string result;
            result.reserve(value.size());

            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] != '%' || i + 2 >= value.size()) {
                    result.push_back(value[i]);
                    continue;
                }

                const int high = hexValue(value[i + 1]);
                const int low = hexValue(value[i + 2]);
                if (high < 0 || low < 0) {
                    result.push_back(value[i]);
                    continue;
                }

                result.push_back(static_cast<char>((high << 4) | low));
                i += 2;
            }

            return result;
        }

        std::optional<std::string> fileUriToPath(std::string_view uri) {
            constexpr std::string_view fileScheme = "file:";
            if (!uri.starts_with(fileScheme)) {
                return std::nullopt;
            }

            std::string decoded = percentDecode(uri);
            std::string_view rest(decoded);
            rest.remove_prefix(fileScheme.size());

            if (rest.starts_with("//")) {
                rest.remove_prefix(2);
                const auto pathStart = rest.find('/');
                if (pathStart == std::string_view::npos) {
                    return std::nullopt;
                }

                const auto host = rest.substr(0, pathStart);
                if (!host.empty() && host != "localhost") {
                    return std::nullopt;
                }

                rest.remove_prefix(pathStart);
            }

            if (rest.empty() || !rest.starts_with('/') ||
                rest.find('\0') != std::string_view::npos) {
                return std::nullopt;
            }

            return std::string(rest);
        }

        std::vector<std::string> parseLocalPaths(std::string_view data) {
            std::vector<std::string> paths;
            paths.reserve(8);
            std::unordered_set<std::string> seen;
            seen.reserve(8);
            std::size_t offset = 0;

            while (offset < data.size() && paths.size() < kMaxDroppedPaths) {
                const auto lineEnd = data.find('\n', offset);
                const auto lineLength = lineEnd == std::string_view::npos
                                            ? data.size() - offset
                                            : lineEnd - offset;
                auto line = data.substr(offset, lineLength);
                offset = lineEnd == std::string_view::npos ? data.size()
                                                           : lineEnd + 1;

                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }

                if (line.empty() || line.size() > kMaxPathBytes ||
                    line.front() == '#' ||
                    line.find('\0') != std::string::npos) {
                    continue;
                }

                // Nautilus/Thunar may include the operation name before URIs
                // for x-special/gnome-copied-files.
                if (line == "copy" || line == "move" || line == "cut") {
                    continue;
                }

                auto path = fileUriToPath(line);
                if (path) {
                    if (seen.emplace(*path).second) {
                        paths.emplace_back(std::move(*path));
                    }
                    continue;
                }

                if (line.front() == '/') {
                    std::string path(line);
                    if (seen.emplace(path).second) {
                        paths.emplace_back(std::move(path));
                    }
                }
            }

            return paths;
        }

        int packedPositionX(long packedPosition) {
            return static_cast<int>(
                static_cast<int16_t>((packedPosition >> 16) & 0xffff));
        }

        int packedPositionY(long packedPosition) {
            return static_cast<int>(
                static_cast<int16_t>(packedPosition & 0xffff));
        }

        std::string atomName(Display *display, Atom atom) {
            if (!display || atom == None) {
                return "None";
            }

            char *name = XGetAtomName(display, atom);
            if (!name) {
                return std::to_string(atom);
            }

            std::string result(name);
            XFree(name);
            return result;
        }

    } // namespace

    struct WindowDropHandler::Impl {
        enum class SelectionRequestPhase {
            none,
            positionPrefetch,
        };

        Impl(_XDisplay *display, X11WindowHandle targetWindow) {
            init(display, targetWindow);
        }

        ~Impl() {
            shutdown();
        }

        void init(_XDisplay *nativeDisplay,
                  X11WindowHandle nativeTargetWindow) {
            if (!nativeDisplay || nativeTargetWindow == 0) {
                return;
            }

            // The target was created on another X connection. Make that request
            // visible to the server before the XDND connection references it.
            XSync(nativeDisplay, False);

            display = XOpenDisplay(XDisplayString(nativeDisplay));
            if (!display) {
                BESS_ERROR("[WindowDrop] Failed to open a dedicated X11 "
                           "connection for XDND");
                return;
            }

            targetWindow = static_cast<::Window>(nativeTargetWindow);
            atoms = createAtoms(display);

            proxyWindow = XCreateWindow(display,
                                        DefaultRootWindow(display),
                                        -1,
                                        -1,
                                        1,
                                        1,
                                        0,
                                        0,
                                        InputOnly,
                                        CopyFromParent,
                                        0,
                                        nullptr);
            if (proxyWindow == None) {
                BESS_ERROR("[WindowDrop] Failed to create the native XDND "
                           "proxy window");
                XCloseDisplay(display);
                display = nullptr;
                targetWindow = None;
                return;
            }

            XChangeProperty(
                display,
                targetWindow,
                atoms.xdndAware,
                XA_ATOM,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(&kXdndVersion),
                1);

            XChangeProperty(
                display,
                proxyWindow,
                atoms.xdndAware,
                XA_ATOM,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(&kXdndVersion),
                1);

            XChangeProperty(
                display,
                targetWindow,
                atoms.xdndProxy,
                XA_WINDOW,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(&proxyWindow),
                1);

            // A valid XdndProxy must point back to itself. The proxy belongs to
            // this dedicated connection, so GLFW cannot consume its events.
            XChangeProperty(
                display,
                proxyWindow,
                atoms.xdndProxy,
                XA_WINDOW,
                32,
                PropModeReplace,
                reinterpret_cast<const unsigned char *>(&proxyWindow),
                1);

            XSync(display, False);
            initialized = true;
        }

        void shutdown() {
            if (display) {
                if (targetWindow != None) {
                    XDeleteProperty(display, targetWindow, atoms.xdndProxy);
                    XDeleteProperty(display, targetWindow, atoms.xdndAware);
                }
                if (activeSelectionProperty != None) {
                    XDeleteProperty(
                        display, selectionWindow(), activeSelectionProperty);
                }
                if (proxyWindow != None) {
                    XDestroyWindow(display, proxyWindow);
                }
                XSync(display, False);
                XCloseDisplay(display);
            }

            callbacks.clear();
            initialized = false;
            display = nullptr;
            targetWindow = None;
            proxyWindow = None;
            sourceWindow = None;
            acceptedTarget = None;
            activeSelectionTarget = None;
            activeSelectionProperty = None;
            selectionRequestPhase = SelectionRequestPhase::none;
            awaitingSelection = false;
            dragActive = false;
            dropPending = false;
            xdndVersion = 0;
        }

        bool poll() {
            if (!initialized || !display) {
                return false;
            }

            bool handledEvent = false;
            XEvent event;
            while (XCheckIfEvent(display,
                                 &event,
                                 &Impl::isRelevantEvent,
                                 reinterpret_cast<XPointer>(this))) {
                handledEvent = true;
                process(event);
            }

            if (awaitingSelection &&
                std::chrono::steady_clock::now() >= selectionRequestDeadline) {
                handledEvent = true;
                handleSelectionTimeout();
            }

            return handledEvent || dragActive || awaitingSelection ||
                   dropPending;
        }

        WindowDropHandler::SubscriptionId subscribe(Callback callback) {
            if (!callback) {
                return 0;
            }

            const auto subscriptionId = nextSubscriptionId++;
            callbacks.emplace(subscriptionId, std::move(callback));
            return subscriptionId;
        }

        void unsubscribe(WindowDropHandler::SubscriptionId subscriptionId) {
            callbacks.erase(subscriptionId);
        }

        bool isInitialized() const {
            return initialized;
        }

      private:
        ::Window selectionWindow() const {
            return proxyWindow;
        }

        static Bool
        isRelevantEvent(Display *, XEvent *event, XPointer userData) {
            const auto self = reinterpret_cast<Impl *>(userData);
            if (!self || !self->initialized) {
                return False;
            }

            if (event->type == ClientMessage &&
                (event->xclient.window == self->targetWindow ||
                 event->xclient.window == self->proxyWindow)) {
                const Atom messageType = event->xclient.message_type;
                return messageType == self->atoms.xdndEnter ||
                               messageType == self->atoms.xdndPosition ||
                               messageType == self->atoms.xdndLeave ||
                               messageType == self->atoms.xdndDrop
                           ? True
                           : False;
            }

            if (event->type == SelectionNotify && self->awaitingSelection &&
                event->xselection.requestor == self->selectionWindow() &&
                event->xselection.selection == self->atoms.xdndSelection &&
                (event->xselection.property == self->activeSelectionProperty ||
                 event->xselection.property == None)) {
                return True;
            }

            return False;
        }

        void process(const XEvent &event) {
            if (event.type == ClientMessage) {
                processClientMessage(event.xclient);
                return;
            }

            if (event.type == SelectionNotify) {
                processSelectionNotify(event.xselection);
            }
        }

        void processClientMessage(const XClientMessageEvent &message) {
            if (message.message_type == atoms.xdndEnter) {
                handleEnter(message);
            } else if (message.message_type == atoms.xdndPosition) {
                handlePosition(message);
            } else if (message.message_type == atoms.xdndLeave) {
                handleLeave(message);
            } else if (message.message_type == atoms.xdndDrop) {
                handleDrop(message);
            }
        }

        void handleEnter(const XClientMessageEvent &message) {
            resetDragState();
            sourceWindow = static_cast<::Window>(message.data.l[0]);
            xdndVersion = message.data.l[1] >> 24;
            dragActive = true;

            if (xdndVersion > kXdndVersion) {
                emit({
                    .type = WindowDropEventType::enter,
                });
                return;
            }

            const auto offeredTypes = getOfferedTypes(message);
            offeredTargets = prioritizeTargets(offeredTypes);
            acceptedTarget =
                offeredTargets.empty() ? None : offeredTargets.front();

            emit({
                .type = WindowDropEventType::enter,
                .accepted = acceptedTarget != None,
            });
        }

        void handlePosition(const XClientMessageEvent &message) {
            if (!dragActive) {
                return;
            }

            const auto eventSource = static_cast<::Window>(message.data.l[0]);
            if (sourceWindow != None && eventSource != sourceWindow) {
                return;
            }

            sourceWindow = eventSource;
            const bool accepted =
                xdndVersion <= kXdndVersion && acceptedTarget != None;

            if (xdndVersion >= 1) {
                lastPositionTimestamp = static_cast<Time>(message.data.l[3]);
            }

            const long packedPosition = message.data.l[2];
            const int rootX = packedPositionX(packedPosition);
            const int rootY = packedPositionY(packedPosition);
            ::Window child = None;
            if (!XTranslateCoordinates(display,
                                       DefaultRootWindow(display),
                                       targetWindow,
                                       rootX,
                                       rootY,
                                       &lastPositionX,
                                       &lastPositionY,
                                       &child)) {
                lastPositionX = rootX;
                lastPositionY = rootY;
            }
            hasLastPosition = true;

            if (!accepted) {
                completePosition(false);
                return;
            }

            if (prefetchComplete) {
                completePosition(hasValidPrefetchedData());
                return;
            }

            if (awaitingSelection) {
                return;
            }

            const auto selectionOwner =
                XGetSelectionOwner(display, atoms.xdndSelection);
            if (selectionOwner != sourceWindow) {
                BESS_WARN("[WindowDrop] Cannot prefetch XdndSelection: "
                          "owner {} differs from source {}",
                          selectionOwner,
                          sourceWindow);
                prefetchComplete = true;
                completePosition(false);
                return;
            }

            // XDND requires selection conversion to use the XdndPosition
            // timestamp. Fetch now and cache the payload; waiting until
            // XdndDrop lets some XWayland/GTK sources expose clipboard data.
            beginSelectionRequest(acceptedTarget, lastPositionTimestamp);
        }

        void handleLeave(const XClientMessageEvent &) {
            emit({
                .type = WindowDropEventType::leave,
            });
            resetDragState();
        }

        void handleDrop(const XClientMessageEvent &message) {
            const auto eventSource = static_cast<::Window>(message.data.l[0]);
            if (!dragActive) {
                sendFinishedTo(eventSource, false);
                return;
            }

            if (sourceWindow != None && eventSource != sourceWindow) {
                sendFinishedTo(eventSource, false);
                return;
            }

            sourceWindow = eventSource;
            const bool accepted = hasValidPrefetchedData();

            dragActive = false;
            dropPending = true;

            if (awaitingSelection) {
                return;
            }

            if (!prefetchComplete) {
                BESS_WARN("[WindowDrop] Rejecting XdndDrop because no "
                          "XdndPosition payload was prefetched");
            }

            finishDrop(accepted);
        }

        void beginSelectionRequest(Atom target, Time timestamp) {
            activeSelectionTarget = target;
            activeSelectionProperty = nextSelectionProperty();
            selectionRequestPhase = SelectionRequestPhase::positionPrefetch;
            selectionRequestDeadline =
                std::chrono::steady_clock::now() + kSelectionNotifyTimeout;
            XDeleteProperty(
                display, selectionWindow(), activeSelectionProperty);
            XSync(display, False);
            awaitingSelection = true;
            XConvertSelection(display,
                              atoms.xdndSelection,
                              activeSelectionTarget,
                              activeSelectionProperty,
                              selectionWindow(),
                              timestamp);
            XFlush(display);
        }

        void processSelectionNotify(const XSelectionEvent &selection) {
            awaitingSelection = false;

            if (selectionRequestPhase !=
                SelectionRequestPhase::positionPrefetch) {
                BESS_WARN("[WindowDrop] Ignoring XdndSelection response "
                          "without an active XdndPosition request");
                return;
            }

            if (selection.property == None) {
                completePositionPrefetch(std::nullopt);
                return;
            }

            if (selection.target != activeSelectionTarget) {
                BESS_WARN("[WindowDrop] Rejecting selection data for target {} "
                          "while waiting for {}",
                          atomName(display, selection.target),
                          atomName(display, activeSelectionTarget));
                XDeleteProperty(display, selectionWindow(), selection.property);
                completePositionPrefetch(std::nullopt);
                return;
            }

            completePositionPrefetch(readSelectionProperty(selection.property));
        }

        bool hasValidPrefetchedData() const {
            if (!prefetchComplete || !prefetchedPayload ||
                prefetchedPayload->formatBits != 8 ||
                prefetchedPayload->data.empty()) {
                return false;
            }

            const bool requiresPaths =
                prefetchedTarget == atoms.textUriList ||
                prefetchedTarget == atoms.xSpecialGnomeCopiedFiles;
            return !requiresPaths || !prefetchedPayload->paths.empty();
        }

        void completePosition(bool accepted) const {
            sendStatus(accepted);
            if (!hasLastPosition) {
                return;
            }

            emit({
                .type = WindowDropEventType::position,
                .x = lastPositionX,
                .y = lastPositionY,
                .accepted = accepted,
            });
        }

        void completePositionPrefetch(
            std::optional<SelectionPayload> selectionPayload) {
            const auto requestedTarget = activeSelectionTarget;
            clearActiveSelectionRequest();

            prefetchedPayload.reset();
            prefetchedTarget = requestedTarget;
            if (selectionPayload && selectionPayload->bytesAfter == 0 &&
                selectionPayload->actualFormat == 8 &&
                selectionPayload->actualType != None) {
                auto payload = std::make_shared<Events::WindowDropPayload>();
                payload->requestedMimeType = atomName(display, requestedTarget);
                payload->mimeType =
                    atomName(display, selectionPayload->actualType);
                payload->data = std::move(selectionPayload->data);
                if (requestedTarget == atoms.textUriList ||
                    requestedTarget == atoms.xSpecialGnomeCopiedFiles) {
                    payload->paths = parseLocalPaths(payload->data);
                }
                payload->formatBits = selectionPayload->actualFormat;
                prefetchedPayload = std::move(payload);
            }
            prefetchComplete = true;

            const bool success = hasValidPrefetchedData();
            if (!success) {
                BESS_WARN("[WindowDrop] Rejected payload target={} type={} "
                          "format={} bytes={} trailing-bytes={}",
                          atomName(display, requestedTarget),
                          selectionPayload
                              ? atomName(display, selectionPayload->actualType)
                              : "None",
                          selectionPayload ? selectionPayload->actualFormat : 0,
                          prefetchedPayload
                              ? prefetchedPayload->data.size()
                              : (selectionPayload
                                     ? selectionPayload->data.size()
                                     : 0),
                          selectionPayload ? selectionPayload->bytesAfter : 0);
            }

            if (dragActive) {
                completePosition(success);
            }

            if (dropPending) {
                finishDrop(success);
            }
        }

        void finishDrop(bool accepted) {
            emit({
                .type = WindowDropEventType::drop,
                .payload = accepted ? prefetchedPayload : nullptr,
                .x = lastPositionX,
                .y = lastPositionY,
                .accepted = accepted,
            });

            sendFinished(accepted);
            resetDragState();
        }

        void clearActiveSelectionRequest() {
            if (display && selectionWindow() != None &&
                activeSelectionProperty != None) {
                XDeleteProperty(
                    display, selectionWindow(), activeSelectionProperty);
            }

            activeSelectionTarget = None;
            activeSelectionProperty = None;
            selectionRequestPhase = SelectionRequestPhase::none;
            awaitingSelection = false;
        }

        void handleSelectionTimeout() {
            BESS_WARN("[WindowDrop] Timed out waiting for XdndSelection "
                      "data requested during XdndPosition");
            clearActiveSelectionRequest();
            prefetchedPayload.reset();
            prefetchedTarget = None;
            prefetchComplete = true;

            if (dragActive) {
                completePosition(false);
            }

            if (dropPending) {
                finishDrop(false);
            }
        }

        std::vector<Atom>
        getOfferedTypes(const XClientMessageEvent &message) const {
            constexpr long moreThanThreeTypesMask = 1;
            const bool hasTypeList =
                (message.data.l[1] & moreThanThreeTypesMask) != 0;

            if (hasTypeList) {
                return readSourceTypeList(
                    static_cast<::Window>(message.data.l[0]));
            }

            std::vector<Atom> offeredTypes;
            offeredTypes.reserve(3);
            for (long i = 2; i <= 4; ++i) {
                const auto atom = static_cast<Atom>(message.data.l[i]);
                if (atom != None) {
                    offeredTypes.push_back(atom);
                }
            }

            return offeredTypes;
        }

        std::vector<Atom> readSourceTypeList(::Window source) const {
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long itemCount = 0;
            unsigned long bytesAfter = 0;
            unsigned char *data = nullptr;

            const int status = XGetWindowProperty(display,
                                                  source,
                                                  atoms.xdndTypeList,
                                                  0,
                                                  1024,
                                                  False,
                                                  XA_ATOM,
                                                  &actualType,
                                                  &actualFormat,
                                                  &itemCount,
                                                  &bytesAfter,
                                                  &data);

            std::vector<Atom> offeredTypes;
            if (status == Success && actualType == XA_ATOM &&
                actualFormat == 32 && data) {
                const auto *atomsData = reinterpret_cast<const Atom *>(data);
                offeredTypes.assign(atomsData, atomsData + itemCount);
            }

            if (data) {
                XFree(data);
            }

            return offeredTypes;
        }

        std::vector<Atom>
        prioritizeTargets(const std::vector<Atom> &offeredTypes) const {
            std::vector<Atom> prioritized;
            prioritized.reserve(offeredTypes.size());

            if (hasAtom(offeredTypes, atoms.xSpecialGnomeCopiedFiles)) {
                appendUniqueAtom(prioritized, atoms.xSpecialGnomeCopiedFiles);
            }

            if (hasAtom(offeredTypes, atoms.textUriList)) {
                appendUniqueAtom(prioritized, atoms.textUriList);
            }

            if (hasAtom(offeredTypes, atoms.utf8String)) {
                appendUniqueAtom(prioritized, atoms.utf8String);
            }

            if (hasAtom(offeredTypes, atoms.textPlainUtf8)) {
                appendUniqueAtom(prioritized, atoms.textPlainUtf8);
            }

            if (hasAtom(offeredTypes, atoms.textPlain)) {
                appendUniqueAtom(prioritized, atoms.textPlain);
            }

            // Only request data targets. Some file managers expose operation
            // atoms such as DELETE next to payload targets; converting those
            // can read unrelated selection data.
            return prioritized;
        }

        Atom nextSelectionProperty() {
            if (activeSelectionProperty != None) {
                XDeleteProperty(
                    display, selectionWindow(), activeSelectionProperty);
            }

            ++selectionRequestSerial;
            const auto propertyName = "BESS_XDND_SELECTION_" +
                                      std::to_string(selectionWindow()) + "_" +
                                      std::to_string(selectionRequestSerial);
            return internAtom(display, propertyName.c_str());
        }

        std::optional<SelectionPayload>
        readSelectionProperty(Atom property) const {
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long itemCount = 0;
            unsigned long bytesAfter = 0;
            unsigned char *data = nullptr;

            const int status = XGetWindowProperty(display,
                                                  selectionWindow(),
                                                  property,
                                                  0,
                                                  kMaxDropPayloadLongs,
                                                  True,
                                                  AnyPropertyType,
                                                  &actualType,
                                                  &actualFormat,
                                                  &itemCount,
                                                  &bytesAfter,
                                                  &data);

            if (status != Success || !data) {
                if (data) {
                    XFree(data);
                }
                return std::nullopt;
            }

            if (actualType == None) {
                XFree(data);
                return std::nullopt;
            }

            SelectionPayload payload{
                .actualType = actualType,
                .actualFormat = actualFormat,
                .bytesAfter = bytesAfter,
            };

            if (actualFormat == 8 && itemCount <= kMaxDropPayloadBytes) {
                payload.data.assign(reinterpret_cast<const char *>(data),
                                    reinterpret_cast<const char *>(data) +
                                        itemCount);
            }

            XFree(data);
            return payload;
        }

        void sendStatus(bool accepted) const {
            if (sourceWindow == None) {
                return;
            }

            XEvent reply{};
            reply.type = ClientMessage;
            reply.xclient.window = sourceWindow;
            reply.xclient.message_type = atoms.xdndStatus;
            reply.xclient.format = 32;
            reply.xclient.data.l[0] = targetWindow;
            // Bit 0 accepts the drop; bit 1 asks the source to keep sending
            // position updates. This is Chromium's XDND status behavior.
            reply.xclient.data.l[1] = accepted ? 3 : 0;
            reply.xclient.data.l[2] = 0;
            reply.xclient.data.l[3] = 0;
            reply.xclient.data.l[4] = accepted ? atoms.xdndActionCopy : None;

            XSendEvent(display, sourceWindow, False, NoEventMask, &reply);
            XFlush(display);
        }

        void sendFinished(bool accepted) const {
            sendFinishedTo(sourceWindow, accepted);
        }

        void sendFinishedTo(::Window destination, bool accepted) const {
            if (xdndVersion < 2) {
                return;
            }

            if (destination == None) {
                return;
            }

            XEvent reply{};
            reply.type = ClientMessage;
            reply.xclient.window = destination;
            reply.xclient.message_type = atoms.xdndFinished;
            reply.xclient.format = 32;
            reply.xclient.data.l[0] = targetWindow;
            reply.xclient.data.l[1] = accepted ? 1 : 0;
            reply.xclient.data.l[2] = accepted ? atoms.xdndActionCopy : None;

            XSendEvent(display, destination, False, NoEventMask, &reply);
            XFlush(display);
        }

        void emit(const WindowDropEvent &event) const {
            std::vector<Callback> snapshot;
            snapshot.reserve(callbacks.size());
            for (const auto &[_, callback] : callbacks) {
                snapshot.push_back(callback);
            }
            for (const auto &callback : snapshot) {
                callback(event);
            }
        }

        void resetDragState() {
            if (display && selectionWindow() != None &&
                activeSelectionProperty != None) {
                XDeleteProperty(
                    display, selectionWindow(), activeSelectionProperty);
            }

            sourceWindow = None;
            acceptedTarget = None;
            offeredTargets.clear();
            activeSelectionTarget = None;
            activeSelectionProperty = None;
            selectionRequestPhase = SelectionRequestPhase::none;
            prefetchedPayload.reset();
            prefetchedTarget = None;
            awaitingSelection = false;
            dragActive = false;
            dropPending = false;
            prefetchComplete = false;
            hasLastPosition = false;
            lastPositionX = 0;
            lastPositionY = 0;
            xdndVersion = 0;
            lastPositionTimestamp = CurrentTime;
        }

      private:
        Display *display = nullptr;
        ::Window targetWindow = None;
        ::Window proxyWindow = None;
        ::Window sourceWindow = None;
        XdndAtoms atoms;
        Atom acceptedTarget = None;
        std::vector<Atom> offeredTargets;
        Atom activeSelectionTarget = None;
        Atom activeSelectionProperty = None;
        SelectionRequestPhase selectionRequestPhase =
            SelectionRequestPhase::none;
        std::shared_ptr<Events::WindowDropPayload> prefetchedPayload;
        Atom prefetchedTarget = None;
        std::chrono::steady_clock::time_point selectionRequestDeadline{};
        long xdndVersion = 0;
        Time lastPositionTimestamp = CurrentTime;
        unsigned long selectionRequestSerial = 0;
        int lastPositionX = 0;
        int lastPositionY = 0;
        bool awaitingSelection = false;
        bool dragActive = false;
        bool dropPending = false;
        bool prefetchComplete = false;
        bool hasLastPosition = false;
        bool initialized = false;

        WindowDropHandler::SubscriptionId nextSubscriptionId = 1;
        std::unordered_map<WindowDropHandler::SubscriptionId, Callback>
            callbacks;
    };

    WindowDropHandler::WindowDropHandler(_XDisplay *display,
                                         X11WindowHandle targetWindow)
        : m_impl(std::make_unique<Impl>(display, targetWindow)) {
    }

    WindowDropHandler::~WindowDropHandler() = default;

    WindowDropHandler::WindowDropHandler(WindowDropHandler &&) noexcept =
        default;

    WindowDropHandler &
    WindowDropHandler::operator=(WindowDropHandler &&) noexcept = default;

    bool WindowDropHandler::isInitialized() const {
        return m_impl->isInitialized();
    }

    bool WindowDropHandler::poll() {
        return m_impl->poll();
    }

    WindowDropHandler::SubscriptionId
    WindowDropHandler::subscribe(Callback callback) {
        return m_impl->subscribe(std::move(callback));
    }

    void WindowDropHandler::unsubscribe(SubscriptionId subscriptionId) {
        m_impl->unsubscribe(subscriptionId);
    }

} // namespace Bess::Platform::X11
