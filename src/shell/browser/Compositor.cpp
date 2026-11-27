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

#include "browser/Compositor.h"

#include "browser/View.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cstdio>
#include <cstring>

namespace StarfishShell {

namespace {

    EGLContext toEGLContext(uintptr_t context)
    {
        return reinterpret_cast<EGLContext>(context);
    }

} // namespace

// Binds the compositor's context for a scope and puts the previous
// binding back afterwards. The engine remembers which context it bound
// last and does not bind it again, so the shell must never leave a
// different one current.
class Compositor::ScopedOwnContext {
public:
    explicit ScopedOwnContext(Compositor& compositor)
        : m_compositor(compositor)
        , m_previousContext(eglGetCurrentContext())
        , m_previousDraw(eglGetCurrentSurface(EGL_DRAW))
        , m_previousRead(eglGetCurrentSurface(EGL_READ))
    {
        eglMakeCurrent(m_compositor.m_display, m_compositor.m_surface,
                       m_compositor.m_surface, m_compositor.m_context);
    }

    ~ScopedOwnContext()
    {
        eglMakeCurrent(m_compositor.m_display, m_previousDraw, m_previousRead,
                       m_previousContext);
    }

private:
    Compositor& m_compositor;
    EGLContext m_previousContext;
    EGLSurface m_previousDraw;
    EGLSurface m_previousRead;
};

Compositor::~Compositor()
{
    if (!m_display) {
        return;
    }
    if (m_context) {
        {
            ScopedOwnContext scope(*this);
            if (m_readFramebuffer) {
                glDeleteFramebuffers(1, &m_readFramebuffer);
            }
        }
        if (eglGetCurrentContext() == m_context) {
            eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
        }
        eglDestroyContext(m_display, m_context);
    }
    if (m_surface) {
        eglDestroySurface(m_display, m_surface);
    }
    // No eglTerminate: EGL_DEFAULT_DISPLAY is shared by the whole process.
}

bool Compositor::init(unsigned long nativeWindow)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY ||
        !eglInitialize(display, nullptr, nullptr)) {
        fprintf(stderr, "Browser: cannot initialize EGL (0x%x)\n",
                eglGetError());
        return false;
    }
    m_display = display;
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint configAttributes[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_DEPTH_SIZE,
        0,
        EGL_STENCIL_SIZE,
        0,
        EGL_SAMPLES,
        0,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (!eglChooseConfig(display, configAttributes, &config, 1, &configCount) ||
        configCount != 1) {
        fprintf(stderr, "Browser: no EGL config for OpenGL ES 3 (0x%x)\n",
                eglGetError());
        return false;
    }
    m_config = config;

    // The surface follows the size of its window by itself.
    EGLSurface surface =
        eglCreateWindowSurface(display, config, nativeWindow, nullptr);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Browser: cannot create EGL surface (0x%x)\n",
                eglGetError());
        return false;
    }
    m_surface = surface;

    m_context = toEGLContext(createContext());
    if (!m_context) {
        return false;
    }

    ScopedOwnContext scope(*this);
    glGenFramebuffers(1, &m_readFramebuffer);
    return true;
}

uintptr_t Compositor::createContext()
{
    // The compositor copies view textures with glBlitFramebuffer, which
    // needs OpenGL ES 3.
    const EGLint attributes[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE };
    // m_context is still null for the compositor's own context, which is
    // the first one made and the root of the share group.
    EGLContext context =
        eglCreateContext(m_display, m_config, m_context, attributes);
    if (context == EGL_NO_CONTEXT) {
        fprintf(stderr, "Browser: cannot create GL context (0x%x)\n",
                eglGetError());
        return kNoContext;
    }
    return reinterpret_cast<uintptr_t>(context);
}

void Compositor::destroyContext(uintptr_t context)
{
    if (eglGetCurrentContext() == toEGLContext(context)) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
    }
    eglDestroyContext(m_display, toEGLContext(context));
}

bool Compositor::makeCurrent(uintptr_t context)
{
    return eglMakeCurrent(m_display, m_surface, m_surface,
                          toEGLContext(context));
}

bool Compositor::clearCurrent()
{
    return eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                          EGL_NO_CONTEXT);
}

unsigned Compositor::createTexture(unsigned width, unsigned height)
{
    ScopedOwnContext scope(*this);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    // Another context of the share group is about to use the texture.
    glFlush();
    return texture;
}

void Compositor::deleteTexture(unsigned texture)
{
    ScopedOwnContext scope(*this);
    glDeleteTextures(1, &texture);
}

void* Compositor::getProcAddress(const char* name)
{
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}

bool Compositor::isSupportedExtension(const char* extension)
{
    const char* extensions = eglQueryString(m_display, EGL_EXTENSIONS);
    return extensions && strstr(extensions, extension);
}

void Compositor::composite(const std::vector<View*>& views,
                           unsigned windowHeight)
{
    ScopedOwnContext scope(*this);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_readFramebuffer);
    for (View* view : views) {
        if (!view->hasFrame()) {
            continue;
        }
        const View::Rect& rect = view->rect();
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, view->texture(), 0);
        // View rectangles have their origin at the top left of the
        // window, GL at the bottom left.
        int top = static_cast<int>(windowHeight) - rect.y;
        int bottom = top - static_cast<int>(rect.height);
        glBlitFramebuffer(0, 0, rect.width, rect.height, rect.x, bottom,
                          rect.x + static_cast<int>(rect.width), top,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    eglSwapBuffers(m_display, m_surface);
}

} // namespace StarfishShell

#endif
