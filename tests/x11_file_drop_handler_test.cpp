#include "platform/x11/x11_file_drop_handler.h"

#include "gtest/gtest.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {
    using Bess::Platform::X11::FileDropEvent;
    using Bess::Platform::X11::FileDropEventType;
    using Bess::Platform::X11::FileDropHandler;

    constexpr auto kEventTimeout = std::chrono::milliseconds(500);

    bool waitUntil(const std::function<bool()> &condition) {
        const auto deadline = std::chrono::steady_clock::now() + kEventTimeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return condition();
    }

    Atom atom(Display *display, const char *name) {
        return XInternAtom(display, name, False);
    }

    ::Window
    readWindowProperty(Display *display, ::Window window, Atom property) {
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

        ::Window result = None;
        if (status == Success && actualType == XA_WINDOW &&
            actualFormat == 32 && itemCount == 1 && data) {
            result = *reinterpret_cast<const ::Window *>(data);
        }
        if (data) {
            XFree(data);
        }
        return result;
    }

    void sendClientMessage(Display *display,
                           ::Window destination,
                           ::Window logicalTarget,
                           Atom messageType,
                           const std::array<long, 5> &data) {
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = display;
        event.xclient.window = logicalTarget;
        event.xclient.message_type = messageType;
        event.xclient.format = 32;
        std::copy(data.begin(), data.end(), event.xclient.data.l);
        ASSERT_NE(XSendEvent(display, destination, False, NoEventMask, &event),
                  0);
        XFlush(display);
    }

    bool takeEvent(Display *display,
                   int eventType,
                   const std::function<bool(const XEvent &)> &predicate,
                   XEvent &result) {
        while (XPending(display) > 0) {
            XEvent event{};
            XNextEvent(display, &event);
            if (event.type == eventType && predicate(event)) {
                result = event;
                return true;
            }
        }
        return false;
    }
} // namespace

