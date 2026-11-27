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

#include "browser/TabModel.h"

namespace StarfishShell {

namespace {

    struct CountingObserver : TabModel::Observer {
        void tabsChanged() override
        {
            tabsChangedCount++;
        }

        void tabUpdated(const TabModel::Tab& tab) override
        {
            tabUpdatedCount++;
            lastUpdated = tab;
        }

        int tabsChangedCount = 0;
        int tabUpdatedCount = 0;
        TabModel::Tab lastUpdated;
    };

} // namespace

TEST(BrowserTabModelTest, StartsEmpty)
{
    TabModel model;
    EXPECT_TRUE(model.tabs().empty());
    EXPECT_EQ(TabModel::kNoTab, model.activeTab());
    EXPECT_FALSE(model.hasTab(1));
}

TEST(BrowserTabModelTest, AddedTabBecomesActive)
{
    TabModel model;
    TabModel::TabId first = model.addTab("https://a.test/");
    TabModel::TabId second = model.addTab("https://b.test/");

    EXPECT_NE(TabModel::kNoTab, first);
    EXPECT_NE(first, second);
    ASSERT_EQ(2u, model.tabs().size());
    EXPECT_EQ(first, model.tabs()[0].id);
    EXPECT_EQ(second, model.tabs()[1].id);
    EXPECT_EQ(second, model.activeTab());
    EXPECT_EQ("https://a.test/", model.tab(first).url);
    EXPECT_FALSE(model.tab(first).isLoading);
}

TEST(BrowserTabModelTest, ActivateTab)
{
    TabModel model;
    TabModel::TabId first = model.addTab("a");
    TabModel::TabId second = model.addTab("b");

    EXPECT_TRUE(model.activateTab(first));
    EXPECT_EQ(first, model.activeTab());
    EXPECT_FALSE(model.activateTab(second + 100));
    EXPECT_EQ(first, model.activeTab());
}

TEST(BrowserTabModelTest, ClosingActiveTabActivatesNeighbour)
{
    TabModel model;
    TabModel::TabId first = model.addTab("a");
    TabModel::TabId second = model.addTab("b");
    TabModel::TabId third = model.addTab("c");

    // A tab in the middle hands over to the one that takes its place.
    model.activateTab(second);
    EXPECT_TRUE(model.closeTab(second));
    EXPECT_EQ(third, model.activeTab());

    // The last tab hands over to the one before it.
    EXPECT_TRUE(model.closeTab(third));
    EXPECT_EQ(first, model.activeTab());

    EXPECT_TRUE(model.closeTab(first));
    EXPECT_EQ(TabModel::kNoTab, model.activeTab());
    EXPECT_TRUE(model.tabs().empty());
}

TEST(BrowserTabModelTest, ClosingInactiveTabKeepsActiveTab)
{
    TabModel model;
    TabModel::TabId first = model.addTab("a");
    TabModel::TabId second = model.addTab("b");

    EXPECT_TRUE(model.closeTab(first));
    EXPECT_EQ(second, model.activeTab());
    EXPECT_FALSE(model.closeTab(first));
    EXPECT_FALSE(model.hasTab(first));
}

TEST(BrowserTabModelTest, IdsAreNotReused)
{
    TabModel model;
    TabModel::TabId first = model.addTab("a");
    model.closeTab(first);
    EXPECT_NE(first, model.addTab("b"));
}

TEST(BrowserTabModelTest, ObserverSeesListAndActiveChanges)
{
    TabModel model;
    CountingObserver observer;
    model.setObserver(&observer);

    TabModel::TabId first = model.addTab("a");
    TabModel::TabId second = model.addTab("b");
    EXPECT_EQ(2, observer.tabsChangedCount);

    model.activateTab(first);
    EXPECT_EQ(3, observer.tabsChangedCount);
    // Activating the active tab changes nothing.
    model.activateTab(first);
    EXPECT_EQ(3, observer.tabsChangedCount);

    model.closeTab(second);
    EXPECT_EQ(4, observer.tabsChangedCount);
    model.closeTab(second);
    EXPECT_EQ(4, observer.tabsChangedCount);
    EXPECT_EQ(0, observer.tabUpdatedCount);
}

TEST(BrowserTabModelTest, ObserverSeesTabUpdatesOnlyOnChange)
{
    TabModel model;
    CountingObserver observer;
    model.setObserver(&observer);
    TabModel::TabId id = model.addTab("a");

    model.setTitle(id, "Title");
    EXPECT_EQ(1, observer.tabUpdatedCount);
    EXPECT_EQ(id, observer.lastUpdated.id);
    EXPECT_EQ("Title", observer.lastUpdated.title);
    model.setTitle(id, "Title");
    EXPECT_EQ(1, observer.tabUpdatedCount);

    model.setURL(id, "b");
    EXPECT_EQ("b", model.tab(id).url);
    model.setLoading(id, true);
    EXPECT_TRUE(model.tab(id).isLoading);
    EXPECT_EQ(3, observer.tabUpdatedCount);

    model.setHistoryState(id, true, false);
    EXPECT_TRUE(model.tab(id).canGoBack);
    EXPECT_FALSE(model.tab(id).canGoForward);
    EXPECT_EQ(4, observer.tabUpdatedCount);
    model.setHistoryState(id, true, false);
    EXPECT_EQ(4, observer.tabUpdatedCount);

    // A page event for a tab that is already gone is dropped.
    model.closeTab(id);
    model.setTitle(id, "Late");
    model.setURL(id, "late");
    model.setLoading(id, false);
    model.setHistoryState(id, false, true);
    EXPECT_EQ(4, observer.tabUpdatedCount);
}

} // namespace StarfishShell

#endif
