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

#ifndef __StarfishShellBrowserTabModel__
#define __StarfishShellBrowserTabModel__

#include <string>
#include <vector>

namespace StarfishShell {

// The tabs of one browser window: their order, which one is active, and
// what each one shows. Every other part of the browser reads tab state
// from here instead of keeping its own copy. Knows nothing about windows,
// views or rendering.
class TabModel {
public:
    typedef unsigned TabId;
    // Never the id of a tab; activeTab() returns it when there is no tab.
    enum : TabId { kNoTab = 0 };

    struct Tab {
        TabId id = kNoTab;
        std::string url;
        std::string title;
        bool isLoading = false;
    };

    class Observer {
    public:
        virtual ~Observer() = default;
        // A tab was added or closed, or another tab became active.
        virtual void tabsChanged() = 0;
        // The URL, title or loading state of `tab` changed.
        virtual void tabUpdated(const Tab& tab) = 0;
    };

    // The observer must outlive the model, or be reset to nullptr first.
    void setObserver(Observer* observer)
    {
        m_observer = observer;
    }

    // In tab strip order.
    const std::vector<Tab>& tabs() const
    {
        return m_tabs;
    }

    TabId activeTab() const
    {
        return m_activeTab;
    }

    bool hasTab(TabId id) const;
    // `id` must be a tab of this model.
    const Tab& tab(TabId id) const;

    // Appends a tab and makes it the active one.
    TabId addTab(const std::string& url);
    // Closing the active tab activates the tab that takes its place, or
    // the one before it when it was the last. Returns false for an
    // unknown id.
    bool closeTab(TabId id);
    // Returns false for an unknown id.
    bool activateTab(TabId id);

    // No-ops for an unknown id: a page event may arrive for a tab that
    // was just closed.
    void setURL(TabId id, const std::string& url);
    void setTitle(TabId id, const std::string& title);
    void setLoading(TabId id, bool isLoading);

private:
    // m_tabs.size() when there is no such tab.
    size_t indexOf(TabId id) const;
    void notifyTabsChanged();
    void notifyTabUpdated(const Tab& tab);

    std::vector<Tab> m_tabs;
    TabId m_activeTab = kNoTab;
    TabId m_lastTabId = kNoTab;
    Observer* m_observer = nullptr;
};

} // namespace StarfishShell

#endif