TEST(X11FileDropHandlerTest, CachesDragSelectionBeforeDrop) {
    Display *targetDisplay = XOpenDisplay(nullptr);
    if (!targetDisplay) {
        GTEST_SKIP() << "No X11 display is available";
    }

    Display *sourceDisplay = XOpenDisplay(XDisplayString(targetDisplay));
    ASSERT_NE(sourceDisplay, nullptr);

    const ::Window targetWindow =
        XCreateSimpleWindow(targetDisplay,
                            DefaultRootWindow(targetDisplay),
                            0,
                            0,
                            100,
                            100,
                            0,
                            0,
                            0);
    const ::Window sourceWindow = XCreateSimpleWindow(
        sourceDisplay, DefaultRootWindow(sourceDisplay), 0, 0, 1, 1, 0, 0, 0);
    const ::Window clipboardWindow = XCreateSimpleWindow(
        sourceDisplay, DefaultRootWindow(sourceDisplay), 0, 0, 1, 1, 0, 0, 0);
    ASSERT_NE(targetWindow, None);
    ASSERT_NE(sourceWindow, None);
    ASSERT_NE(clipboardWindow, None);

    const Atom xdndAware = atom(sourceDisplay, "XdndAware");
    const Atom xdndProxy = atom(sourceDisplay, "XdndProxy");
    const Atom xdndEnter = atom(sourceDisplay, "XdndEnter");
    const Atom xdndPosition = atom(sourceDisplay, "XdndPosition");
    const Atom xdndStatus = atom(sourceDisplay, "XdndStatus");
    const Atom xdndDrop = atom(sourceDisplay, "XdndDrop");
    const Atom xdndFinished = atom(sourceDisplay, "XdndFinished");
    const Atom xdndSelection = atom(sourceDisplay, "XdndSelection");
    const Atom xdndTypeList = atom(sourceDisplay, "XdndTypeList");
    const Atom xdndActionCopy = atom(sourceDisplay, "XdndActionCopy");
    const Atom textUriList = atom(sourceDisplay, "text/uri-list");
    const Atom deleteTarget = atom(sourceDisplay, "DELETE");

    std::vector<FileDropEvent> receivedEvents;
    {
        FileDropHandler handler(targetDisplay, targetWindow);
        ASSERT_TRUE(handler.isInitialized());
        const auto subscription =
            handler.subscribe([&](const FileDropEvent &event) {
                receivedEvents.push_back(event);
            });

        XSync(targetDisplay, False);
        const ::Window proxyWindow =
            readWindowProperty(targetDisplay, targetWindow, xdndProxy);
        ASSERT_NE(proxyWindow, None);
        EXPECT_EQ(readWindowProperty(targetDisplay, proxyWindow, xdndProxy),
                  proxyWindow);

        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char *awareData = nullptr;
        ASSERT_EQ(XGetWindowProperty(targetDisplay,
                                     proxyWindow,
                                     xdndAware,
                                     0,
                                     1,
                                     False,
                                     XA_ATOM,
                                     &actualType,
                                     &actualFormat,
                                     &itemCount,
                                     &bytesAfter,
                                     &awareData),
                  Success);
        ASSERT_EQ(actualType, XA_ATOM);
        ASSERT_EQ(actualFormat, 32);
        ASSERT_EQ(itemCount, 1U);
        XFree(awareData);

        const std::array<Atom, 3> offeredTypes{
            textUriList,
            textUriList,
            deleteTarget,
        };
        XChangeProperty(
            sourceDisplay,
            sourceWindow,
            xdndTypeList,
            XA_ATOM,
            32,
            PropModeReplace,
            reinterpret_cast<const unsigned char *>(offeredTypes.data()),
            offeredTypes.size());
        XSetSelectionOwner(
            sourceDisplay, xdndSelection, sourceWindow, CurrentTime);
        XSync(sourceDisplay, False);
        ASSERT_EQ(XGetSelectionOwner(sourceDisplay, xdndSelection),
                  sourceWindow);

        sendClientMessage(
            sourceDisplay,
            proxyWindow,
            targetWindow,
            xdndEnter,
            {static_cast<long>(sourceWindow), (5L << 24) | 1L, 0, 0, 0});
        ASSERT_TRUE(waitUntil([&] {
            handler.poll();
            return std::ranges::any_of(receivedEvents, [](const auto &event) {
                return event.type == FileDropEventType::enter;
            });
        }));

        sendClientMessage(sourceDisplay,
                          proxyWindow,
                          targetWindow,
                          xdndPosition,
                          {static_cast<long>(sourceWindow),
                           0,
                           (100L << 16) | 200L,
                           CurrentTime,
                           static_cast<long>(xdndActionCopy)});

        XEvent selectionRequest{};
        ASSERT_TRUE(waitUntil([&] {
            handler.poll();
            return takeEvent(
                sourceDisplay,
                SelectionRequest,
                [&](const XEvent &event) {
                    return event.xselectionrequest.owner == sourceWindow &&
                           event.xselectionrequest.selection == xdndSelection;
                },
                selectionRequest);
        }));
        ASSERT_EQ(selectionRequest.xselectionrequest.target, textUriList);
        ASSERT_EQ(selectionRequest.xselectionrequest.requestor, proxyWindow);
        ASSERT_NE(selectionRequest.xselectionrequest.property, None);

        constexpr char draggedFile[] = "file:///tmp/dragged-image.png\r\n";
        XChangeProperty(sourceDisplay,
                        proxyWindow,
                        selectionRequest.xselectionrequest.property,
                        textUriList,
                        8,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char *>(draggedFile),
                        sizeof(draggedFile) - 1);

        XEvent selectionNotify{};
        selectionNotify.xselection.type = SelectionNotify;
        selectionNotify.xselection.display = sourceDisplay;
        selectionNotify.xselection.requestor = proxyWindow;
        selectionNotify.xselection.selection = xdndSelection;
        selectionNotify.xselection.target = textUriList;
        selectionNotify.xselection.property =
            selectionRequest.xselectionrequest.property;
        selectionNotify.xselection.time = CurrentTime;
        ASSERT_NE(XSendEvent(sourceDisplay,
                             proxyWindow,
                             False,
                             NoEventMask,
                             &selectionNotify),
                  0);
        XFlush(sourceDisplay);

        ASSERT_TRUE(waitUntil([&] {
            handler.poll();
            return std::ranges::any_of(receivedEvents, [](const auto &event) {
                return event.type == FileDropEventType::position &&
                       event.accepted;
            });
        }));

        XEvent statusEvent{};
        ASSERT_TRUE(waitUntil([&] {
            return takeEvent(
                sourceDisplay,
                ClientMessage,
                [&](const XEvent &event) {
                    return event.xclient.message_type == xdndStatus;
                },
                statusEvent);
        }));
        EXPECT_EQ(statusEvent.xclient.data.l[0], targetWindow);
        EXPECT_EQ(statusEvent.xclient.data.l[1] & 3L, 3L);

        // Replacing the XDND owner here models the observed XWayland fallback
        // to clipboard contents after button release. No conversion request
        // should occur after this point because the file list is already
        // cached.
        XSetSelectionOwner(
            sourceDisplay, xdndSelection, clipboardWindow, CurrentTime);
        XSync(sourceDisplay, False);
        ASSERT_EQ(XGetSelectionOwner(sourceDisplay, xdndSelection),
                  clipboardWindow);

        sendClientMessage(
            sourceDisplay,
            proxyWindow,
            targetWindow,
            xdndDrop,
            {static_cast<long>(sourceWindow), 0, CurrentTime, 0, 0});

        ASSERT_TRUE(waitUntil([&] {
            handler.poll();
            return std::ranges::any_of(receivedEvents, [](const auto &event) {
                return event.type == FileDropEventType::finished;
            });
        }));

        const auto selectionData =
            std::ranges::find_if(receivedEvents, [](const auto &event) {
                return event.type == FileDropEventType::selectionData;
            });
        ASSERT_NE(selectionData, receivedEvents.end());
        ASSERT_TRUE(selectionData->accepted);
        ASSERT_EQ(selectionData->files.size(), 1U);
        EXPECT_EQ(selectionData->files.front(), "/tmp/dragged-image.png");
        EXPECT_EQ(selectionData->rawData, draggedFile);

        bool sawPostDropSelectionRequest = false;
        bool sawFinished = false;
        ASSERT_TRUE(waitUntil([&] {
            XSync(sourceDisplay, False);
            while (XPending(sourceDisplay) > 0) {
                XEvent event{};
                XNextEvent(sourceDisplay, &event);
                sawPostDropSelectionRequest |= event.type == SelectionRequest;
                sawFinished |= event.type == ClientMessage &&
                               event.xclient.message_type == xdndFinished;
            }
            return sawFinished;
        }));
        EXPECT_FALSE(sawPostDropSelectionRequest);

        handler.unsubscribe(subscription);
    }

    XDestroyWindow(sourceDisplay, clipboardWindow);
    XDestroyWindow(sourceDisplay, sourceWindow);
    XDestroyWindow(targetDisplay, targetWindow);
    XCloseDisplay(sourceDisplay);
    XCloseDisplay(targetDisplay);
}
