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
    m_inputRouter.setVisibleViews({});
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

    if (options.urls.empty()) {
        return newTab("about:blank") != TabModel::kNoTab;
    }
    for (const std::string& url : options.urls) {
        if (newTab(url) == TabModel::kNoTab) {
            return false;
        }
    }
    return true;
}

int Browser::run(double timeoutInSec)
{
    return m_appLoop->start(timeoutInSec);
}

void Browser::quit()
{
    m_appLoop->stop();
}

TabModel::TabId Browser::newTab(const std::string& url)
{
    std::unique_ptr<View> view(new View(*m_compositor));
    bool isCreated = view->init(pageRect(), [this](View* view) {
        // A page in the background still draws now and then.
        if (view == activeView()) {
            composite();
        }
    });
    if (!isCreated) {
        return TabModel::kNoTab;
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

    m_views.push_back(TabView{ tab, std::move(view) });
    showActiveTab();
    return tab;
}

bool Browser::closeTab(TabModel::TabId tab)
{
    if (!m_tabs.closeTab(tab)) {
        return false;
    }
    std::unique_ptr<View> closedView;
    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (it->tab == tab) {
            closedView = std::move(it->view);
            m_views.erase(it);
            break;
        }
    }
    // Nothing refers to the view any more once the next tab is shown.
    showActiveTab();
    closedView.reset();

    if (m_tabs.tabs().empty()) {
        quit();
    }
    return true;
}

bool Browser::activateTab(TabModel::TabId tab)
{
    if (!m_tabs.activateTab(tab)) {
        return false;
    }
    showActiveTab();
    return true;
}

View* Browser::viewOf(TabModel::TabId tab)
{
    for (TabView& tabView : m_views) {
        if (tabView.tab == tab) {
            return tabView.view.get();
        }
    }
    return nullptr;
}

View* Browser::activeView()
{
    return viewOf(m_tabs.activeTab());
}

View::Rect Browser::pageRect() const
{
    View::Rect rect;
    rect.width = m_window->width();
    rect.height = m_window->height();
    return rect;
}

std::vector<View*> Browser::visibleViews()
{
    std::vector<View*> views;
    if (View* view = activeView()) {
        views.push_back(view);
    }
    return views;
}

void Browser::showActiveTab()
{
    TabModel::TabId active = m_tabs.activeTab();
    if (active != m_shownTab) {
        // Null when that tab was just closed.
        if (View* previous = viewOf(m_shownTab)) {
            previous->setPaused(true);
        }
        m_shownTab = active;
        if (View* view = viewOf(active)) {
            // The window may have changed size while the tab was in the
            // background.
            view->setRect(pageRect());
            view->setPaused(false);
        }
    }
    m_inputRouter.setVisibleViews(visibleViews());
    m_inputRouter.setFocusedView(activeView());
    composite();
}

void Browser::composite()
{
    m_compositor->composite(visibleViews(), m_window->height());
}

void Browser::activateNeighbourTab(int offset)
{
    const std::vector<TabModel::Tab>& tabs = m_tabs.tabs();
    int count = static_cast<int>(tabs.size());
    for (int i = 0; i < count; i++) {
        if (tabs[i].id == m_tabs.activeTab()) {
            activateTab(tabs[((i + offset) % count + count) % count].id);
            return;
        }
    }
}

bool Browser::handleCommandKey(LWE::KeyValue key, bool isPressed,
                               unsigned modifiers)
{
    if (!(modifiers & BrowserWindow::ControlModifier)) {
        return false;
    }
    enum {
        NoCommand,
        NewTab,
        CloseTab,
        NextTab,
        PreviousTab
    } command = NoCommand;
    bool isShiftHeld = modifiers & BrowserWindow::ShiftModifier;
    switch (static_cast<int>(key)) {
    case 't':
    case 'T':
        command = NewTab;
        break;
    case 'w':
    case 'W':
        command = CloseTab;
        break;
    case LWE::KeyValue::TabKey:
        command = isShiftHeld ? PreviousTab : NextTab;
        break;
    case LWE::KeyValue::PageDownKey:
        command = NextTab;
        break;
    case LWE::KeyValue::PageUpKey:
        command = PreviousTab;
        break;
    default:
        return false;
    }
    // The release of a command key is swallowed like its press.
    if (!isPressed) {
        return true;
    }
    switch (command) {
    case NewTab:
        newTab("about:blank");
        break;
    case CloseTab:
        closeTab(m_tabs.activeTab());
        break;
    case NextTab:
        activateNeighbourTab(1);
        break;
    case PreviousTab:
        activateNeighbourTab(-1);
        break;
    default:
        break;
    }
    return true;
}

void Browser::windowResized(unsigned, unsigned)
{
    // Tabs in the background catch up when they are shown.
    if (View* view = activeView()) {
        view->setRect(pageRect());
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
    m_inputRouter.pointerMoved(x, y);
}

void Browser::pointerButtonChanged(int x, int y, bool isPressed)
{
    m_inputRouter.pointerButtonChanged(x, y, isPressed);
}

void Browser::wheelScrolled(int x, int y, int delta)
{
    m_inputRouter.wheelScrolled(x, y, delta);
}

void Browser::keyChanged(LWE::KeyValue key, bool isPressed, unsigned modifiers)
{
    if (!handleCommandKey(key, isPressed, modifiers)) {
        m_inputRouter.keyChanged(key, isPressed);
    }
}

void Browser::textCommitted(const std::string& text)
{
    m_inputRouter.textCommitted(text);
}

} // namespace StarfishShell

#endif
