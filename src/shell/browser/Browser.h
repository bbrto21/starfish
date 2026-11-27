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

#ifndef __StarfishShellBrowser__
#define __StarfishShellBrowser__

#include "browser/BrowserWindow.h"
#include "browser/Compositor.h"
#include "browser/InputRouter.h"
#include "browser/TabModel.h"
#include "browser/View.h"

#include <memory>
#include <string>
#include <vector>

namespace StarfishShell {

class AppLoop;

// The tabbed browser behind `Starfish browser`: one window whose pages
// are views composited by the shell. Ties the window, the compositor, the
// tab model and the views together, and routes input to the views.
//
// Keyboard commands: Control+T opens a tab, Control+W closes the active
// one, Control+Tab and Control+Shift+Tab (or Control+PageDown and
// Control+PageUp) move to the next and the previous tab.
class Browser : private BrowserWindow::Client {
public:
    struct Options {
        unsigned width = 1280;
        unsigned height = 720;
        // A hidden window still renders; test runs use it.
        bool isWindowVisible = true;
        // Where the engine keeps cookies, storage and caches.
        std::string storageDir;
        // One tab per URL, the last one active. A blank tab when empty.
        std::vector<std::string> urls;
    };

    Browser();
    ~Browser();

    // Initializes the engine and opens the window with its tabs.
    bool init(const Options& options);

    // Runs the main loop until quit(), or for `timeoutInSec` seconds when
    // that is not 0.
    int run(double timeoutInSec = 0);
    void quit();

    const TabModel& tabs() const
    {
        return m_tabs;
    }

    // Opens a tab showing `url` and makes it the active one. Returns
    // TabModel::kNoTab when the page view cannot be created.
    TabModel::TabId newTab(const std::string& url);
    // Closing the last tab ends run(). Returns false for an unknown id.
    bool closeTab(TabModel::TabId tab);
    // Returns false for an unknown id.
    bool activateTab(TabModel::TabId tab);

    // nullptr for an unknown id.
    View* viewOf(TabModel::TabId tab);
    // The view of the active tab.
    View* activeView();

    BrowserWindow& window()
    {
        return *m_window;
    }

private:
    struct TabView {
        TabModel::TabId tab;
        std::unique_ptr<View> view;
    };

    // The part of the window pages are shown in.
    View::Rect pageRect() const;
    // The views on screen, the bottom one first.
    std::vector<View*> visibleViews();
    // Puts the view of the active tab on screen, pauses the one it
    // replaces and moves the focus. Call after every change to m_tabs.
    void showActiveTab();
    void composite();
    // Moves `offset` tabs to the right of the active one, wrapping around.
    void activateNeighbourTab(int offset);
    // True when the key is one of the keyboard commands, which the page
    // then does not see.
    bool handleCommandKey(LWE::KeyValue key, bool isPressed,
                          unsigned modifiers);

    // BrowserWindow::Client
    void windowResized(unsigned width, unsigned height) override;
    void windowExposed() override;
    void windowCloseRequested() override;
    void pointerMoved(int x, int y) override;
    void pointerButtonChanged(int x, int y, bool isPressed) override;
    void wheelScrolled(int x, int y, int delta) override;
    void keyChanged(LWE::KeyValue key, bool isPressed,
                    unsigned modifiers) override;
    void textCommitted(const std::string& text) override;

    std::unique_ptr<AppLoop> m_appLoop;
    std::unique_ptr<BrowserWindow> m_window;
    std::unique_ptr<Compositor> m_compositor;
    TabModel m_tabs;
    std::vector<TabView> m_views;
    InputRouter m_inputRouter;
    // The tab whose view is on screen; follows m_tabs.activeTab().
    TabModel::TabId m_shownTab = TabModel::kNoTab;
    bool m_isEngineInitialized = false;
};

} // namespace StarfishShell

#endif
