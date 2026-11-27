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

#ifndef __StarfishShellBrowserInputRouter__
#define __StarfishShellBrowserInputRouter__

#include "PlatformIntegrationData.h"

#include <string>
#include <vector>

namespace StarfishShell {

class View;

// Decides which view an input event of the window belongs to. Pointer
// events go to the view under the pointer, keys and text to the view that
// has the focus; pressing the pointer on a view gives it the focus.
class InputRouter {
public:
    // The views currently shown, the bottom one first (the order they are
    // composited in). A view that is no longer listed stops receiving
    // keys and loses any pointer press it was holding, but its page is not
    // told that it lost the focus. Call before destroying a view.
    void setVisibleViews(const std::vector<View*>& views);

    // The topmost view at a window position, nullptr when there is none.
    View* viewAt(int x, int y) const;

    // nullptr takes the focus away from every view.
    void setFocusedView(View* view);

    View* focusedView() const
    {
        return m_focusedView;
    }

    // Positions are in window pixels, origin at the top left.
    void pointerMoved(int x, int y);
    void pointerButtonChanged(int x, int y, bool isPressed);
    void wheelScrolled(int x, int y, int delta);
    void keyChanged(LWE::KeyValue key, bool isPressed);
    void textCommitted(const std::string& text);

private:
    std::vector<View*> m_views;
    View* m_focusedView = nullptr;
    // The view the primary button went down on. It keeps receiving the
    // pointer until the button is released, so a drag that leaves the
    // view still ends in it.
    View* m_pressedView = nullptr;
};

} // namespace StarfishShell

#endif
