/*
 * Copyright (c) 2026-present Samsung Electronics Co., Ltd
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "ShellConfig.h"

#if defined(SHELL_ENABLE_BROWSER)

#include "gtest/gtest.h"

#include "LWEWebView.h"
#include "browser/Browser.h"

#include <chrono>
#include <functional>
#include <string>

// After gtest: Xlib defines None, Bool and Status as macros.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

namespace StarfishShell {

// Drives the browser the way a user does: through a second X connection
// that sends input to the browser window and reads its pixels back.
class BrowserTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_display = XOpenDisplay(nullptr);
        ASSERT_TRUE(m_display);
    }

    void TearDown() override
    {
        XCloseDisplay(m_display);
    }

    void open(const std::string& html, unsigned width = 400,
              unsigned height = 300)
    {
        Browser::Options options;
        options.width = width;
        options.height = height;
        options.storageDir = "/tmp/starfish_storage/";
        options.url = "data:text/html," + html;
        ASSERT_TRUE(m_browser.init(options));
        ASSERT_TRUE(runUntil([this] {
            const TabModel& tabs = m_browser.tabs();
            return !tabs.tab(tabs.activeTab()).isLoading &&
                   m_browser.activeView()->hasFrame();
        }));
    }

    bool runUntil(const std::function<bool()>& isDone, double seconds = 10)
    {
        auto end = std::chrono::steady_clock::now() +
                   std::chrono::duration<double>(seconds);
        while (!isDone()) {
            if (std::chrono::steady_clock::now() >= end) {
                return false;
            }
            m_browser.run(0.05);
        }
        return true;
    }

    std::string evaluate(const std::string& script)
    {
        return m_browser.activeView()->container()->EvaluateJavaScript(script);
    }

    // 0xRRGGBB of a window pixel, as the X server shows it.
    unsigned long windowPixel(int x, int y)
    {
        XImage* image = XGetImage(m_display, m_browser.window().nativeHandle(),
                                  x, y, 1, 1, AllPlanes, ZPixmap);
        if (!image) {
            return ~0ul;
        }
        unsigned long pixel = XGetPixel(image, 0, 0) & 0xffffff;
        XDestroyImage(image);
        return pixel;
    }

    void send(XEvent& event, long mask)
    {
        ::Window window = m_browser.window().nativeHandle();
        event.xany.display = m_display;
        event.xany.window = window;
        XSendEvent(m_display, window, False, mask, &event);
        XFlush(m_display);
    }

    void sendButton(int type, unsigned button, int x, int y)
    {
        XEvent event = {};
        event.xbutton.type = type;
        event.xbutton.button = button;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.same_screen = True;
        send(event, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask);
    }

    void sendKey(int type, KeySym keysym)
    {
        XEvent event = {};
        event.xkey.type = type;
        event.xkey.keycode = XKeysymToKeycode(m_display, keysym);
        event.xkey.same_screen = True;
        send(event, type == KeyPress ? KeyPressMask : KeyReleaseMask);
    }

    Display* m_display = nullptr;
    Browser m_browser;
};

TEST_F(BrowserTest, ShowsPageInWindow)
{
    open(
        "<title>Green</title>"
        "<body style='margin:0;background:%2300ff00'></body>");

    const TabModel& tabs = m_browser.tabs();
    ASSERT_EQ(1u, tabs.tabs().size());
    EXPECT_EQ("Green", tabs.tab(tabs.activeTab()).title);
    EXPECT_EQ(0, tabs.tab(tabs.activeTab()).url.find("data:text/html,"));

    EXPECT_TRUE(runUntil([this] {
        return windowPixel(0, 0) == 0x00ff00 &&
               windowPixel(399, 299) == 0x00ff00;
    }));
}

TEST_F(BrowserTest, PageIsDrawnUpright)
{
    // Red at the top, blue at the bottom.
    open(
        "<body style='margin:0;background:%230000ff'>"
        "<div style='height:150px;background:%23ff0000'></div></body>");

    EXPECT_TRUE(runUntil([this] {
        return windowPixel(200, 10) == 0xff0000 &&
               windowPixel(200, 290) == 0x0000ff;
    }));
}

TEST_F(BrowserTest, PointerReachesPage)
{
    open(
        "<body style='margin:0'><script>"
        "var seen = '';"
        "onmousedown = function(e) {"
        "  seen += 'down@' + e.clientX + ',' + e.clientY + ' '; };"
        "onmouseup = function(e) { seen += 'up'; };"
        "onwheel = function(e) { seen += ' wheel' + (e.deltaY > 0); };"
        "</script></body>");

    sendButton(ButtonPress, Button1, 120, 80);
    sendButton(ButtonRelease, Button1, 120, 80);
    EXPECT_TRUE(runUntil([this] {
        return evaluate("seen") == "down@120,80 up";
    })) << evaluate("seen");

    sendButton(ButtonPress, Button5, 120, 80);
    EXPECT_TRUE(runUntil([this] {
        return evaluate("seen") == "down@120,80 up wheeltrue";
    })) << evaluate("seen");
}

TEST_F(BrowserTest, KeysReachFocusedPage)
{
    open(
        "<body><input id='field'><script>"
        "var keys = '';"
        "onkeydown = function(e) { keys += e.key + ' '; };"
        "field.focus();"
        "</script></body>");

    sendKey(KeyPress, XK_a);
    sendKey(KeyRelease, XK_a);
    sendKey(KeyPress, XK_b);
    sendKey(KeyRelease, XK_b);
    sendKey(KeyPress, XK_Left);
    sendKey(KeyRelease, XK_Left);
    EXPECT_TRUE(runUntil([this] {
        return evaluate("keys") == "a b ArrowLeft ";
    })) << evaluate("keys");
    EXPECT_EQ("ab", evaluate("field.value"));
}

TEST_F(BrowserTest, PageFollowsWindowSize)
{
    open("<body style='margin:0;background:%2300ff00'></body>");

    XResizeWindow(m_display, m_browser.window().nativeHandle(), 640, 480);
    XFlush(m_display);

    EXPECT_TRUE(runUntil([this] {
        return evaluate("innerWidth + 'x' + innerHeight") == "640x480";
    })) << evaluate("innerWidth + 'x' + innerHeight");
    EXPECT_EQ(640u, m_browser.window().width());
    // The grown part of the window shows the page too.
    EXPECT_TRUE(runUntil([this] {
        return windowPixel(0, 0) == 0x00ff00 &&
               windowPixel(639, 479) == 0x00ff00;
    }));
}

TEST_F(BrowserTest, CloseRequestEndsTheLoop)
{
    open("<body></body>");

    XEvent event = {};
    event.xclient.type = ClientMessage;
    event.xclient.message_type = XInternAtom(m_display, "WM_PROTOCOLS", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
    send(event, NoEventMask);

    auto start = std::chrono::steady_clock::now();
    // Without the close request this would run for the full ten seconds.
    m_browser.run(10);
    EXPECT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::seconds(5));
}

} // namespace StarfishShell

#endif
