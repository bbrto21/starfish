# Browser mode

`Starfish browser` runs the x11 shell as a small tabbed browser. It is a
development-time test bed, not a browser product.

## Purpose and scope

Starfish is meant to be embedded in real browsers later. A browser asks
things of an engine that a single-page shell never does: several views
alive at once, views that come and go, a host that composites them and
routes input between them, pages in the background, a chrome that is
itself a page. This mode exists so those paths are built against, run in
CI and kept working while the engine is developed, before a product
depends on them.

So it is deliberately the smallest host that exercises them:

- The shell owns one X window and its input. Every page, and the tab strip
  itself, is an `LWE::WebContainer` that renders into a texture, and the
  shell draws those textures into the window.
- It uses the public embedding API only (`inc/`), the way an embedder
  would. What it needs and the API lacks is a finding about the API, not
  something to reach around.

What it is not, and should not grow into here:

- A product. There is no packaging or installation, no settings, history,
  bookmarks, downloads, permission prompts, certificate UI, private mode
  or session restore, and the tab strip is as plain as it can be.
- A security boundary. Everything runs in one process on one thread; all
  tabs share one engine instance, and a crash in one page ends the run.
  The tab strip is kept apart from pages only as far as the embedding API
  allows (see Tab strip).
- A reference design for a product browser's shell. Its structure is
  chosen to keep the engine-facing parts clear and testable, on one
  platform (X11, GLib main loop, OpenGL ES 3).

The plain shell (`Starfish <URL>`) is a different path: one `LWE::WebView`
that the x11 bridge puts in a window of its own. Nothing in this directory
is used by it.

## Build and run

The mode is compiled in with `-DSTARFISH_SHELL_BROWSER=ON` (off by default)
and needs `-DSHELL=x11 -DBACKEND=glib_cairo_gl`: it creates its own X
window, attaches GLib sources to the main loop and composites with
OpenGL ES 3.

```sh
./Starfish browser [URL...] [options]
```

Each URL opens in its own tab; without one a blank tab opens. The options
of the plain shell apply (`--width`, `--height`, `--timeout`,
`--storage-dir`, `--hide-window`, `--device-pixel-ratio`, `--screen-shot`,
`--regression-test`, ...), plus two of its own:

| Option | Effect |
|---|---|
| `--no-ui` | No tab strip or toolbar; the page fills the window. For test runners |
| `--ui-dir=<path>` | Read the tab strip page from `<path>` instead of `ui/` here |

| Key | Action |
|---|---|
| Ctrl+T | Open a tab |
| Ctrl+W | Close the active tab; closing the last one exits |
| Ctrl+Tab, Ctrl+PageDown | Next tab |
| Ctrl+Shift+Tab, Ctrl+PageUp | Previous tab |
| Ctrl+L | Move to the address bar |

## How tabs are shown

One X window, split by the shell into rectangles. Each rectangle shows the
texture of one view; a view is one `WebContainer`.

```
 X window (BrowserWindow)
+----------------------------------------------------------+
| [ Tab A ][ Tab B* ][ Tab C ] [+]                          |  tab strip view
| [<] [>] [R] [ address bar                               ] |  ui/index.html, 76 CSS px
+----------------------------------------------------------+
|                                                          |
|                                                          |
|   view of the active tab (B)                             |  page view
|                                                          |
|                                                          |
+----------------------------------------------------------+
  with --no-ui there is no strip and the page view fills the window
```

Every tab has a view of its own, but only the active tab's view is on
screen. The others stay alive, paused, with their last frame:

```
                 WebContainer    GL context     texture
                 ------------    ----------     -------
 tab A  View ---- page A -------- context A ---> texture A     paused, not drawn
 tab B  View ---- page B -------- context B ---> texture B --+
 tab C  View ---- page C -------- context C ---> texture C   |  paused, not drawn
 strip  View ---- ui/index.html - context U ---> texture U --+
                                                             |
                    contexts share textures with             v
                    the compositor's context           Compositor
                                                  copies the visible textures
                                                  to their rectangles, bottom
                                                  view first, and presents
                                                             |
                                                             v
                                                  EGL surface of the X window
```

Switching to tab C changes which page view is listed as visible: B is
paused, C is resumed and resized to the page rectangle if the window
changed meanwhile, and the next composite draws texture C in the same
rectangle. No window, surface or context is created or destroyed.

Input takes the reverse route:

