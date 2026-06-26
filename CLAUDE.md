# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Constraints

- **Do not change any theme, styling, or CSS** (including `BASE_HTML`, `preview.html`, Shiki theme config, or any inline styles) without explicit user permission.

## Build

**This is a Windows-only project. Compilation requires Visual Studio 2022 on Windows (not WSL).**

Open `NMD.sln` in Visual Studio 2022 and build — or from a Developer Command Prompt:

```
msbuild NMD.sln /p:Configuration=Release /p:Platform=x64
```

Output is `x64/Release/NMD.dll`. Copy the DLL to `%APPDATA%\Notepad++\plugins\NMD\NMD.dll` to install.

## Activating WebView2

WebView2 code is gated behind `#ifdef HAVE_WEBVIEW2` and compiles as no-ops without the SDK. To enable:

1. Install the NuGet package `Microsoft.Web.WebView2` into the project.
2. Add `HAVE_WEBVIEW2` to `<PreprocessorDefinitions>` in both Debug and Release `ItemDefinitionGroup` blocks in `NMD.vcxproj`.
3. Add the SDK's `include/` and `lib/` paths to `<AdditionalIncludeDirectories>` and `<AdditionalLibraryDirectories>`.

## Architecture

The plugin is a Win32 DLL. The data flow is:

```
NPP notification (beNotified)
  └─ Plugin.cpp dispatches to PreviewWindow static methods
       ├─ OnModified()       → debounce timer (300ms SetTimer/KillTimer)
       ├─ OnBufferActivated() → immediate PushContent()
       └─ OnFileClosed()    → PushContent(L"")

PushContent(wstring)
  └─ escapes backtick / backslash / $ for JS template literal
  └─ ExecuteScript("renderMarkdown(`...`)")
       └─ WebView2 calls renderMarkdown() in preview.html
            └─ marked.parse(md) → innerHTML of #content div
```

### Key files

| File | Role |
|---|---|
| `src/Plugin.cpp` | DLL entry point; all 6 mandatory NPP exports; `beNotified` dispatcher |
| `src/PreviewWindow.cpp` | Everything visual: window creation, WebView2 async init, PushContent, GetCurrentMarkdown, local asset serving |
| `src/Commands.cpp` | Thin shim — `CmdTogglePreview` calls `PreviewWindow::Toggle` |
| `include/PluginInterface.h` | NPP ABI types (`NppData`, `FuncItem`, `ShortcutKey`) |
| `include/Notepad_plus_msgs.h` | NPP message constants + `SCNotification` struct |
| `res/preview.html` | Light-theme reference copy of the preview page; keep in sync with `BASE_HTML` |
| `res/shiki.bundle.js` | Shiki v1 browser bundle (ESM, 1.6 MB) — built from `shiki` + 16 languages + github-dark/light themes |
| `res/onig.wasm` | Oniguruma WASM regex engine (456 KB) used by Shiki |
| `res/resource.h` | Resource IDs: `IDI_PREVIEW`, `IDR_SHIKI_JS`, `IDR_ONIG_WASM` |
| `NMD.rc` | Windows resources: icon + Shiki JS + WASM as `RCDATA` |
| `NMD.def` | Linker exports for the 6 NPP entry points |

### Globals

`g_nppData` and `g_hInstance` are declared in `Plugin.h` and defined in `Plugin.cpp`. All code that needs NPP or Scintilla handles goes through these.

### WebView2 lifetime

Init is async: `WM_CREATE` calls `InitWebView2()` → two COM callbacks → environment + controller + webView stored as static raw pointers (`s_env`, `s_controller`, `s_webView`). All three released in `WM_DESTROY`. The window procedure is in `PreviewWindow::WndProc` (static, registered as `lpfnWndProc`).

### Local asset serving

`InitWebView2()` registers a `WebResourceRequested` filter for `https://nmd-local/*`. Requests to `https://nmd-local/shiki.js` and `https://nmd-local/onig.wasm` are served directly from the DLL's `RCDATA` resources via `FindResource`/`LoadResource`/`SHCreateMemStream`. Responses include `Access-Control-Allow-Origin: *` — required because `NavigateToString` gives the page a null origin and CORS applies to all cross-origin imports/fetches.

### Syntax highlighting

Shiki highlights only fenced code blocks with a language tag (`pre > code[class*="language-"]`). Inline code is never touched. The `__shikiHighlight()` JS function runs after `marked.parse()` in `renderMarkdown()`, and again once the Shiki module finishes async loading. `BASE_HTML` uses the `github-dark` theme; `preview.html` uses `github-light`.

### Rebuilding the Shiki bundle

If languages or themes need updating, rebuild with:

```
mkdir /tmp/shiki-build && cd /tmp/shiki-build
npm init -y && npm install shiki @shikijs/core @shikijs/engine-oniguruma esbuild
# edit entry.mjs as needed
node_modules/.bin/esbuild entry.mjs --bundle --format=esm --platform=browser --minify --outfile=shiki.bundle.js
cp shiki.bundle.js <repo>/res/
cp node_modules/shiki/dist/onig.wasm <repo>/res/
```

### HTML template

`BASE_HTML` in `PreviewWindow.cpp` is a `LR"html(...)html"` wide raw string literal containing the full preview page (marked.js embedded, CSS inline, `renderMarkdown()` function, Shiki module script). `res/preview.html` is the canonical source; keep them in sync when editing the template. marked.js is embedded via the Python snippet at the bottom of `build-plan.md`.

## Remaining work (Phase 6)

Window position/size persistence (`GetPluginsConfigDir` → write/read an INI), optional scroll sync via `IntersectionObserver`, large-file preview cap.
