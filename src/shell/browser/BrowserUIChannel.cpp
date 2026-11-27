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

#include "browser/BrowserUIChannel.h"

#include "LWEWebView.h"

#include <glib.h>

#include <cstdio>
#include <cstdlib>

namespace StarfishShell {

namespace {

    const char* const kShellObject = "shell";
    const char* const kUIObject = "browserUI";

    // A JavaScript string literal. Everything that could end the literal or
    // the script it is embedded in is written as an escape.
    std::string toJSString(const std::string& text)
    {
        std::string result = "\"";
        for (unsigned char c : text) {
            if (c < 0x20 || c == '"' || c == '\\' || c == '<' || c == '>' ||
                c == '&' || c == '\'') {
                char escape[8];
                snprintf(escape, sizeof(escape), "\\u%04x", c);
                result += escape;
            } else {
                result += static_cast<char>(c);
            }
        }
        // U+2028 and U+2029 end a line inside a string literal.
        const char* const lineSeparators[] = { "\xe2\x80\xa8", "\xe2\x80\xa9" };
        const char* const escapes[] = { "\\u2028", "\\u2029" };
        for (int i = 0; i < 2; i++) {
            size_t position = 0;
            while ((position = result.find(lineSeparators[i], position)) !=
                   std::string::npos) {
                result.replace(position, 3, escapes[i]);
            }
        }
        return result + "\"";
    }

    std::string toJSObject(const TabModel::Tab& tab)
    {
        return "{id:" + std::to_string(tab.id) + ",url:" + toJSString(tab.url) +
               ",title:" + toJSString(tab.title) +
               ",isLoading:" + (tab.isLoading ? "true" : "false") +
               ",canGoBack:" + (tab.canGoBack ? "true" : "false") +
               ",canGoForward:" + (tab.canGoForward ? "true" : "false") + "}";
    }

    TabModel::TabId toTabId(const std::string& text)
    {
        return static_cast<TabModel::TabId>(strtoul(text.c_str(), nullptr, 10));
    }

    struct PendingCommand {
        std::shared_ptr<bool> isAlive;
        std::function<void()> run;
    };

} // namespace

BrowserUIChannel::BrowserUIChannel(Client& client)
    : m_client(client)
    , m_isAlive(new bool(true))
{
}

BrowserUIChannel::~BrowserUIChannel()
{
    *m_isAlive = false;
}

void BrowserUIChannel::connect(LWE::WebContainer* container)
{
    m_container = container;
    Client* client = &m_client;
    addCommand("newTab",
               [client](const std::string&) { client->uiRequestedNewTab(); });
    addCommand("closeTab", [client](const std::string& id) {
        client->uiRequestedCloseTab(toTabId(id));
    });
    addCommand("activateTab", [client](const std::string& id) {
        client->uiRequestedActivateTab(toTabId(id));
    });
    addCommand("navigate", [client](const std::string& text) {
        client->uiRequestedNavigate(text);
    });
    addCommand("goBack",
               [client](const std::string&) { client->uiRequestedGoBack(); });
    addCommand("goForward", [client](const std::string&) {
        client->uiRequestedGoForward();
    });
    addCommand("reload",
               [client](const std::string&) { client->uiRequestedReload(); });
}

void BrowserUIChannel::addCommand(
    const char* name, const std::function<void(const std::string&)>& handler)
{
    std::shared_ptr<bool> isAlive = m_isAlive;
    m_container->AddJavaScriptInterface(
        kShellObject, name,
        [isAlive, handler](const std::string& argument) -> std::string {
            // Still inside the page's script here; see Client.
            auto* command = new PendingCommand{ isAlive, [handler, argument] {
                                                   handler(argument);
                                               } };
            g_idle_add_full(
                G_PRIORITY_DEFAULT,
                [](gpointer data) -> gboolean {
                    auto* command = static_cast<PendingCommand*>(data);
                    if (*command->isAlive) {
                        command->run();
                    }
                    return G_SOURCE_REMOVE;
                },
                command,
                [](gpointer data) {
                    delete static_cast<PendingCommand*>(data);
                });
            return std::string();
        });
}

void BrowserUIChannel::evaluate(const std::string& script)
{
    if (m_isReady) {
        m_container->EvaluateJavaScript(script, [](const std::string&) {});
    }
}

void BrowserUIChannel::sendTabsChanged(const TabModel& tabs)
{
    std::string script = std::string(kUIObject) + ".tabsChanged([";
    for (const TabModel::Tab& tab : tabs.tabs()) {
        script += toJSObject(tab) + ",";
    }
    script += "]," + std::to_string(tabs.activeTab()) + ")";
    evaluate(script);
}

void BrowserUIChannel::sendTabUpdated(const TabModel::Tab& tab)
{
    evaluate(std::string(kUIObject) + ".tabUpdated(" + toJSObject(tab) + ")");
}

void BrowserUIChannel::sendFocusAddress()
{
    evaluate(std::string(kUIObject) + ".focusAddress()");
}

} // namespace StarfishShell

#endif
