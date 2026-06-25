# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
| `src/PreviewWindow.cpp` | Everything visual: window creation, WebView2 async init, PushContent, GetCurrentMarkdown |
| `src/Commands.cpp` | Thin shim — `CmdTogglePreview` calls `PreviewWindow::Toggle` |
| `include/PluginInterface.h` | NPP ABI types (`NppData`, `FuncItem`, `ShortcutKey`) |
| `include/Notepad_plus_msgs.h` | NPP message constants + `SCNotification` struct |
| `res/preview.html` | Rendered in WebView2; contains marked.js v15 inline + GitHub-style CSS |
| `NMD.def` | Linker exports for the 6 NPP entry points |

### Globals

`g_nppData` and `g_hInstance` are declared in `Plugin.h` and defined in `Plugin.cpp`. All code that needs NPP or Scintilla handles goes through these.

### WebView2 lifetime

Init is async: `WM_CREATE` calls `InitWebView2()` → two COM callbacks → controller + webView stored as static raw pointers. Released in `WM_DESTROY`. The window procedure is in `PreviewWindow::WndProc` (static, registered as `lpfnWndProc`).

### HTML template

`BASE_HTML` in `PreviewWindow.cpp` is a `LR"html(...)html"` wide raw string literal containing the full preview page (marked.js embedded, CSS inline, `renderMarkdown()` function). `res/preview.html` is the canonical source; keep them in sync when editing the template. marked.js is embedded via the Python snippet at the bottom of `build-plan.md`.

## Remaining work (Phase 6)

Window position/size persistence (`GetPluginsConfigDir` → write/read an INI), optional scroll sync via `IntersectionObserver`, large-file preview cap.