```
 X event --> BrowserWindow --> Browser --+-- Ctrl+T/W/Tab/L --> tab command
                                         |
                                         +--> InputRouter
                                                |-- pointer, wheel --> view under the pointer
                                                |                      (strip or active page),
                                                |                      in that view's coordinates
                                                +-- keys, IME text --> focused view
```

And tab state flows one way, from the model to the strip:

```
 page events (load started/finished) --> TabModel --> BrowserUIChannel --> strip page redraws
 strip clicks --> BrowserUIChannel --> Browser::newTab/closeTab/activateTab --> TabModel
```

## Memory structure

Everything is in one process and on one thread. What a tab adds, and what
all tabs share:

```
 process (one thread runs the shell, every page and the tab strip)
 |
 +-- shared by all views
 |     garbage-collected heap     one heap holds the DOM and script objects
 |                                of every view; a collection walks them all
 |     engine globals             atomic and static strings, line-break
 |                                iterators, the HTTP cache
 |     storage directory          cookies, localStorage and caches of one
 |                                --storage-dir
 |     shell                      X window, EGL surface, compositor context
 |
 +-- per view (each tab, and the tab strip)
       shell     View             texture of the view's size (width x height
                                  x 4 bytes), GL context, framebuffer
       engine    WebView          renderer and its GL resources, timers,
                                  font cache and font selector
                 script VM        one VM instance per view, not shared with
                                  any other view
                 browsing context document, layout tree and one script
                                  context for the top-level page and one for
                                  each iframe, all inside the view's VM
```

- A view's VM does not outlive a page: each top-level navigation throws the
  VM away and makes a new one for the next document.
- Views cannot reach each other's script objects. That is the right
  default for unrelated tabs; a page and a window it opened would need one
  VM between them, which this layout does not give.
- Closing a tab destroys its view, VM and texture. The memory goes back to
  the shared heap, and is reused or returned by later collections rather
  than at once.
- A background tab is paused, which stops its rendering and animation
  frames. It keeps its DOM, its VM, its texture and its timers; nothing is
  discarded from it, and the shell has no memory limit or tab eviction.
- Because the heap and the thread are shared, a long script or a large
  collection in one tab stalls the others, and memory is not attributed to
  tabs: the numbers below are differences between runs, not per-tab
  accounting.

Measured resident size with N tabs open (debug build, software GL, 1280x720
window, six seconds after start; software GL keeps textures in ordinary
memory, so a GPU build will differ):

| Tabs | Blank pages | Pages of 200 paragraphs |
|---|---|---|
| 1 | 207 MB | 212 MB |
| 2 | 219 MB | 226 MB |
| 4 | 245 MB | 257 MB |
| 8 | 298 MB | 317 MB |
| 16 | 402 MB | 438 MB |

About 13 MB for each blank tab and 15 MB for each text tab, of which
3.7 MB is the view's texture. These were taken without the tab strip; it
is one more view.

## Parts

| File | Responsibility | Knows about |
|---|---|---|
| `TabModel` | Tab order, the active tab, each tab's URL, title, loading and history state. The only place tab state lives | nothing |
| `BrowserWindow` | The X window; turns X events (pointer, wheel, keys, XIM text, resize, close) into `Client` calls | Xlib, GLib |
| `Compositor` | The window's EGL surface, the GL contexts and textures of the views, and drawing the view textures into the window | EGL, GLES |
| `View` | One `WebContainer`, the texture it renders into and its rectangle in the window | `Compositor`, LWE |
| `InputRouter` | Which view an input event belongs to: pointer by position, keys by focus | `View` |
| `BrowserUIChannel` | The messages between the shell and the tab strip page | `TabModel` types, LWE |
| `Browser` | Ties the parts together; tab commands and keyboard commands | all of the above |
| `ui/` | The tab strip and toolbar page | `BrowserUIChannel`'s messages |

Rules the structure relies on:

- Tab state is read from `TabModel`. The tab strip draws a copy it is sent;
  nothing else keeps one.
- Tabs change only through `Browser::newTab`, `closeTab` and `activateTab`.
  Keyboard commands and the tab strip both end up there.
- The shell and the tab strip page exchange only the messages defined in
  `BrowserUIChannel.h`.
- Headers here name Xlib, GLib and EGL types without including their
  headers: Xlib defines `None`, `Bool` and `Status` as macros.

## Rendering

Every view has its own GL context. All contexts share objects with the
compositor's, so a texture a page rendered into can be drawn by the
compositor.

