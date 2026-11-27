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

#include "browser/BrowserWindow.h"

#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <glib.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace StarfishShell {

namespace {

    // UnidentifiedKey for a key that is neither listed nor printable ASCII.
    LWE::KeyValue toKeyValue(KeySym keysym)
    {
        switch (keysym) {
        case XK_BackSpace:
            return LWE::KeyValue::BackspaceKey;
        case XK_Tab:
        // Shift+Tab.
        case XK_ISO_Left_Tab:
            return LWE::KeyValue::TabKey;
        case XK_Return:
            return LWE::KeyValue::EnterKey;
        case XK_Escape:
            return LWE::KeyValue::EscapeKey;
        case XK_Left:
            return LWE::KeyValue::ArrowLeftKey;
        case XK_Up:
            return LWE::KeyValue::ArrowUpKey;
        case XK_Right:
            return LWE::KeyValue::ArrowRightKey;
        case XK_Down:
            return LWE::KeyValue::ArrowDownKey;
        case XK_Delete:
            return LWE::KeyValue::DeleteKey;
        case XK_Insert:
            return LWE::KeyValue::InsertKey;
        case XK_Home:
            return LWE::KeyValue::HomeKey;
        case XK_End:
            return LWE::KeyValue::EndKey;
        case XK_Page_Up:
            return LWE::KeyValue::PageUpKey;
        case XK_Page_Down:
            return LWE::KeyValue::PageDownKey;
        case XK_Shift_L:
            return LWE::KeyValue::ShiftLeftKey;
        case XK_Shift_R:
            return LWE::KeyValue::ShiftRightKey;
        case XK_Control_L:
        case XK_Control_R:
            return LWE::KeyValue::ControlLeftKey;
        case XK_Alt_L:
        case XK_Alt_R:
            return LWE::KeyValue::AltLeftKey;
        case XK_F1:
            return LWE::KeyValue::F1Key;
        case XK_F2:
            return LWE::KeyValue::F2Key;
        case XK_F3:
            return LWE::KeyValue::F3Key;
        case XK_F4:
            return LWE::KeyValue::F4Key;
        case XK_F5:
            return LWE::KeyValue::F5Key;
        case XK_F6:
            return LWE::KeyValue::F6Key;
        case XK_F7:
            return LWE::KeyValue::F7Key;
        case XK_F8:
            return LWE::KeyValue::F8Key;
        case XK_F9:
            return LWE::KeyValue::F9Key;
        case XK_F10:
            return LWE::KeyValue::F10Key;
        case XK_F11:
            return LWE::KeyValue::F11Key;
        case XK_F12:
            return LWE::KeyValue::F12Key;
        default:
            break;
        }
        if (keysym >= 0x20 && keysym <= 0x7e) {
            return static_cast<LWE::KeyValue>(keysym);
        }
        return LWE::KeyValue::UnidentifiedKey;
    }

    unsigned toModifiers(unsigned state)
    {
        unsigned modifiers = 0;
        if (state & ShiftMask) {
            modifiers |= BrowserWindow::ShiftModifier;
        }
        if (state & ControlMask) {
            modifiers |= BrowserWindow::ControlModifier;
        }
        if (state & Mod1Mask) {
            modifiers |= BrowserWindow::AltModifier;
        }
        return modifiers;
    }

    bool isPrintableASCII(KeySym keysym)
    {
        return keysym >= 0x20 && keysym <= 0x7e;
    }

} // namespace

// Runs dispatchPendingEvents() from the GLib main loop. Xlib reads ahead
// while it waits for a reply, so events can sit in its queue with nothing
// left to read on the connection; prepare() and check() look at the queue
// as well as the descriptor.
struct BrowserWindow::EventSource {
    GSource base;
    BrowserWindow* window;

    static gboolean prepare(GSource* source, gint* timeout)
    {
        *timeout = -1;
        return XPending(
                   reinterpret_cast<EventSource*>(source)->window->m_display) >
               0;
    }

    static gboolean check(GSource* source)
    {
        return XPending(
                   reinterpret_cast<EventSource*>(source)->window->m_display) >
               0;
    }

    static gboolean dispatch(GSource* source, GSourceFunc, gpointer)
    {
        reinterpret_cast<EventSource*>(source)->window->dispatchPendingEvents();
        return G_SOURCE_CONTINUE;
    }

    static GSourceFuncs s_functions;
};

GSourceFuncs BrowserWindow::EventSource::s_functions = {
    EventSource::prepare,
    EventSource::check,
    EventSource::dispatch,
    nullptr,
    nullptr,
    nullptr,
};

BrowserWindow::BrowserWindow(Client& client)
    : m_client(client)
{
}

BrowserWindow::~BrowserWindow()
{
    if (m_eventSource) {
        g_source_destroy(m_eventSource);
        g_source_unref(m_eventSource);
    }
    if (m_inputContext) {
        XDestroyIC(m_inputContext);
    }
    if (m_inputMethod) {
        XCloseIM(m_inputMethod);
    }
    if (m_display) {
        if (m_window) {
            XDestroyWindow(m_display, m_window);
        }
        XCloseDisplay(m_display);
    }
}

