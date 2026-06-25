# NMD — Notepad++ Markdown Preview Plugin {#root}

Owned floating window, WebView2 renderer, C++.

## Goal {#goal}

A Notepad++ plugin that opens an owned floating window rendering the current
document as Markdown, updating live as you type. No docked panel. Window can
live on a second monitor.

## Key Decisions {#decisions}

| Decision | Choice | Reason |
|---|---|---|
| Language | C++ | Standard NPP plugin path |
| Renderer | WebView2 (Edge Chromium) | Modern CSS, no IE artifacts |
| Markdown parsing | JS-side via `marked.js` (bundled) | Zero C++ deps, full CommonMark, easy theming |
| Build system | Visual Studio 2022 (.vcxproj) | NPP plugin template ships this |
| WebView2 SDK | vcpkg or NuGet `Microsoft.Web.WebView2` | Official distribution |

Markdown is parsed inside WebView2 with bundled `marked.js` — no C++ parser
needed. The plugin passes raw text; JS does the rest.

## Plugin Exports (Required by NPP) {#exports}

Every NPP plugin DLL must export these six functions:

```
isUnicode()       → return true (always)
getName()         → plugin display name
getFuncsArray()   → menu commands array
setInfo()         → store NppData (NPP hwnd, Scintilla hwnds)
beNotified()      → handle SCNotification events
messageProc()     → handle Win32 messages (can return FALSE for most)
```

## Architecture {#architecture}

```
NMD.dll
├── Plugin.cpp          entry point, exports, NppData storage
├── PreviewWindow.cpp   owned floating Win32 window + WebView2 host
├── Markdown.html       bundled template (marked.js inline, CSS theme)
└── Commands.cpp        menu command handlers (toggle window)
```

### Window Ownership {#architecture.ownership}

```cpp
CreateWindowEx(
    WS_EX_TOOLWINDOW,          // no taskbar entry
    CLASS_NAME,
    L"Markdown Preview",
    WS_POPUP | WS_CAPTION | WS_SIZEBOX | WS_SYSMENU,
    x, y, w, h,
    nppData._nppHandle,        // owner = Notepad++
    nullptr, hInstance, nullptr
);
```

`WS_EX_TOOLWINDOW` keeps it out of the taskbar. Owned by NPP so it
minimizes/restores with it.

### WebView2 Initialization {#architecture.webview2}

WebView2 is async-init (COM). Sequence:

1. `CreateCoreWebView2EnvironmentWithOptions` → callback
2. `CreateCoreWebView2Controller(hwnd)` → callback
3. Store `ICoreWebView2Controller` + `ICoreWebView2`, resize to window.
4. `NavigateToString(baseHtml)` — load the bundled HTML template once.

All subsequent updates use `ExecuteScript` to push new content, not full
reloads.

### Live Update {#architecture.live-update}

Hook in `beNotified()`:

| Notification | Action |
|---|---|
| `SCN_MODIFIED` (insert/delete) | Debounce 300ms, then push content |
| `NPPN_BUFFERACTIVATED` | Immediate push (tab switch) |
| `NPPN_FILEBEFORECLOSE` | Clear preview |

Debounce via `SetTimer` / `KillTimer` on the preview window — no threads
needed.

Content retrieval: `SendMessage(sciHwnd, SCI_GETTEXT, len, buf)` — direct
Scintilla message, no file I/O.

### Content Push {#architecture.content-push}

```cpp
// Escape raw markdown for JS string, then:
webView->ExecuteScript(
    L"renderMarkdown(`" + escapedMarkdown + L"`)",
    nullptr
);
```

JS side:
```js
function renderMarkdown(md) {
  document.getElementById('content').innerHTML = marked.parse(md);
}
```

## Theming {#theming}

Theming is deferred until the plugin reaches operational state. This section
defines the mechanism so the structure is ready when look-and-feel decisions
are made.

### Approach {#theming.approach}

All visual customization lives in `res/preview.html`. marked.js outputs
standard HTML tags — theming is pure CSS against those tags. No recompilation
needed for style changes.

Two-layer CSS structure:

```
/* Layer 1: base reset + layout (rarely touched) */
/* Layer 2: theme — fonts, colors, spacing (where customization lives) */
```

### Extension Points {#theming.extension-points}

