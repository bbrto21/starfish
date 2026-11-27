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

#include "browser/Browser.h"

#include "AppLoop.h"
#include "LWEWebView.h"

namespace StarfishShell {

Browser::Browser()
{
}

Browser::~Browser()
{
    // Views before the compositor they render with, the compositor before
    // the window it draws into.
    m_views.clear();
    m_compositor.reset();
    m_window.reset();
    if (m_isEngineInitialized) {
        LWE::LWE::Finalize();
    }
    if (m_appLoop) {
        m_appLoop->deinit();
    }
}

bool Browser::init(const Options& options)
{
    m_appLoop = AppLoop::create();
    m_appLoop->init();

    m_window.reset(new BrowserWindow(*this));
    if (!m_window->init("Starfish", options.width, options.height)) {
        return false;
    }
    m_compositor.reset(new Compositor());
    if (!m_compositor->init(m_window->nativeHandle())) {
        return false;
    }

    LWE::LWE::SetVersionPreference(true);
    LWE::LWE::Initialize(options.storageDir.c_str(),
                         LWE::InitializeOption::None);
    m_isEngineInitialized = true;

    return openTab(options.url);
}

int Browser::run(double timeoutInSec)
{
    return m_appLoop->start(timeoutInSec);
}

void Browser::quit()
{
    m_appLoop->stop();
}

View* Browser::activeView()
{
    for (TabView& tabView : m_views) {
        if (tabView.tab == m_tabs.activeTab()) {
            return tabView.view.get();
        }
    }
    return nullptr;
}

bool Browser::openTab(const std::string& url)
{
    std::unique_ptr<View> view(new View(*m_compositor));
    if (!view->init(pageRect(), [this](View*) { composite(); })) {
        return false;
    }

    TabModel::TabId tab = m_tabs.addTab(url);
    LWE::WebContainer* container = view->container();
    container->RegisterOnPageStartedHandler(
        [this, tab](LWE::WebContainer*, const std::string& url) {
            m_tabs.setURL(tab, url);
            m_tabs.setLoading(tab, true);
        });
    container->RegisterOnPageLoadedHandler(
        [this, tab](LWE::WebContainer* container, const std::string& url) {
            m_tabs.setURL(tab, url);
            m_tabs.setTitle(tab, container->GetTitle());
            m_tabs.setLoading(tab, false);
        });
    container->RegisterOnShowSoftwareKeyboardIfPossibleHandler(
        [this](LWE::WebContainer*) { m_window->setInputMethodActive(true); });
    container->RegisterOnHideSoftwareKeyboardIfPossibleHandler(
        [this](LWE::WebContainer*) { m_window->setInputMethodActive(false); });

    container->LoadURL(url);
    container->Focus();
    m_views.push_back(TabView{ tab, std::move(view) });
    return true;
}

View::Rect Browser::pageRect() const
{
    View::Rect rect;
    rect.width = m_window->width();
    rect.height = m_window->height();
    return rect;
}

void Browser::composite()
{
    std::vector<View*> views;
    if (View* view = activeView()) {
        views.push_back(view);
    }
    m_compositor->composite(views, m_window->height());
}

void Browser::windowResized(unsigned, unsigned)
{
    for (TabView& tabView : m_views) {
        tabView.view->setRect(pageRect());
    }
    composite();
}

void Browser::windowExposed()
{
    composite();
}

void Browser::windowCloseRequested()
{
    quit();
}

void Browser::pointerMoved(int x, int y)
{
    View* view = activeView();
    if (!view) {
        return;
    }
    // The held button lets the page follow a drag.
    view->container()->DispatchMouseMoveEvent(
        LWE::MouseButtonValue::NoButton,
        m_isPrimaryButtonDown ? LWE::MouseButtonsValue::LeftButtonDown
                              : LWE::MouseButtonsValue::NoButtonDown,
        x - view->rect().x, y - view->rect().y);
}

void Browser::pointerButtonChanged(int x, int y, bool isPressed)
{
    m_isPrimaryButtonDown = isPressed;
    View* view = activeView();
    if (!view) {
        return;
    }
    x -= view->rect().x;
    y -= view->rect().y;
    if (isPressed) {
        view->container()->DispatchMouseDownEvent(
            LWE::MouseButtonValue::LeftButton,
            LWE::MouseButtonsValue::LeftButtonDown, x, y);
    } else {
        view->container()->DispatchMouseUpEvent(
            LWE::MouseButtonValue::LeftButton,
            LWE::MouseButtonsValue::NoButtonDown, x, y);
    }
}

void Browser::wheelScrolled(int x, int y, int delta)
{
    if (View* view = activeView()) {
        view->container()->DispatchMouseWheelEvent(x - view->rect().x,
                                                   y - view->rect().y, delta);
    }
}

void Browser::keyChanged(LWE::KeyValue key, bool isPressed)
{
    View* view = activeView();
    if (!view) {
        return;
    }
    if (isPressed) {
        view->container()->DispatchKeyDownEvent(key);
        view->container()->DispatchKeyPressEvent(key);
    } else {
        view->container()->DispatchKeyUpEvent(key);
    }
}

void Browser::textCommitted(const std::string& text)
{
    if (View* view = activeView()) {
        // XIM reports a finished composition only, with no updates.
        view->container()->DispatchCompositionEndEvent(text);
    }
}

} // namespace StarfishShell

#endif
