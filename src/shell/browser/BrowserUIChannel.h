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

#ifndef __StarfishShellBrowserUIChannel__
#define __StarfishShellBrowserUIChannel__

#include "browser/TabModel.h"

#include <functional>
#include <memory>
#include <string>

namespace LWE {
class WebContainer;
}

namespace StarfishShell {

// The only way the shell and the browser UI page (browser/ui) talk to
// each other, and the one place the messages are defined. browser/ui/ui.js
// is the other end and must match.
//
// UI to shell, as functions of the JavaScript object `shell` that take one
// string:
//   newTab("")  closeTab(id)  activateTab(id)  navigate(text)
//   goBack("")  goForward("")  reload("")
// Shell to UI, as functions of the page's `browserUI` object:
//   tabsChanged(tabs, activeId)  tabUpdated(tab)  focusAddress()
// where a tab is
//   { id, url, title, isLoading, canGoBack, canGoForward }.
class BrowserUIChannel {
public:
    // What the UI asks for. Never called from inside a call into the UI
    // page: a command is delivered on a later turn of the main loop, so
    // it may freely create and destroy views.
    class Client {
    public:
        virtual ~Client() = default;
        virtual void uiRequestedNewTab() = 0;
        virtual void uiRequestedCloseTab(TabModel::TabId tab) = 0;
        virtual void uiRequestedActivateTab(TabModel::TabId tab) = 0;
        // `text` is what the user typed, not necessarily a URL.
        virtual void uiRequestedNavigate(const std::string& text) = 0;
        virtual void uiRequestedGoBack() = 0;
        virtual void uiRequestedGoForward() = 0;
        virtual void uiRequestedReload() = 0;
    };

    // The client must outlive the channel.
    explicit BrowserUIChannel(Client& client);
    ~BrowserUIChannel();

    // Makes the commands available to the page of `container`. Call
    // before the page loads.
    void connect(LWE::WebContainer* container);

    // Messages sent before the page is ready are dropped; send the whole
    // state once it is.
    void setReady(bool isReady)
    {
        m_isReady = isReady;
    }

    void sendTabsChanged(const TabModel& tabs);
    void sendTabUpdated(const TabModel::Tab& tab);
    void sendFocusAddress();

private:
    void addCommand(const char* name,
                    const std::function<void(const std::string&)>& handler);
    void evaluate(const std::string& script);

    Client& m_client;
    LWE::WebContainer* m_container = nullptr;
    bool m_isReady = false;
    // Cleared on destruction; a command still waiting for its turn sees
    // it and does nothing.
    std::shared_ptr<bool> m_isAlive;
};

} // namespace StarfishShell

#endif
