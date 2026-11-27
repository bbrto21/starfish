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
class Browser : private BrowserWindow::Client {
public:
    struct Options {
        unsigned width = 1280;
        unsigned height = 720;
        // Where the engine keeps cookies, storage and caches.
        std::string storageDir;
        // The page of the first tab.
        std::string url = "about:blank";
    };

    Browser();
    ~Browser();

    // Initializes the engine and opens the window with its first tab.
    bool init(const Options& options);

    // Runs the main loop until quit(), or for `timeoutInSec` seconds when
    // that is not 0.
    int run(double timeoutInSec = 0);
    void quit();

    const TabModel& tabs() const
    {
        return m_tabs;
    }

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

    bool openTab(const std::string& url);
    // The part of the window pages are shown in.
    View::Rect pageRect() const;
    void composite();

    // BrowserWindow::Client
    void windowResized(unsigned width, unsigned height) override;
    void windowExposed() override;
    void windowCloseRequested() override;
    void pointerMoved(int x, int y) override;
    void pointerButtonChanged(int x, int y, bool isPressed) override;
    void wheelScrolled(int x, int y, int delta) override;
    void keyChanged(LWE::KeyValue key, bool isPressed) override;
    void textCommitted(const std::string& text) override;

    std::unique_ptr<AppLoop> m_appLoop;
    std::unique_ptr<BrowserWindow> m_window;
    std::unique_ptr<Compositor> m_compositor;
    TabModel m_tabs;
    std::vector<TabView> m_views;
    bool m_isEngineInitialized = false;
    bool m_isPrimaryButtonDown = false;
};

} // namespace StarfishShell

#endif