1. `View` creates a texture of its size and a `WebContainer` with
   `CreateGLWithRenderTarget`.
2. When the page paints, the engine asks for a render target; the view
   returns a framebuffer with its texture attached. Framebuffer objects are
   not shared between contexts, so this one lives in the view's context.
3. When the frame is done the view flushes and tells `Browser`, which
   composites if the view is on screen.
4. `Compositor::composite` copies each visible view's texture to its
   rectangle of the window, bottom view first, and presents.

Two things to keep in mind when changing this:

- The engine remembers which context it bound last and does not bind it
  again. Shell code that binds another context must put the previous one
  back; `Compositor` does this with a scoped helper.
- On resize a view gets a new texture, not new storage for the old one, and
  drops its framebuffer attachment explicitly. A context only sees another
  context's change to a shared texture after attaching it again, and GL may
  hand out the old texture name for the new texture.

Only the active tab and the tab strip are on screen. A tab in the
background is paused and keeps its last frame; it takes the current window
size when it is shown again.

Sizes and positions are window pixels throughout the shell.
`--device-pixel-ratio` only changes how many of them a CSS pixel covers.

## Input

`BrowserWindow` delivers events to `Browser`. Keys held with Control are
checked against the keyboard commands first; a command key is not passed
on to a page. Everything else goes to `InputRouter`:

- Pointer and wheel events go to the topmost view at that position,
  translated to the view's own coordinates.
- A press gives its view the focus. The view keeps the pointer until the
  button is released, so a drag that leaves it still ends in it.
- Keys and committed input-method text go to the focused view.

## Tab strip

`ui/index.html` is loaded from disk at start-up into a view above the
pages, so it can be edited without rebuilding. By default it is found by
walking up from the executable to the `src/shell/browser/ui` of the source
tree it was built in; a build that is moved away from its tree needs
`--ui-dir`.

Messages, defined in `BrowserUIChannel.h` and mirrored in `ui/ui.js`:

| Direction | Message |
|---|---|
| Page to shell | `shell.newTab()`, `shell.closeTab(id)`, `shell.activateTab(id)`, `shell.navigate(text)`, `shell.goBack()`, `shell.goForward()`, `shell.reload()` |
| Shell to page | `browserUI.tabsChanged(tabs, activeId)`, `browserUI.tabUpdated(tab)`, `browserUI.focusAddress()` |

A tab is `{ id, url, title, isLoading, canGoBack, canGoForward }`.

- A command is run on a later turn of the main loop, not inside the page's
  call, so it may create and destroy views.
- The `shell` object exists only in the tab strip's view. Pages cannot
  reach it, and the tab strip's view refuses to navigate anywhere else.
- Text sent to the page is escaped as a JavaScript string literal; page
  titles and URLs are untrusted.

## Tests

Both run with `Starfish unit-test` on a build that has the mode enabled.

- `src/shell/test/BrowserTabModelTest.cpp` covers `TabModel` alone.
- `src/shell/test/BrowserTest.cpp` drives a real browser the way a user
  does: a second X connection sends pointer and key events to the window
  and reads its pixels back. It covers what is on screen, input reaching
  the right page, tab commands, the tab strip, resize, device pixel ratio
  and exit.

Test runners that judge a single page can use the mode through a wrapper
that adds `--no-ui`, passed as `STARFISH_BIN`:

```sh
#!/bin/bash
exec /path/to/Starfish browser "$@" --no-ui
```

## Known limits

Left out on purpose, as outside the scope above:

- Product features and hardening; see Purpose and scope.
- Platforms other than X11 with the GLib main loop; the `uv_cairo_gl`
  backend is not supported.
- Running the engine on its own thread: `--prefer-isolated-thread` has no
  effect here.

Findings about the engine or its embedding API that a product browser
would run into as well:

- The API has no callback for a title change. A tab's title is read when a
  load finishes, so a later change to `document.title` is not shown.
- The engine has no notion of a view having the system focus.
  `WebContainer::Focus()` does nothing and `Blur()` makes the page drop its
  focused element, so moving the focus between the tab strip and a page
  loses that page's caret, and pages get no window `focus` or `blur`
  events. Switching tabs avoids `Blur()` and keeps the caret.

Limits of this implementation:

- Input-method composition is committed text only (XIM reports no
  composition updates), and has not been exercised against a real input
  method.
- A resized view shows nothing until its page has painted at the new size.
- Every frame of a visible view recomposites the whole window.
- Not tried under a window manager; tests run on a bare Xvfb display.