| Selector target | Controls |
|---|---|
| `body` | Background, base font, line height |
| `h1`–`h6` | Heading fonts, sizes, colors, borders |
| `p`, `li` | Body text, spacing |
| `code`, `pre` | Inline and block code appearance |
| `blockquote` | Quote styling |
| `table`, `th`, `td` | Table appearance |
| `a` | Link color and decoration |
| `img` | Max-width, centering |

### Custom Classes via marked.js Renderer {#theming.custom-renderer}

If standard tag selectors are not granular enough, a custom marked.js renderer
adds CSS classes to any element without touching the markdown source:

```js
const renderer = new marked.Renderer();
renderer.heading = ({ text, depth }) =>
  `<h${depth} class="nmd-h${depth}">${text}</h${depth}>`;
marked.use({ renderer });
```

This is the hook for arbitrary "custom selectors" — define the class in CSS,
attach it in the renderer.

### User-Editable Theme File (Optional) {#theming.user-file}

If end-user theming is needed later, load `preview.css` from NPP's plugin
config dir at WebView2 init time. The inline CSS in `preview.html` acts as
the fallback. Add this only when the look-and-feel is finalized — no point
designing the file format before the content is known.

### Status {#theming.status}

> **Deferred.** Placeholder CSS ships with Phase 4 (readable defaults only).
> Custom theme defined after plugin reaches operational state.

---

## Build Phases {#phases}

### Phase 1 — Scaffold {#phases.scaffold} ✅

1. ~~Clone `notepad-plus-plus/npp-plugin-template`.~~ Headers written from spec directly into `include/`.
2. All six exports implemented in `src/Plugin.cpp` + `src/Plugin.h`.
3. "Toggle Preview" menu command wired via `src/Commands.cpp`.

### Phase 2 — Floating Window {#phases.window} ✅

1. Window class registered in `PreviewWindow::RegisterClass`; `CreateWindowEx` with `WS_EX_TOOLWINDOW` and NPP as owner.
2. `PreviewWindow::Toggle` shows/hides the window.
3. Position persistence deferred to Phase 6.

### Phase 3 — WebView2 {#phases.webview2} ✅

1. WebView2 guarded behind `#ifdef HAVE_WEBVIEW2` — add SDK and define to activate.
2. Async init chain in `WM_CREATE`: environment → controller → `NavigateToString(BASE_HTML)`.
3. `BASE_HTML` raw string literal in `PreviewWindow.cpp` carries the full HTML template.

### Phase 4 — Markdown Rendering {#phases.markdown} ✅

1. `marked.js` v15.0.12 (~40KB minified) embedded inline in `res/preview.html`.
2. `PushContent(std::wstring)` implemented: escapes backtick/backslash/`$`, calls `ExecuteScript`.
3. GitHub-style CSS in `res/preview.html`; theming deferred per plan.

### Phase 5 — Live Update {#phases.live-update} ✅

1. `beNotified` dispatches `SCN_MODIFIED` and `NPPN_BUFFERACTIVATED` in `src/Plugin.cpp`.
2. 300ms debounce via `SetTimer`/`KillTimer` on `TIMER_DEBOUNCE`.
3. `GetCurrentMarkdown()` reads active Scintilla via `NPPM_GETCURRENTSCINTILLA` + `SCI_GETTEXT`, converts UTF-8 → `wstring`.
4. `PushContent` called on timer fire, buffer activate, and cleared on file close.

### Phase 6 — Polish {#phases.polish}

1. Persist window size + position.
2. Sync scroll position (optional — JS `IntersectionObserver` approach).
3. Handle large files gracefully (preview cap or lazy render).

## Dependencies {#dependencies}

| Dep | Source | Notes |
|---|---|---|
| NPP Plugin API headers | `notepad-plus-plus/npp-plugin-template` | Headers only |
| WebView2 SDK | vcpkg `microsoft-webview2` or NuGet | Needs Edge runtime on target machine |
| `marked.js` | [marked.js releases](https://github.com/markedjs/marked) | Bundle minified, inline in HTML |

Edge runtime ships with Windows 11 and modern Win10. Edge not present =
WebView2 init fails gracefully; show a message in the window.

## File Layout (Target) {#layout}

```
nmd/
├── src/
│   ├── Plugin.cpp
│   ├── Plugin.h
│   ├── PreviewWindow.cpp
│   ├── PreviewWindow.h
│   └── Commands.cpp
├── res/
│   └── preview.html          (marked.js + CSS inline)
├── NMD.vcxproj
└── NMD.sln
```
