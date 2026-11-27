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

#include "browser/View.h"

#include "browser/Compositor.h"

#include "LWEWebView.h"

#include <GLES3/gl3.h>

namespace StarfishShell {

View::View(Compositor& compositor)
    : m_compositor(compositor)
{
}

View::~View()
{
    if (m_container) {
        // The page releases its GL resources, which needs m_context.
        m_container->Destroy();
    }
    if (m_texture) {
        m_compositor.deleteTexture(m_texture);
    }
    if (m_context) {
        // Takes m_framebuffer with it.
        m_compositor.destroyContext(m_context);
    }
}

bool View::init(const Rect& rect, const FrameHandler& frameHandler)
{
    m_rect = rect;
    m_frameHandler = frameHandler;

    m_context = m_compositor.createContext();
    if (!m_context) {
        return false;
    }
    m_texture = m_compositor.createTexture(rect.width, rect.height);
    if (!m_texture) {
        return false;
    }

    LWE::WebContainer::WebContainerArguments args{
        rect.width, rect.height, 1, "serif", "ko-KR", "Asia/Seoul",
    };

    typedef LWE::WebContainer WebContainer;
    WebContainer::RendererGLConfiguration config;
    config.onMakeCurrent = [this](WebContainer*) {
        m_compositor.makeCurrent(m_context);
    };
    config.onSwapBuffers = [this](WebContainer*, bool) {
        // The compositor reads the texture from its own context.
        glFlush();
        m_hasFrame = true;
        if (m_frameHandler) {
            m_frameHandler(this);
        }
    };
    // Contexts the page asks for (WebGL, ...) come from the same share
    // group, so their textures reach the page's context.
    config.onCreateSharedContext = [this](WebContainer*) -> uintptr_t {
        uintptr_t context = m_compositor.createContext();
        return context ? context : UINTPTR_MAX;
    };
    config.onDestroyContext = [this](WebContainer*, uintptr_t context) -> bool {
        m_compositor.destroyContext(context);
        return true;
    };
    config.onClearCurrentContext = [this](WebContainer*) -> bool {
        return m_compositor.clearCurrent();
    };
    config.onMakeCurrentWithContext = [this](WebContainer*,
                                             uintptr_t context) -> bool {
        return m_compositor.makeCurrent(context);
    };
    config.onGetProcAddress = [this](WebContainer*, const char* name) -> void* {
        return m_compositor.getProcAddress(name);
    };
    config.onIsSupportedExtension = [this](WebContainer*,
                                           const char* extension) -> bool {
        return m_compositor.isSupportedExtension(extension);
    };

    m_container = WebContainer::CreateGLWithRenderTarget(
        args, config,
        [this](WebContainer*, WebContainer::GLRenderTarget& target) {
            target = WebContainer::GLRenderTarget{};
            target.width = m_rect.width;
            target.height = m_rect.height;
            target.clipWidth = m_rect.width;
            target.clipHeight = m_rect.height;
            target.matrix[0] = target.matrix[4] = target.matrix[8] = 1;
            target.opacity = 1;
            target.color[0] = target.color[1] = target.color[2] = 1;
            target.framebuffer = renderFramebuffer();
            return true;
        });
    return m_container;
}

void View::setRect(const Rect& rect)
{
    bool isResized = rect.width != m_rect.width || rect.height != m_rect.height;
    m_rect = rect;
    if (!isResized) {
        return;
    }
    // A new texture rather than new storage for the old one: a context
    // only sees another context's change to a shared texture once it
    // attaches the texture again. renderFramebuffer() attaches this one.
    m_compositor.deleteTexture(m_texture);
    m_texture = m_compositor.createTexture(rect.width, rect.height);
    // GL may hand the old name out again, so the name alone does not
    // tell that the attachment is stale.
    m_attachedTexture = 0;
    m_hasFrame = false;
    m_container->ResizeTo(rect.width, rect.height);
}

void View::setPaused(bool isPaused)
{
    if (m_isPaused == isPaused) {
        return;
    }
    m_isPaused = isPaused;
    if (isPaused) {
        m_container->Pause();
    } else {
        m_container->Resume();
    }
}

unsigned View::renderFramebuffer()
{
    if (m_framebuffer && m_attachedTexture == m_texture) {
        return m_framebuffer;
    }
    // The engine tracks its own GL state; put its binding back.
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    if (!m_framebuffer) {
        glGenFramebuffers(1, &m_framebuffer);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, previous);
    m_attachedTexture = m_texture;
    return m_framebuffer;
}

} // namespace StarfishShell

#endif
