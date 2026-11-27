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

#ifndef __StarfishShellBrowserView__
#define __StarfishShellBrowserView__

#include <cstdint>
#include <functional>

namespace LWE {
class WebContainer;
}

namespace StarfishShell {

class Compositor;

// One web page: an LWE::WebContainer that renders into a texture the
// compositor can draw, and the rectangle of the window it is shown in.
class View {
public:
    // In window pixels, origin at the top left.
    struct Rect {
        int x = 0;
        int y = 0;
        unsigned width = 0;
        unsigned height = 0;
    };

    // Called each time the page has drawn a new frame into the texture.
    typedef std::function<void(View*)> FrameHandler;

    // The compositor must outlive the view.
    explicit View(Compositor& compositor);
    ~View();

    // LWE must be initialized.
    bool init(const Rect& rect, const FrameHandler& frameHandler);

    // Moves the view; a new size also resizes the page.
    void setRect(const Rect& rect);

    const Rect& rect() const
    {
        return m_rect;
    }

    // Holds the latest frame once hasFrame() is true.
    unsigned texture() const
    {
        return m_texture;
    }

    bool hasFrame() const
    {
        return m_hasFrame;
    }

    LWE::WebContainer* container()
    {
        return m_container;
    }

private:
    // The framebuffer the page renders into, with m_texture attached.
    // Runs with the view's context current.
    unsigned renderFramebuffer();

    Compositor& m_compositor;
    FrameHandler m_frameHandler;
    LWE::WebContainer* m_container = nullptr;
    Rect m_rect;
    uintptr_t m_context = 0;
    unsigned m_texture = 0;
    // Framebuffer objects are not shared between contexts, so this one
    // lives in m_context.
    unsigned m_framebuffer = 0;
    // The texture currently attached to m_framebuffer.
    unsigned m_attachedTexture = 0;
    bool m_hasFrame = false;
};

} // namespace StarfishShell

#endif
