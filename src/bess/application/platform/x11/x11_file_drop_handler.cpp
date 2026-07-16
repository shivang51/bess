#include "platform/x11/x11_file_drop_handler.h"

#include "common/logger.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Bess::Platform::X11 {
    namespace {
        constexpr long kXdndVersion = 5;
        constexpr auto kSelectionNotifyTimeout = std::chrono::milliseconds(750);

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
            std::string rawData;
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

            if (rest.empty()) {
                return std::nullopt;
            }

            return std::string(rest);
        }

        std::vector<std::string> parseFileList(const std::string &rawData) {
            std::vector<std::string> files;
            std::stringstream stream(rawData);
            std::string line;

            while (std::getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.empty() || line.front() == '#') {
                    continue;
                }

                // Nautilus/Thunar may include the operation name before URIs
                // for x-special/gnome-copied-files.
                if (line == "copy" || line == "move" || line == "cut") {
                    continue;
                }

                auto path = fileUriToPath(line);
                if (path) {
                    files.emplace_back(std::move(*path));
                    continue;
                }

                if (line.front() == '/') {
                    files.emplace_back(std::move(line));
                }
            }

            return files;
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

        std::string atomListNames(Display *display,
                                  const std::vector<Atom> &atoms) {
            if (atoms.empty()) {
                return "[]";
            }

            std::string result = "[";
            for (std::size_t i = 0; i < atoms.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                result += atomName(display, atoms[i]);
            }
            result += "]";
            return result;
        }

        std::string windowSummary(Display *display, ::Window window) {
            if (!display || window == None) {
                return "None";
            }

            std::string result = std::to_string(window);

            XClassHint classHint{};
            if (XGetClassHint(display, window, &classHint)) {
                if (classHint.res_name || classHint.res_class) {
                    result += " class=";
                    result += classHint.res_name ? classHint.res_name : "?";
                    result += "/";
                    result += classHint.res_class ? classHint.res_class : "?";
                }

                if (classHint.res_name) {
                    XFree(classHint.res_name);
                }
                if (classHint.res_class) {
                    XFree(classHint.res_class);
                }
            }

            return result;
        }
    } // namespace

    const char *toString(FileDropEventType type) {
        switch (type) {
        case FileDropEventType::enter:
            return "enter";
        case FileDropEventType::position:
            return "position";
        case FileDropEventType::leave:
            return "leave";
        case FileDropEventType::drop:
            return "drop";
        case FileDropEventType::selectionData:
            return "selection-data";
        case FileDropEventType::finished:
            return "finished";
        }

        return "unknown";
    }

    struct FileDropHandler::Impl {
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
                BESS_ERROR("[FileDragDrop] Failed to open a dedicated X11 "
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
                BESS_ERROR("[FileDragDrop] Failed to create the native XDND "
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

            BESS_DEBUG("[FileDragDrop] XDND proxy target={} proxy={}",
                       windowSummary(display, targetWindow),
                       proxyWindow);
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

        FileDropHandler::SubscriptionId subscribe(Callback callback) {
            if (!callback) {
                return 0;
            }

            const auto subscriptionId = nextSubscriptionId++;
            callbacks.emplace(subscriptionId, std::move(callback));
            return subscriptionId;
        }

        void unsubscribe(FileDropHandler::SubscriptionId subscriptionId) {
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
                    .type = FileDropEventType::enter,
                });
                return;
            }

            const auto offeredTypes = getOfferedTypes(message);
            offeredTargets = prioritizeTargets(offeredTypes);
            acceptedTarget =
                offeredTargets.empty() ? None : offeredTargets.front();

            BESS_DEBUG("[FileDragDrop] Xdnd offered targets: {}",
                       atomListNames(display, offeredTypes));
            BESS_DEBUG("[FileDragDrop] Xdnd prioritized targets: {}",
                       atomListNames(display, offeredTargets));
            BESS_DEBUG("[FileDragDrop] accepted Xdnd target: {}",
                       atomName(display, acceptedTarget));
            BESS_DEBUG(
                "[FileDragDrop] XdndEnter source={} selection-owner={} "
                "target={} source-proxy={} target-proxy={}",
                windowSummary(display, sourceWindow),
                windowSummary(display,
                              XGetSelectionOwner(display, atoms.xdndSelection)),
                windowSummary(display, targetWindow),
                readWindowAtom(sourceWindow, atoms.xdndProxy),
                readWindowAtom(targetWindow, atoms.xdndProxy));

            emit({
                .type = FileDropEventType::enter,
                .accepted = acceptedTarget != None,
            });
        }

        void handlePosition(const XClientMessageEvent &message) {
            if (!dragActive) {
                BESS_DEBUG("[FileDragDrop] Ignoring XdndPosition with no "
                           "active drag");
                return;
            }

            const auto eventSource = static_cast<::Window>(message.data.l[0]);
            if (sourceWindow != None && eventSource != sourceWindow) {
                BESS_DEBUG(
                    "[FileDragDrop] Ignoring XdndPosition from source {} "
                    "while tracking source {}",
                    eventSource,
                    sourceWindow);
                return;
            }

            sourceWindow = eventSource;
            const bool accepted =
                xdndVersion <= kXdndVersion && acceptedTarget != None;

            if (xdndVersion >= 1) {
                lastPositionTimestamp = static_cast<Time>(message.data.l[3]);
            }

            const long packedPosition = message.data.l[2];
            lastPositionX = packedPositionX(packedPosition);
            lastPositionY = packedPositionY(packedPosition);
            hasLastPosition = true;

            BESS_DEBUG("[FileDragDrop] XdndPosition action={} timestamp={}",
                       atomName(display, static_cast<Atom>(message.data.l[4])),
                       lastPositionTimestamp);

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
                BESS_WARN("[FileDragDrop] Cannot prefetch XdndSelection: "
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
                .type = FileDropEventType::leave,
            });
            resetDragState();
        }

        void handleDrop(const XClientMessageEvent &message) {
            const auto eventSource = static_cast<::Window>(message.data.l[0]);
            if (!dragActive) {
                BESS_DEBUG("[FileDragDrop] Rejecting XdndDrop from source {} "
                           "with no active drag",
                           eventSource);
                sendFinishedTo(eventSource, false);
                return;
            }

            if (sourceWindow != None && eventSource != sourceWindow) {
                BESS_DEBUG("[FileDragDrop] Rejecting XdndDrop from source {} "
                           "while tracking source {}",
                           eventSource,
                           sourceWindow);
                sendFinishedTo(eventSource, false);
                return;
            }

            sourceWindow = eventSource;
            const bool accepted = hasValidPrefetchedData();

            dragActive = false;
            dropPending = true;
            activeDropTimestamp = xdndVersion >= 1
                                      ? static_cast<Time>(message.data.l[2])
                                      : CurrentTime;

            BESS_DEBUG("[FileDragDrop] XdndDrop timestamp={} "
                       "prefetch-complete={} prefetched-files={}",
                       activeDropTimestamp,
                       prefetchComplete,
                       prefetchedFiles.size());

            if (awaitingSelection) {
                BESS_DEBUG("[FileDragDrop] XdndDrop is waiting for the active "
                           "XdndPosition selection request");
                return;
            }

            if (!prefetchComplete) {
                BESS_WARN("[FileDragDrop] Rejecting XdndDrop because no "
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
            BESS_DEBUG("[FileDragDrop] XdndPosition selection request "
                       "target={} property={} timestamp={}",
                       atomName(display, activeSelectionTarget),
                       atomName(display, activeSelectionProperty),
                       timestamp);

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
                BESS_WARN("[FileDragDrop] Ignoring XdndSelection response "
                          "without an active XdndPosition request");
                return;
            }

            if (selection.property == None) {
                completePositionPrefetch(std::nullopt);
                return;
            }

            if (selection.target != activeSelectionTarget) {
                BESS_WARN(
                    "[FileDragDrop] Rejecting selection data for target {} "
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
            return prefetchComplete && prefetchedPayload.has_value() &&
                   !prefetchedFiles.empty();
        }

        void completePosition(bool accepted) const {
            sendStatus(accepted);
            if (!hasLastPosition) {
                return;
            }

            emit({
                .type = FileDropEventType::position,
                .x = lastPositionX,
                .y = lastPositionY,
                .accepted = accepted,
            });
        }

        void completePositionPrefetch(std::optional<SelectionPayload> payload) {
            const auto requestedTarget = activeSelectionTarget;
            clearActiveSelectionRequest();

            prefetchedPayload = std::move(payload);
            prefetchedFiles.clear();
            if (prefetchedPayload) {
                prefetchedFiles = parseFileList(prefetchedPayload->rawData);
            }
            prefetchComplete = true;

            const bool success = hasValidPrefetchedData();
            BESS_DEBUG("[FileDragDrop] XdndPosition selection received "
                       "target={} actual-type={} format={} raw-bytes={} "
                       "files={} accepted={}",
                       atomName(display, requestedTarget),
                       prefetchedPayload
                           ? atomName(display, prefetchedPayload->actualType)
                           : "None",
                       prefetchedPayload ? prefetchedPayload->actualFormat : 0,
                       prefetchedPayload ? prefetchedPayload->rawData.size()
                                         : 0,
                       prefetchedFiles.size(),
                       success);

            if (!success) {
                BESS_WARN("[FileDragDrop] Rejected prefetched {} payload "
                          "because it did not contain file URIs or absolute "
                          "paths",
                          atomName(display, requestedTarget));
                emitSelectionData(false);
            }

            if (dragActive) {
                completePosition(success);
            }

            if (dropPending) {
                finishDrop(success);
            }
        }

        void emitSelectionData(bool accepted) {
            emit({
                .type = FileDropEventType::selectionData,
                .files = prefetchedFiles,
                .rawData = prefetchedPayload ? prefetchedPayload->rawData
                                             : std::string{},
                .requestedType = atomName(display, acceptedTarget),
                .actualType =
                    prefetchedPayload
                        ? atomName(display, prefetchedPayload->actualType)
                        : std::string{},
                .actualFormat =
                    prefetchedPayload ? prefetchedPayload->actualFormat : 0,
                .accepted = accepted,
            });
            selectionDataEmitted = true;
        }

        void finishDrop(bool accepted) {
            emit({
                .type = FileDropEventType::drop,
                .accepted = accepted,
            });

            if (!selectionDataEmitted || accepted) {
                emitSelectionData(accepted);
            }

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
            BESS_WARN("[FileDragDrop] Timed out waiting for XdndSelection "
                      "data requested during XdndPosition");
            clearActiveSelectionRequest();
            prefetchedPayload.reset();
            prefetchedFiles.clear();
            prefetchComplete = true;

            if (!selectionDataEmitted) {
                emitSelectionData(false);
            }

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

        std::string readWindowAtom(::Window window, Atom property) const {
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long itemCount = 0;
            unsigned long bytesAfter = 0;
            unsigned char *data = nullptr;

            const int status = XGetWindowProperty(display,
                                                  window,
                                                  property,
                                                  0,
                                                  1,
                                                  False,
                                                  XA_WINDOW,
                                                  &actualType,
                                                  &actualFormat,
                                                  &itemCount,
                                                  &bytesAfter,
                                                  &data);

            std::string result = "None";
            if (status == Success && actualFormat == 32 && itemCount > 0 &&
                data) {
                const auto *windowData =
                    reinterpret_cast<const ::Window *>(data);
                result = windowSummary(display, *windowData);
            }

            if (data) {
                XFree(data);
            }

            return result;
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
                                                  1024 * 1024,
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

            if (actualFormat == 8) {
                payload.rawData.assign(reinterpret_cast<const char *>(data),
                                       reinterpret_cast<const char *>(data) +
                                           itemCount);
            } else if (actualFormat == 32) {
                const auto *longData = reinterpret_cast<const long *>(data);
                for (unsigned long i = 0; i < itemCount; ++i) {
                    payload.rawData += std::to_string(longData[i]);
                    payload.rawData.push_back('\n');
                }
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

            emit({
                .type = FileDropEventType::finished,
                .accepted = accepted,
            });
        }

        void emit(const FileDropEvent &event) const {
            for (const auto &[_, callback] : callbacks) {
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
            prefetchedFiles.clear();
            awaitingSelection = false;
            dragActive = false;
            dropPending = false;
            prefetchComplete = false;
            selectionDataEmitted = false;
            hasLastPosition = false;
            lastPositionX = 0;
            lastPositionY = 0;
            xdndVersion = 0;
            lastPositionTimestamp = CurrentTime;
            activeDropTimestamp = CurrentTime;
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
        std::optional<SelectionPayload> prefetchedPayload;
        std::vector<std::string> prefetchedFiles;
        std::chrono::steady_clock::time_point selectionRequestDeadline{};
        long xdndVersion = 0;
        Time lastPositionTimestamp = CurrentTime;
        Time activeDropTimestamp = CurrentTime;
        unsigned long selectionRequestSerial = 0;
        int lastPositionX = 0;
        int lastPositionY = 0;
        bool awaitingSelection = false;
        bool dragActive = false;
        bool dropPending = false;
        bool prefetchComplete = false;
        bool selectionDataEmitted = false;
        bool hasLastPosition = false;
        bool initialized = false;

        FileDropHandler::SubscriptionId nextSubscriptionId = 1;
        std::unordered_map<FileDropHandler::SubscriptionId, Callback> callbacks;
    };

    FileDropHandler::FileDropHandler(_XDisplay *display,
                                     X11WindowHandle targetWindow)
        : m_impl(std::make_unique<Impl>(display, targetWindow)) {
    }

    FileDropHandler::~FileDropHandler() = default;

    FileDropHandler::FileDropHandler(FileDropHandler &&) noexcept = default;

    FileDropHandler &
    FileDropHandler::operator=(FileDropHandler &&) noexcept = default;

    bool FileDropHandler::isInitialized() const {
        return m_impl->isInitialized();
    }

    bool FileDropHandler::poll() {
        return m_impl->poll();
    }

    FileDropHandler::SubscriptionId
    FileDropHandler::subscribe(Callback callback) {
        return m_impl->subscribe(std::move(callback));
    }

    void FileDropHandler::unsubscribe(SubscriptionId subscriptionId) {
        m_impl->unsubscribe(subscriptionId);
    }

} // namespace Bess::Platform::X11
