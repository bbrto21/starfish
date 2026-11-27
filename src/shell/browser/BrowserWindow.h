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

#ifndef __StarfishShellBrowserWindow__
#define __StarfishShellBrowserWindow__

#include "PlatformIntegrationData.h"

#include <string>

// Xlib and GLib types are only named here; their headers define macros
// (None, Bool, Status, ...) that must not leak into every includer.
struct _XDisplay;
struct _XIM;
struct _XIC;
struct _GSource;
union _XEvent;

namespace StarfishShell {

// The X window of the browser. Turns X events into the calls of Client
// and knows nothing about what is drawn inside.
class BrowserWindow {
public:
    enum Modifier : unsigned {
        ShiftModifier = 1 << 0,
        ControlModifier = 1 << 1,
        AltModifier = 1 << 2,
    };

    // Positions are in window pixels, origin at the top left.
    class Client {
    public:
        virtual ~Client() = default;
        virtual void windowResized(unsigned width, unsigned height) = 0;
        // The window content was lost and must be drawn again.
        virtual void windowExposed() = 0;
        virtual void windowCloseRequested() = 0;
        virtual void pointerMoved(int x, int y) = 0;
        // The primary button.
        virtual void pointerButtonChanged(int x, int y, bool isPressed) = 0;
        // `delta` is -1 for one step up, 1 for one step down.
        virtual void wheelScrolled(int x, int y, int delta) = 0;
        // `modifiers` are the Modifier bits held with the key. A letter
        // typed with Control or Alt arrives as its unmodified key.
        virtual void keyChanged(LWE::KeyValue key, bool isPressed,
                                unsigned modifiers) = 0;
        // Text from the input method that no single key stands for.
        virtual void textCommitted(const std::string& text) = 0;
    };

    // The client must outlive the window.
    explicit BrowserWindow(Client& client);
    ~BrowserWindow();

    // Creates and shows the window, and starts delivering its events
    // from the default GLib main context.
    bool init(const char* title, unsigned width, unsigned height);

    // The X window id.
    unsigned long nativeHandle() const
    {
        return m_window;
    }

    unsigned width() const
    {
        return m_width;
    }

    unsigned height() const
    {
        return m_height;
    }

    // Whether key presses go through the input method.
    void setInputMethodActive(bool isActive);

private:
    struct EventSource;

    void dispatchPendingEvents();
    void handleKeyEvent(_XEvent& event);

    Client& m_client;
    _XDisplay* m_display = nullptr;
    unsigned long m_window = 0;
    unsigned long m_wmDeleteWindow = 0;
    _XIM* m_inputMethod = nullptr;
    _XIC* m_inputContext = nullptr;
    _GSource* m_eventSource = nullptr;
    unsigned m_width = 0;
    unsigned m_height = 0;
};

} // namespace StarfishShell

#endif
