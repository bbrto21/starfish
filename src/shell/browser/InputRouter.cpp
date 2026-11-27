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

#include "browser/InputRouter.h"

#include "browser/View.h"

#include "LWEWebView.h"

#include <algorithm>

namespace StarfishShell {

void InputRouter::setVisibleViews(const std::vector<View*>& views)
{
    m_views = views;
    auto isVisible = [this](View* view) {
        return std::find(m_views.begin(), m_views.end(), view) != m_views.end();
    };
    if (m_pressedView && !isVisible(m_pressedView)) {
        m_pressedView = nullptr;
    }
    if (m_focusedView && !isVisible(m_focusedView)) {
        setFocusedView(nullptr);
    }
}

View* InputRouter::viewAt(int x, int y) const
{
    for (auto it = m_views.rbegin(); it != m_views.rend(); ++it) {
        const View::Rect& rect = (*it)->rect();
        if (x >= rect.x && y >= rect.y &&
            x < rect.x + static_cast<int>(rect.width) &&
            y < rect.y + static_cast<int>(rect.height)) {
            return *it;
        }
    }
    return nullptr;
}

void InputRouter::setFocusedView(View* view)
{
    if (m_focusedView == view) {
        return;
    }
    if (m_focusedView) {
        m_focusedView->container()->Blur();
    }
    m_focusedView = view;
    if (m_focusedView) {
        m_focusedView->container()->Focus();
    }
}

void InputRouter::pointerMoved(int x, int y)
{
    View* view = m_pressedView ? m_pressedView : viewAt(x, y);
    if (!view) {
        return;
    }
    // The held button lets the page follow a drag.
    view->container()->DispatchMouseMoveEvent(
        LWE::MouseButtonValue::NoButton,
        m_pressedView ? LWE::MouseButtonsValue::LeftButtonDown
                      : LWE::MouseButtonsValue::NoButtonDown,
        x - view->rect().x, y - view->rect().y);
}

void InputRouter::pointerButtonChanged(int x, int y, bool isPressed)
{
    if (isPressed) {
        View* view = viewAt(x, y);
        m_pressedView = view;
        if (!view) {
            return;
        }
        setFocusedView(view);
        view->container()->DispatchMouseDownEvent(
            LWE::MouseButtonValue::LeftButton,
            LWE::MouseButtonsValue::LeftButtonDown, x - view->rect().x,
            y - view->rect().y);
        return;
    }

    View* view = m_pressedView;
    m_pressedView = nullptr;
    if (!view) {
        return;
    }
    view->container()->DispatchMouseUpEvent(
        LWE::MouseButtonValue::LeftButton, LWE::MouseButtonsValue::NoButtonDown,
        x - view->rect().x, y - view->rect().y);
}

void InputRouter::wheelScrolled(int x, int y, int delta)
{
    if (View* view = viewAt(x, y)) {
        view->container()->DispatchMouseWheelEvent(x - view->rect().x,
                                                   y - view->rect().y, delta);
    }
}

void InputRouter::keyChanged(LWE::KeyValue key, bool isPressed)
{
    if (!m_focusedView) {
        return;
    }
    if (isPressed) {
        m_focusedView->container()->DispatchKeyDownEvent(key);
        m_focusedView->container()->DispatchKeyPressEvent(key);
    } else {
        m_focusedView->container()->DispatchKeyUpEvent(key);
    }
}

void InputRouter::textCommitted(const std::string& text)
{
    if (m_focusedView) {
        // XIM reports a finished composition only, with no updates.
        m_focusedView->container()->DispatchCompositionEndEvent(text);
    }
}

} // namespace StarfishShell

#endif