bool BrowserWindow::init(const char* title, unsigned width, unsigned height)
{
    // The input method follows the user's locale.
    if (!setlocale(LC_ALL, "")) {
        fprintf(stderr, "Browser: cannot set the system locale\n");
    }

    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        fprintf(stderr, "Browser: cannot open the X display\n");
        return false;
    }

    const char* modifiers = getenv("XMODIFIERS");
    if (!XSetLocaleModifiers(modifiers ? modifiers : "")) {
        fprintf(stderr, "Browser: cannot set the X locale modifiers\n");
    }

    m_window =
        XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), 0, 0,
                            width, height, 0, 0, WhitePixel(m_display, 0));
    m_width = width;
    m_height = height;
    XSelectInput(m_display, m_window,
                 StructureNotifyMask | ExposureMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | KeyPressMask |
                     KeyReleaseMask);
    XStoreName(m_display, m_window, title);

    // Lets the window manager ask for a close instead of killing the
    // connection.
    m_wmDeleteWindow = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
    Atom protocols[] = { m_wmDeleteWindow };
    XSetWMProtocols(m_display, m_window, protocols, 1);

    XMapWindow(m_display, m_window);

    m_inputMethod = XOpenIM(m_display, nullptr, nullptr, nullptr);
    if (m_inputMethod) {
        m_inputContext = XCreateIC(
            m_inputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, m_window, XNFocusWindow, m_window, nullptr);
    }
    if (m_inputContext) {
        XSetICFocus(m_inputContext);
    } else {
        // Keys still work, as plain key events.
        fprintf(stderr, "Browser: no X input method\n");
    }
    XSync(m_display, False);

    m_eventSource =
        g_source_new(&EventSource::s_functions, sizeof(EventSource));
    reinterpret_cast<EventSource*>(m_eventSource)->window = this;
    g_source_add_unix_fd(
        m_eventSource, ConnectionNumber(m_display),
        static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR));
    g_source_attach(m_eventSource, nullptr);
    return true;
}

void BrowserWindow::setInputMethodActive(bool isActive)
{
    if (!m_inputContext) {
        return;
    }
    if (isActive) {
        XSetICFocus(m_inputContext);
    } else {
        XUnsetICFocus(m_inputContext);
    }
}

void BrowserWindow::dispatchPendingEvents()
{
    while (XPending(m_display)) {
        XEvent event;
        XNextEvent(m_display, &event);
        // The input method consumes the key events of a composition.
        if (XFilterEvent(&event, 0)) {
            continue;
        }

        switch (event.type) {
        case ConfigureNotify: {
            unsigned width = event.xconfigure.width;
            unsigned height = event.xconfigure.height;
            // Also sent when the window only moves.
            if (width != m_width || height != m_height) {
                m_width = width;
                m_height = height;
                m_client.windowResized(width, height);
            }
            break;
        }
        case Expose:
            // The last of a run of Expose events carries count 0.
            if (!event.xexpose.count) {
                m_client.windowExposed();
            }
            break;
        case MotionNotify:
            m_client.pointerMoved(event.xmotion.x, event.xmotion.y);
            break;
        case ButtonPress:
        case ButtonRelease:
            if (event.xbutton.button == Button1) {
                m_client.pointerButtonChanged(event.xbutton.x, event.xbutton.y,
                                              event.type == ButtonPress);
            } else if (event.type == ButtonPress &&
                       (event.xbutton.button == Button4 ||
                        event.xbutton.button == Button5)) {
                // X reports the wheel as buttons 4 (up) and 5 (down).
                m_client.wheelScrolled(event.xbutton.x, event.xbutton.y,
                                       event.xbutton.button == Button4 ? -1
                                                                       : 1);
            }
            break;
        case KeyPress:
        case KeyRelease:
            handleKeyEvent(event);
            break;
        case ClientMessage:
            if (static_cast<unsigned long>(event.xclient.data.l[0]) ==
                m_wmDeleteWindow) {
                m_client.windowCloseRequested();
            }
            break;
        default:
            break;
        }
    }
}

void BrowserWindow::handleKeyEvent(XEvent& event)
{
    bool isPressed = event.type == KeyPress;
    char text[128];
    KeySym keysym = 0;
    int length =
        XLookupString(&event.xkey, text, sizeof(text), &keysym, nullptr);

    // Keys that produce no text never involve the input method, and
    // neither does a letter held with Control or Alt: that is a command,
    // for which X would report a control character.
    unsigned modifiers = toModifiers(event.xkey.state);
    LWE::KeyValue key = toKeyValue(keysym);
    if (key != LWE::KeyValue::UnidentifiedKey &&
        (!isPrintableASCII(keysym) ||
         (modifiers & (ControlModifier | AltModifier)))) {
        m_client.keyChanged(key, isPressed, modifiers);
        return;
    }

    // XmbLookupString is only defined for key presses.
    if (m_inputContext && isPressed) {
        Status status = 0;
        length = XmbLookupString(m_inputContext, &event.xkey, text,
                                 sizeof(text), &keysym, &status);
        if (status != XLookupChars && status != XLookupBoth) {
            return;
        }
    }
    if (length == 1 && isprint(static_cast<unsigned char>(text[0]))) {
        m_client.keyChanged(
            static_cast<LWE::KeyValue>(static_cast<unsigned char>(text[0])),
            isPressed, modifiers);
    } else if (length > 0 && isPressed) {
        m_client.textCommitted(std::string(text, length));
    }
}

} // namespace StarfishShell

#endif
