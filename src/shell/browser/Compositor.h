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

#ifndef __StarfishShellBrowserCompositor__
#define __StarfishShellBrowserCompositor__

#include <cstdint>
#include <vector>

namespace StarfishShell {

class View;

// Owns the GL side of the browser window: the EGL surface of the window,
// the contexts and textures the views render with, and the pass that
// draws the view textures into the window.
//
// Contexts are passed around as uintptr_t, the way LWE's GL callbacks
// carry them. Every context made here shares textures with the
// compositor's own, so a texture a view rendered into can be drawn by
// the compositor.
class Compositor {
public:
    enum : uintptr_t { kNoContext = 0 };

    Compositor() = default;
    ~Compositor();

    // `nativeWindow` is the X window to draw into.
    bool init(unsigned long nativeWindow);

    // Returns kNoContext on failure.
    uintptr_t createContext();
    void destroyContext(uintptr_t context);
    // Binds `context` with the window surface on the calling thread.
    bool makeCurrent(uintptr_t context);
    bool clearCurrent();

    // An RGBA texture of the given size, 0 on failure. The GL binding of
    // the caller is left as it was.
    unsigned createTexture(unsigned width, unsigned height);
    void deleteTexture(unsigned texture);

    void* getProcAddress(const char* name);
    bool isSupportedExtension(const char* extension);

    // Draws `views` into the window, the first one at the bottom, and
    // presents the result. Views that have not drawn a frame yet are
    // skipped. The GL binding of the caller is left as it was.
    void composite(const std::vector<View*>& views, unsigned windowHeight);

private:
    class ScopedOwnContext;

    void* m_display = nullptr;
    void* m_config = nullptr;
    void* m_surface = nullptr;
    void* m_context = nullptr;
    // Lives in m_context; a view texture is attached to it to be read.
    unsigned m_readFramebuffer = 0;
};

} // namespace StarfishShell

#endif
