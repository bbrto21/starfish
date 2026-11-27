# Browser mode

`Starfish browser` runs the x11 shell as a tabbed browser. The shell owns
one X window and its input; every page, and the tab strip itself, is an
`LWE::WebContainer` that renders into a texture, and the shell draws those
textures into the window.

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

- GLib main loop only; the `uv_cairo_gl` backend is not supported.
- A tab's title is read when a load finishes. A later change to
  `document.title` is not shown: the embedding API has no callback for it.
- Moving the focus between the tab strip and a page blurs the view that
  loses it, and the engine then forgets that view's focused element.
  Switching tabs does not blur, so a tab keeps its caret.
- Pages get no `focus` or `blur` window events from the shell.
- Input-method composition is committed text only; XIM reports no
  composition updates.
- A resized view shows nothing until its page has painted at the new size.
- The engine runs on the shell's thread; `--prefer-isolated-thread` has no
  effect here.
