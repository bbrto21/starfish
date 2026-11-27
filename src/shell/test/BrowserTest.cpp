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
        options.hasUI = m_hasUI;
        options.urls.push_back(page(html));
        ASSERT_TRUE(m_browser.init(options));
        waitForActiveTab();
    }

    static std::string page(const std::string& html)
    {
        return "data:text/html," + html;
    }

    static std::string colorPage(const std::string& color)
    {
        return page("<body style='margin:0;background:%23" + color +
                    "'></body>");
    }

    // Until the active tab has loaded and drawn.
    void waitForActiveTab()
    {
        ASSERT_TRUE(runUntil([this] {
            const TabModel& tabs = m_browser.tabs();
            return !tabs.tab(tabs.activeTab()).isLoading &&
                   m_browser.activeView()->hasFrame();
        }));
    }

    TabModel::TabId newTab(const std::string& url)
    {
        TabModel::TabId tab = m_browser.newTab(url);
        EXPECT_NE(TabModel::kNoTab, tab);
        waitForActiveTab();
        return tab;
    }

    bool showsColor(unsigned long color)
    {
        return runUntil(
            [this, color] { return windowPixel(200, 150) == color; }, 5);
    }

    std::string evaluateIn(TabModel::TabId tab, const std::string& script)
    {
        return m_browser.viewOf(tab)->container()->EvaluateJavaScript(script);
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

    void sendKey(int type, KeySym keysym, unsigned state = 0)
    {
        XEvent event = {};
        event.xkey.type = type;
        event.xkey.keycode = XKeysymToKeycode(m_display, keysym);
        event.xkey.state = state;
        event.xkey.same_screen = True;
        send(event, type == KeyPress ? KeyPressMask : KeyReleaseMask);
    }

    void command(KeySym keysym, unsigned state = ControlMask)
    {
        sendKey(KeyPress, keysym, state);
        sendKey(KeyRelease, keysym, state);
    }

    Display* m_display = nullptr;
    Browser m_browser;
    // Most tests are about pages, which then fill the window.
    bool m_hasUI = false;
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

TEST_F(BrowserTest, HiddenWindowStillRenders)
{
    Browser::Options options;
    options.width = 400;
    options.height = 300;
    options.isWindowVisible = false;
    options.hasUI = false;
    options.storageDir = "/tmp/starfish_storage/";
    options.urls.push_back(colorPage("00ff00"));
    ASSERT_TRUE(m_browser.init(options));
    waitForActiveTab();

    XWindowAttributes attributes;
    XGetWindowAttributes(m_display, m_browser.window().nativeHandle(),
                         &attributes);
    EXPECT_LT(attributes.x + attributes.width, 0);
    EXPECT_TRUE(attributes.override_redirect);
    EXPECT_EQ("400x300", evaluate("innerWidth + 'x' + innerHeight"));
}

TEST_F(BrowserTest, DevicePixelRatioScalesPage)
{
    Browser::Options options;
    options.width = 400;
    options.height = 300;
    options.devicePixelRatio = 2;
    options.hasUI = false;
    options.storageDir = "/tmp/starfish_storage/";
    options.urls.push_back(
        page("<body style='margin:0;background:%2300ff00'>"
             "<div style='width:100px;height:100px;background:%23ff0000'>"
             "</div><script>var seen = '';"
             "onmousedown = function(e) {"
             "  seen = e.clientX + ',' + e.clientY; };"
             "</script></body>"));
    ASSERT_TRUE(m_browser.init(options));
    waitForActiveTab();

    // The window keeps its size in pixels; the page is half as large in
    // CSS pixels and drawn twice as big.
    EXPECT_EQ("200x150 2", evaluate("innerWidth + 'x' + innerHeight + ' ' + "
                                    "devicePixelRatio"));
    EXPECT_TRUE(runUntil([this] {
        return windowPixel(190, 190) == 0xff0000 &&
               windowPixel(210, 210) == 0x00ff00;
    }));

    sendButton(ButtonPress, Button1, 120, 80);
    sendButton(ButtonRelease, Button1, 120, 80);
    EXPECT_TRUE(runUntil([this] { return evaluate("seen") == "60,40"; }))
        << evaluate("seen");
}

TEST_F(BrowserTest, TabKeepsItsFocusedElementInTheBackground)
{
    const char* form =
        "<body><input id='field'><script>"
        "field.focus();</script></body>";
    open(form);
    TabModel::TabId first = m_browser.tabs().activeTab();
    sendKey(KeyPress, XK_a);
    sendKey(KeyRelease, XK_a);
    ASSERT_TRUE(runUntil([this] { return evaluate("field.value") == "a"; }));

    TabModel::TabId second = newTab(page(form));
    sendKey(KeyPress, XK_x);
    sendKey(KeyRelease, XK_x);
    ASSERT_TRUE(runUntil([this] { return evaluate("field.value") == "x"; }));

    // Back in the first tab the caret is still in its field.
    m_browser.activateTab(first);
    sendKey(KeyPress, XK_b);
    sendKey(KeyRelease, XK_b);
    EXPECT_TRUE(runUntil([this] { return evaluate("field.value") == "ab"; }))
        << evaluate("field.value");
    EXPECT_EQ("x", evaluateIn(second, "field.value"));
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

TEST_F(BrowserTest, ShowsTheActiveTab)
{
    open("<body style='margin:0;background:%23ff0000'></body>");
    TabModel::TabId red = m_browser.tabs().activeTab();
    TabModel::TabId green = newTab(colorPage("00ff00"));
    TabModel::TabId blue = newTab(colorPage("0000ff"));

    EXPECT_EQ(blue, m_browser.tabs().activeTab());
    EXPECT_TRUE(showsColor(0x0000ff));

    EXPECT_TRUE(m_browser.activateTab(red));
    EXPECT_TRUE(showsColor(0xff0000));
    EXPECT_TRUE(m_browser.activateTab(green));
    EXPECT_TRUE(showsColor(0x00ff00));
    EXPECT_FALSE(m_browser.activateTab(blue + 100));

    // Closing the shown tab brings up its neighbour.
    EXPECT_TRUE(m_browser.closeTab(green));
    EXPECT_EQ(blue, m_browser.tabs().activeTab());
    EXPECT_TRUE(showsColor(0x0000ff));
    EXPECT_EQ(nullptr, m_browser.viewOf(green));
    EXPECT_FALSE(m_browser.closeTab(green));
}

TEST_F(BrowserTest, KeyboardCommandsDriveTabs)
{
    open(
        "<body style='margin:0;background:%23ff0000'><script>"
        "var keys = '';"
        "onkeydown = function(e) { keys += e.key; };"
        "</script></body>");
    TabModel::TabId first = m_browser.tabs().activeTab();

    command(XK_t);
    ASSERT_TRUE(
        runUntil([this] { return m_browser.tabs().tabs().size() == 2; }));
    TabModel::TabId second = m_browser.tabs().activeTab();
    EXPECT_NE(first, second);
    waitForActiveTab();
    EXPECT_TRUE(showsColor(0xffffff));

    command(XK_Tab);
    EXPECT_TRUE(
        runUntil([&] { return m_browser.tabs().activeTab() == first; }));
    EXPECT_TRUE(showsColor(0xff0000));
    // Past the last tab comes the first, and the other way round.
    command(XK_ISO_Left_Tab, ControlMask | ShiftMask);
    EXPECT_TRUE(
        runUntil([&] { return m_browser.tabs().activeTab() == second; }));
    command(XK_Next);
    EXPECT_TRUE(
        runUntil([&] { return m_browser.tabs().activeTab() == first; }));
    command(XK_Prior);
    EXPECT_TRUE(
        runUntil([&] { return m_browser.tabs().activeTab() == second; }));

    command(XK_w);
    ASSERT_TRUE(
        runUntil([this] { return m_browser.tabs().tabs().size() == 1; }));
    EXPECT_EQ(first, m_browser.tabs().activeTab());
    EXPECT_TRUE(showsColor(0xff0000));

    // The page saw the Control key but none of the command keys.
    sendKey(KeyPress, XK_x);
    sendKey(KeyRelease, XK_x);
    EXPECT_TRUE(runUntil([this] { return evaluate("keys") == "x"; }))
        << evaluate("keys");
}

TEST_F(BrowserTest, InputGoesToTheActiveTabOnly)
{
    const char* recorder =
        "<body style='margin:0'><script>"
        "var seen = '';"
        "onmousedown = function() { seen += 'click '; };"
        "onkeydown = function(e) { seen += e.key; };"
        "</script></body>";
    open(recorder);
    TabModel::TabId first = m_browser.tabs().activeTab();
    TabModel::TabId second = newTab(page(recorder));

    sendButton(ButtonPress, Button1, 200, 200);
    sendButton(ButtonRelease, Button1, 200, 200);
    sendKey(KeyPress, XK_a);
    sendKey(KeyRelease, XK_a);
    EXPECT_TRUE(runUntil([&] {
        return evaluateIn(second, "seen") == "click a";
    })) << evaluateIn(second, "seen");
    EXPECT_EQ("", evaluateIn(first, "seen"));

    m_browser.activateTab(first);
    sendKey(KeyPress, XK_b);
    sendKey(KeyRelease, XK_b);
    EXPECT_TRUE(runUntil([&] { return evaluateIn(first, "seen") == "b"; }))
        << evaluateIn(first, "seen");
    EXPECT_EQ("click a", evaluateIn(second, "seen"));
}

TEST_F(BrowserTest, BackgroundTabStopsAnimating)
{
    open(
        "<body><script>"
        "var ticks = 0;"
        "(function tick() { ticks++; requestAnimationFrame(tick); })();"
        "</script></body>");
    TabModel::TabId animated = m_browser.tabs().activeTab();
    ASSERT_TRUE(runUntil(
        [&] { return std::stoi(evaluateIn(animated, "String(ticks)")) > 5; }));

    newTab(colorPage("00ff00"));
    // A frame already under way may still finish.
    runUntil([] { return false; }, 0.3);
    int ticksWhenHidden = std::stoi(evaluateIn(animated, "String(ticks)"));
    runUntil([] { return false; }, 0.5);
    EXPECT_EQ(ticksWhenHidden,
              std::stoi(evaluateIn(animated, "String(ticks)")));

    m_browser.activateTab(animated);
    EXPECT_TRUE(runUntil([&] {
        return std::stoi(evaluateIn(animated, "String(ticks)")) >
               ticksWhenHidden + 5;
    }));
}

TEST_F(BrowserTest, BackgroundTabCatchesUpWithWindowSize)
{
    open("<body style='margin:0;background:%23ff0000'></body>");
    TabModel::TabId red = m_browser.tabs().activeTab();
    newTab(colorPage("00ff00"));

    XResizeWindow(m_display, m_browser.window().nativeHandle(), 640, 480);
    XFlush(m_display);
    ASSERT_TRUE(runUntil([this] {
        return evaluate("innerWidth + 'x' + innerHeight") == "640x480";
    }));

    m_browser.activateTab(red);
    EXPECT_TRUE(runUntil([this] {
        return evaluate("innerWidth + 'x' + innerHeight") == "640x480" &&
               windowPixel(0, 0) == 0xff0000 &&
               windowPixel(639, 479) == 0xff0000;
    })) << evaluate("innerWidth + 'x' + innerHeight");
}

TEST_F(BrowserTest, TabsOpenAndCloseRepeatedly)
{
    open("<body style='margin:0;background:%23ff0000'></body>");
    TabModel::TabId first = m_browser.tabs().activeTab();

    for (int i = 0; i < 20; i++) {
        TabModel::TabId tab = m_browser.newTab(colorPage("00ff00"));
        ASSERT_NE(TabModel::kNoTab, tab);
        // Every other tab is closed before it has loaded or drawn.
        if (i % 2) {
            waitForActiveTab();
        }
        ASSERT_TRUE(m_browser.closeTab(tab));
    }

    EXPECT_EQ(first, m_browser.tabs().activeTab());
    EXPECT_EQ(1u, m_browser.tabs().tabs().size());
    EXPECT_TRUE(showsColor(0xff0000));
    // The surviving page still draws new frames.
    evaluate("document.body.style.background = '#0000ff'");
    EXPECT_TRUE(showsColor(0x0000ff));
}

TEST_F(BrowserTest, ClosingTheLastTabEndsTheLoop)
{
    open("<body></body>");

    command(XK_w);
    auto start = std::chrono::steady_clock::now();
    m_browser.run(10);
    EXPECT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::seconds(5));
    EXPECT_TRUE(m_browser.tabs().tabs().empty());
}

// The same browser with its tab strip and toolbar, which sit in the top
// 76 pixels of the window.
class BrowserUITest : public BrowserTest {
protected:
    enum { kUIHeight = 76 };

    BrowserUITest()
    {
        m_hasUI = true;
    }

    void openWithUI(const std::string& url)
    {
        Browser::Options options;
        options.width = 800;
        options.height = 400;
        options.storageDir = "/tmp/starfish_storage/";
        options.urls.push_back(url);
        ASSERT_TRUE(m_browser.init(options));
        ASSERT_TRUE(m_browser.uiView());
        waitForActiveTab();
        ASSERT_TRUE(waitForUI("tabCount()", "1"));
    }

    static std::string titledPage(const std::string& title,
                                  const std::string& color)
    {
        return page("<title>" + title +
                    "</title><body style='margin:0;background:%23" + color +
                    "'><script>var keys = '';"
                    "onkeydown = function(e) { keys += e.key; };"
                    "</script></body>");
    }

    std::string evaluateUI(const std::string& script)
    {
        // Helpers for the expressions the tests ask the UI page.
        const char* helpers =
            "function tabCount() {"
            "  return String(document.querySelectorAll('.tab').length); }"
            "function activeTitle() {"
            "  var e = document.querySelector('.tab.active .title');"
            "  return e ? e.textContent : ''; }"
            "function center(selector) {"
            "  var r = document.querySelector(selector)"
            "      .getBoundingClientRect();"
            "  return Math.round(r.left + r.width / 2) + ',' +"
            "      Math.round(r.top + r.height / 2); }";
        return m_browser.uiView()->container()->EvaluateJavaScript(
            std::string(helpers) + script);
    }

    bool waitForUI(const std::string& script, const std::string& expected)
    {
        return runUntil([&] { return evaluateUI(script) == expected; }, 5);
    }

    // A real click in the middle of an element of the UI page.
    void clickUI(const std::string& selector)
    {
        std::string position = evaluateUI("center(\"" + selector + "\")");
        size_t comma = position.find(',');
        ASSERT_NE(std::string::npos, comma) << position;
        int x = std::stoi(position.substr(0, comma));
        int y = std::stoi(position.substr(comma + 1));
        sendButton(ButtonPress, Button1, x, y);
        sendButton(ButtonRelease, Button1, x, y);
    }

    void press(KeySym keysym, unsigned state = 0)
    {
        sendKey(KeyPress, keysym, state);
        sendKey(KeyRelease, keysym, state);
    }

    const TabModel::Tab& activeTab()
    {
        return m_browser.tabs().tab(m_browser.tabs().activeTab());
    }
};

TEST_F(BrowserUITest, ShowsStripAbovePage)
{
    std::string url = titledPage("First", "00ff00");
    openWithUI(url);

    EXPECT_TRUE(waitForUI("activeTitle()", "First"));
    // The engine reports the URL percent-encoded.
    EXPECT_EQ(0, activeTab().url.find("data:text/html,"));
    EXPECT_EQ(activeTab().url, evaluateUI("address.value"));
    EXPECT_EQ("800x76", evaluateUI("innerWidth + 'x' + innerHeight"));
    EXPECT_EQ("800x324", evaluate("innerWidth + 'x' + innerHeight"));

    EXPECT_TRUE(runUntil([this] {
        // The strip's background, then the page from the strip's lower
        // edge to the bottom of the window.
        return windowPixel(790, 3) == 0xdee1e6 &&
               windowPixel(400, kUIHeight + 1) == 0x00ff00 &&
               windowPixel(400, 399) == 0x00ff00;
    }));
}

TEST_F(BrowserUITest, PageSeesPositionsRelativeToItself)
{
    openWithUI(
        page("<body style='margin:0'><script>"
             "var seen = '';"
             "onmousedown = function(e) {"
             "  seen = e.clientX + ',' + e.clientY; };"
             "</script></body>"));

    sendButton(ButtonPress, Button1, 120, kUIHeight + 80);
    sendButton(ButtonRelease, Button1, 120, kUIHeight + 80);
    EXPECT_TRUE(runUntil([this] { return evaluate("seen") == "120,80"; }))
        << evaluate("seen");
}

TEST_F(BrowserUITest, NewTabButtonOpensTabAndFocusesAddress)
{
    openWithUI(titledPage("First", "00ff00"));
    TabModel::TabId first = m_browser.tabs().activeTab();

    clickUI("#new-tab");
    ASSERT_TRUE(waitForUI("tabCount()", "2"));
    EXPECT_NE(first, m_browser.tabs().activeTab());
    EXPECT_TRUE(waitForUI("document.activeElement.id", "address"));
    EXPECT_EQ("", evaluateUI("address.value"));

    // Typing goes to the address bar, not to a page.
    press(XK_a);
    press(XK_b);
    EXPECT_TRUE(waitForUI("address.value", "ab"))
        << evaluateUI("address.value");
    EXPECT_EQ("", evaluateIn(first, "keys"));
}

TEST_F(BrowserUITest, AddressBarNavigatesActiveTab)
{
    openWithUI(titledPage("First", "00ff00"));

    press(XK_l, ControlMask);
    ASSERT_TRUE(waitForUI("document.activeElement.id", "address"));
    std::string target = titledPage("Second", "0000ff");
    evaluateUI("address.value = \"" + target + "\"");
    press(XK_Return);

    EXPECT_TRUE(runUntil([&] { return activeTab().title == "Second"; }));
    EXPECT_TRUE(waitForUI("activeTitle()", "Second"));
    EXPECT_TRUE(waitForUI("address.value", activeTab().url));
    EXPECT_TRUE(runUntil([this] { return windowPixel(400, 300) == 0x0000ff; }));

    // The page has the keyboard again.
    press(XK_x);
    EXPECT_TRUE(runUntil([this] { return evaluate("keys") == "x"; }))
        << evaluate("keys");
}

TEST_F(BrowserUITest, TypedHostNameGetsAScheme)
{
    openWithUI(titledPage("First", "00ff00"));

    press(XK_l, ControlMask);
    ASSERT_TRUE(waitForUI("document.activeElement.id", "address"));
    evaluateUI("address.value = 'host.invalid/path'");
    press(XK_Return);

    EXPECT_TRUE(runUntil([&] {
        return activeTab().url == "http://host.invalid/path";
    })) << activeTab().url;
}

TEST_F(BrowserUITest, TabStripSwitchesAndClosesTabs)
{
    openWithUI(titledPage("First", "ff0000"));
    TabModel::TabId first = m_browser.tabs().activeTab();
    TabModel::TabId second = newTab(titledPage("Second", "0000ff"));
    ASSERT_TRUE(waitForUI("tabCount()", "2"));
    ASSERT_TRUE(waitForUI("activeTitle()", "Second"));

    clickUI(".tab[data-tab='" + std::to_string(first) + "'] .title");
    EXPECT_TRUE(
        runUntil([&] { return m_browser.tabs().activeTab() == first; }));
    EXPECT_TRUE(waitForUI("activeTitle()", "First"));
    EXPECT_TRUE(runUntil([this] { return windowPixel(400, 300) == 0xff0000; }));

    clickUI(".tab[data-tab='" + std::to_string(first) + "'] .close");
    EXPECT_TRUE(runUntil([&] { return !m_browser.tabs().hasTab(first); }));
    EXPECT_EQ(second, m_browser.tabs().activeTab());
    EXPECT_TRUE(waitForUI("tabCount()", "1"));
    EXPECT_TRUE(waitForUI("activeTitle()", "Second"));
    EXPECT_TRUE(runUntil([this] { return windowPixel(400, 300) == 0x0000ff; }));
}

TEST_F(BrowserUITest, BackAndForwardButtons)
{
    openWithUI(titledPage("First", "ff0000"));
    EXPECT_EQ("true", evaluateUI("String(back.disabled)"));

    m_browser.activeView()->container()->LoadURL(
        titledPage("Second", "0000ff"));
    ASSERT_TRUE(runUntil([&] { return activeTab().title == "Second"; }));
    ASSERT_TRUE(waitForUI("String(back.disabled)", "false"));
    EXPECT_EQ("true", evaluateUI("String(forward.disabled)"));

    clickUI("#back");
    EXPECT_TRUE(runUntil([&] { return activeTab().title == "First"; }))
        << activeTab().title;
    EXPECT_EQ("First", evaluate("document.title"));
    EXPECT_TRUE(runUntil([this] { return windowPixel(400, 300) == 0xff0000; }));
    EXPECT_TRUE(waitForUI("String(forward.disabled)", "false"));
    EXPECT_TRUE(waitForUI("String(back.disabled)", "true"));

    clickUI("#forward");
    EXPECT_TRUE(runUntil([&] { return activeTab().title == "Second"; }))
        << activeTab().title;
    EXPECT_TRUE(runUntil([this] { return windowPixel(400, 300) == 0x0000ff; }));
}

TEST_F(BrowserUITest, ReloadButton)
{
    openWithUI(titledPage("First", "ff0000"));
    evaluate("document.title = 'Changed'; window.marker = 1");

    clickUI("#reload");
    EXPECT_TRUE(runUntil([this] {
        return evaluate("String(window.marker) + document.title") ==
               "undefinedFirst";
    })) << evaluate("String(window.marker) + document.title");
}

TEST_F(BrowserUITest, StripFollowsWindowWidth)
{
    openWithUI(titledPage("First", "00ff00"));

    XResizeWindow(m_display, m_browser.window().nativeHandle(), 1000, 500);
    XFlush(m_display);
    EXPECT_TRUE(waitForUI("innerWidth + 'x' + innerHeight", "1000x76"));
    EXPECT_TRUE(runUntil([this] {
        return evaluate("innerWidth + 'x' + innerHeight") == "1000x424" &&
               windowPixel(990, 3) == 0xdee1e6 &&
               windowPixel(990, 490) == 0x00ff00;
    }));
}

TEST_F(BrowserUITest, StripScalesWithDevicePixelRatio)
{
    Browser::Options options;
    options.width = 800;
    options.height = 400;
    options.devicePixelRatio = 2;
    options.storageDir = "/tmp/starfish_storage/";
    options.urls.push_back(titledPage("First", "00ff00"));
    ASSERT_TRUE(m_browser.init(options));
    waitForActiveTab();

    EXPECT_TRUE(waitForUI("innerWidth + 'x' + innerHeight", "400x76"));
    EXPECT_EQ("400x124", evaluate("innerWidth + 'x' + innerHeight"));
    EXPECT_TRUE(runUntil([this] {
        return windowPixel(790, 6) == 0xdee1e6 &&
               windowPixel(400, 2 * kUIHeight + 1) == 0x00ff00;
    }));
}

TEST_F(BrowserUITest, StripDoesNotNavigateAway)
{
    openWithUI(titledPage("First", "00ff00"));

    evaluateUI("location.href = 'about:blank'");
    runUntil([] { return false; }, 0.5);
    EXPECT_EQ("1", evaluateUI("tabCount()"));
}

TEST_F(BrowserUITest, PageCannotReachShellCommands)
{
    openWithUI(titledPage("First", "00ff00"));
    EXPECT_EQ("undefined", evaluate("typeof shell"));
    EXPECT_EQ("object", evaluateUI("typeof shell"));
}

} // namespace StarfishShell

#endif
