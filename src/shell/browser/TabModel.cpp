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

#include "browser/TabModel.h"

#include <cassert>

namespace StarfishShell {

size_t TabModel::indexOf(TabId id) const
{
    for (size_t i = 0; i < m_tabs.size(); i++) {
        if (m_tabs[i].id == id) {
            return i;
        }
    }
    return m_tabs.size();
}

bool TabModel::hasTab(TabId id) const
{
    return indexOf(id) != m_tabs.size();
}

const TabModel::Tab& TabModel::tab(TabId id) const
{
    size_t index = indexOf(id);
    assert(index != m_tabs.size());
    return m_tabs[index];
}

TabModel::TabId TabModel::addTab(const std::string& url)
{
    Tab tab;
    tab.id = ++m_lastTabId;
    tab.url = url;
    m_tabs.push_back(tab);
    m_activeTab = tab.id;
    notifyTabsChanged();
    return tab.id;
}

bool TabModel::closeTab(TabId id)
{
    size_t index = indexOf(id);
    if (index == m_tabs.size()) {
        return false;
    }
    m_tabs.erase(m_tabs.begin() + index);
    if (m_activeTab == id) {
        if (m_tabs.empty()) {
            m_activeTab = kNoTab;
        } else {
            m_activeTab = m_tabs[index < m_tabs.size() ? index : index - 1].id;
        }
    }
    notifyTabsChanged();
    return true;
}

bool TabModel::activateTab(TabId id)
{
    if (!hasTab(id)) {
        return false;
    }
    if (m_activeTab != id) {
        m_activeTab = id;
        notifyTabsChanged();
    }
    return true;
}

void TabModel::setURL(TabId id, const std::string& url)
{
    size_t index = indexOf(id);
    if (index == m_tabs.size() || m_tabs[index].url == url) {
        return;
    }
    m_tabs[index].url = url;
    notifyTabUpdated(m_tabs[index]);
}

void TabModel::setTitle(TabId id, const std::string& title)
{
    size_t index = indexOf(id);
    if (index == m_tabs.size() || m_tabs[index].title == title) {
        return;
    }
    m_tabs[index].title = title;
    notifyTabUpdated(m_tabs[index]);
}

void TabModel::setLoading(TabId id, bool isLoading)
{
    size_t index = indexOf(id);
    if (index == m_tabs.size() || m_tabs[index].isLoading == isLoading) {
        return;
    }
    m_tabs[index].isLoading = isLoading;
    notifyTabUpdated(m_tabs[index]);
}

void TabModel::setHistoryState(TabId id, bool canGoBack, bool canGoForward)
{
    size_t index = indexOf(id);
    if (index == m_tabs.size() ||
        (m_tabs[index].canGoBack == canGoBack &&
         m_tabs[index].canGoForward == canGoForward)) {
        return;
    }
    m_tabs[index].canGoBack = canGoBack;
    m_tabs[index].canGoForward = canGoForward;
    notifyTabUpdated(m_tabs[index]);
}

void TabModel::notifyTabsChanged()
{
    if (m_observer) {
        m_observer->tabsChanged();
    }
}

void TabModel::notifyTabUpdated(const Tab& tab)
{
    if (m_observer) {
        m_observer->tabUpdated(tab);
    }
}

} // namespace StarfishShell

#endif
