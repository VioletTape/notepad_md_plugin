#include "PreviewWindow.h"
#include "Plugin.h"
#include "../include/Scintilla.h"
#include "../res/resource.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <vector>
#pragma comment(lib, "shlwapi.lib")

#define PREVIEW_CLASS   L"NMD_PreviewWnd"
#define TIMER_DEBOUNCE  1
#define TIMER_POLL      2
#define DEBOUNCE_MS     300
#define POLL_MS         500


// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

HWND      PreviewWindow::s_hwnd    = nullptr;
HINSTANCE PreviewWindow::s_hInst  = nullptr;
HWND      PreviewWindow::s_nppHwnd = nullptr;

#ifdef HAVE_WEBVIEW2
ICoreWebView2Controller*  PreviewWindow::s_controller = nullptr;
ICoreWebView2*            PreviewWindow::s_webView    = nullptr;
ICoreWebView2Environment* PreviewWindow::s_env        = nullptr;
#endif

static bool     s_classRegistered = false;
static std::wstring s_lastContent;
static std::wstring s_currentFileDir;
static int      s_lastScrollLine  = -1;
static bool     s_syncFromViewer  = false;
static std::wstring s_iniPath;

// ---------------------------------------------------------------------------
// Debug log — appends a line to NMD.log next to NMD.ini
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Window class registration
// ---------------------------------------------------------------------------

void PreviewWindow::RegisterClass(HINSTANCE hInst) {
    if (s_classRegistered) return;

    WNDCLASSEX wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = PREVIEW_CLASS;
    RegisterClassEx(&wc);

    s_classRegistered = true;
}

// ---------------------------------------------------------------------------
// Config helpers
// ---------------------------------------------------------------------------

static void SaveZoom(double zoom) {
    if (s_iniPath.empty()) return;
    wchar_t buf[32];
    swprintf_s(buf, L"%.4f", zoom);
    WritePrivateProfileString(L"Window", L"Zoom", buf, s_iniPath.c_str());
}

static void SaveWindowPos(HWND hwnd) {
    if (s_iniPath.empty()) return;
    RECT r = {};
    GetWindowRect(hwnd, &r);
    const wchar_t* sec = L"Window";
    auto wi = [&](const wchar_t* k, int v) {
        WritePrivateProfileString(sec, k, std::to_wstring(v).c_str(), s_iniPath.c_str());
    };
    wi(L"X", r.left);
    wi(L"Y", r.top);
    wi(L"W", r.right - r.left);
    wi(L"H", r.bottom - r.top);
}

// ---------------------------------------------------------------------------
// Window creation
// ---------------------------------------------------------------------------

void PreviewWindow::Create(HWND nppHwnd, HINSTANCE hInst) {
    s_hInst   = hInst;
    s_nppHwnd = nppHwnd;
    RegisterClass(hInst);

    wchar_t appData[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData);
    s_iniPath = std::wstring(appData) + L"\\Notepad++\\plugins\\config\\NMD.ini";

    RECT nr = {};
    GetWindowRect(nppHwnd, &nr);
    const wchar_t* sec = L"Window";
    std::wstring ini = s_iniPath;
    auto ri = [&](const wchar_t* k, int def) {
        return (int)GetPrivateProfileInt(sec, k, def, ini.c_str());
    };
    int x = ri(L"X", nr.right + 10);
    int y = ri(L"Y", nr.top);
    int w = ri(L"W", 800);
    int h = ri(L"H", 600);

    s_hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,
        PREVIEW_CLASS,
        L"Markdown Preview",
        WS_POPUP | WS_CAPTION | WS_SIZEBOX | WS_SYSMENU,
        x, y, w, h,
        nppHwnd, nullptr, hInst, nullptr
    );
}

// ---------------------------------------------------------------------------
// WebView2 initialization
// ---------------------------------------------------------------------------

#ifdef HAVE_WEBVIEW2

void PreviewWindow::InitWebView2() {
    HWND hwnd = s_hwnd;

    wchar_t appData[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
    std::wstring userDataDir = std::wstring(appData) + L"\\Notepad++\\plugins\\NMD\\WebView2";

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataDir.c_str(), nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;

                s_env = env;
                s_env->AddRef();

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) return result;

                            s_controller = controller;
                            s_controller->AddRef();

                            s_controller->get_CoreWebView2(&s_webView);

                            // Serve embedded assets under https://nmd-local/
                            s_webView->AddWebResourceRequestedFilter(
                                L"https://nmd-local/*",
                                COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL
                            );
                            s_webView->add_WebResourceRequested(
                                Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                                        ICoreWebView2WebResourceRequest* req = nullptr;
                                        args->get_Request(&req);
                                        LPWSTR uri = nullptr;
                                        req->get_Uri(&uri);
                                        req->Release();

                                        int resId = 0;
                                        const wchar_t* mime = nullptr;
                                        if (wcscmp(uri, L"https://nmd-local/shiki.js") == 0) {
                                            resId = IDR_SHIKI_JS;
                                            mime  = L"application/javascript";
                                        } else if (wcscmp(uri, L"https://nmd-local/onig.wasm") == 0) {
                                            resId = IDR_ONIG_WASM;
                                            mime  = L"application/wasm";
                                        } else if (wcscmp(uri, L"https://nmd-local/nmd.html") == 0) {
                                            resId = IDR_NMD_HTML;
                                            mime  = L"text/html; charset=utf-8";
                                        }
                                        CoTaskMemFree(uri);

                                        if (resId == 0) return S_OK;

                                        HRSRC   hRes  = FindResource(g_hInstance, MAKEINTRESOURCE(resId), RT_RCDATA);
                                        HGLOBAL hMem  = hRes ? LoadResource(g_hInstance, hRes) : nullptr;
                                        if (!hMem) return S_OK;
                                        const BYTE* data = static_cast<const BYTE*>(LockResource(hMem));
                                        DWORD size = SizeofResource(g_hInstance, hRes);

                                        IStream* stream = SHCreateMemStream(data, size);
                                        if (!stream) return S_OK;

                                        std::wstring headers = std::wstring(L"Content-Type: ") + mime
                                            + L"\r\nAccess-Control-Allow-Origin: *";
                                        ICoreWebView2WebResourceResponse* response = nullptr;
                                        s_env->CreateWebResourceResponse(stream, 200, L"OK", headers.c_str(), &response);
                                        stream->Release();

                                        if (response) {
                                            args->put_Response(response);
                                            response->Release();
                                        }
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            // Fit WebView2 to the client area
                            RECT bounds = {};
                            GetClientRect(hwnd, &bounds);
                            s_controller->put_Bounds(bounds);

                            // Restore saved zoom
                            wchar_t zoomBuf[32] = L"1.0000";
                            GetPrivateProfileString(L"Window", L"Zoom", L"1.0000", zoomBuf, 32, s_iniPath.c_str());
                            double zoom = wcstod(zoomBuf, nullptr);
                            if (zoom > 0.1 && zoom < 10.0)
                                s_controller->put_ZoomFactor(zoom);

                            // Persist zoom whenever user changes it
                            s_controller->add_ZoomFactorChanged(
                                Microsoft::WRL::Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
                                    [](ICoreWebView2Controller* ctrl, IUnknown*) -> HRESULT {
                                        double z = 1.0;
                                        ctrl->get_ZoomFactor(&z);
                                        SaveZoom(z);
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            s_webView->add_NavigationCompleted(
                                Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                        PushContent(GetCurrentMarkdown());
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            s_webView->add_NavigationStarting(
                                Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        LPWSTR uri = nullptr;
                                        args->get_Uri(&uri);
                                        if (!uri) return S_OK;
                                        std::wstring url(uri);
                                        CoTaskMemFree(uri);
                                        // Only block external http/https — JS click handler posts those as messages instead
                                        bool isExternal = (url.substr(0, 7) == L"http://" || url.substr(0, 8) == L"https://")
                                            && url.compare(0, 18, L"https://nmd-local/") != 0;
                                        if (isExternal) args->put_Cancel(TRUE);
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            s_webView->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR msg = nullptr;
                                        args->TryGetWebMessageAsString(&msg);
                                        if (!msg) return S_OK;
                                        std::wstring msgStr(msg);
                                        CoTaskMemFree(msg);

                                        if (msgStr.substr(0, 5) == L"open:") {
                                            std::wstring target = msgStr.substr(5);
                                            // Relative links get resolved to https://nmd-local/<path> by the browser
                                            static const std::wstring kLocalBase = L"https://nmd-local/";
                                            if (target.substr(0, kLocalBase.size()) == kLocalBase)
                                                target = target.substr(kLocalBase.size());
                                            else if (target.substr(0, 7) == L"http://" || target.substr(0, 8) == L"https://" || target.substr(0, 7) == L"mailto:") {
                                                ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                                return S_OK;
                                            }
                                            for (auto& ch : target) if (ch == L'/') ch = L'\\';
                                            wchar_t fullPath[MAX_PATH] = {};
                                            PathCombineW(fullPath, s_currentFileDir.c_str(), target.c_str());
                                            DWORD attr = GetFileAttributesW(fullPath);
                                            if (attr != INVALID_FILE_ATTRIBUTES)
                                                SendMessage(g_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fullPath);
                                            return S_OK;
                                        }

                                        int line = _wtoi(msgStr.c_str());
                                        int which = 0;
                                        SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
                                        HWND sci = (which == 0) ? g_nppData._scintillaMainHandle
                                                                 : g_nppData._scintillaSecondHandle;
                                        s_lastScrollLine = line;
                                        s_syncFromViewer = true;
                                        SendMessage(sci, SCI_SETFIRSTVISIBLELINE, (WPARAM)line, 0);
                                        s_syncFromViewer = false;
                                        // Read back actual position in case NPP corrected for caret visibility
                                        s_lastScrollLine = (int)SendMessage(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            s_webView->Navigate(L"https://nmd-local/nmd.html");
                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );
}

#else

// ponytail: stubs compile cleanly when HAVE_WEBVIEW2 not defined
void PreviewWindow::InitWebView2() {}

#endif // HAVE_WEBVIEW2

// ---------------------------------------------------------------------------
// PushContent — escape markdown and call renderMarkdown() in the WebView
// ---------------------------------------------------------------------------

#ifdef HAVE_WEBVIEW2

void PreviewWindow::PushContent(const std::wstring& markdown) {
    if (!s_webView) return;

    // Escape characters that would break a JS template literal:
    //   backslash  ->  \\
    //   backtick   ->  \`
    //   dollar     ->  \$
    std::wstring escaped;
    escaped.reserve(markdown.size());
    for (wchar_t ch : markdown) {
        if (ch == L'\\') {
            escaped += L'\\';
            escaped += L'\\';
        } else if (ch == L'`') {
            escaped += L'\\';
            escaped += L'`';
        } else if (ch == L'$') {
            escaped += L'\\';
            escaped += L'$';
        } else {
            escaped += ch;
        }
    }

    s_lastScrollLine = -1; // force re-sync after content reload
    std::wstring script = L"renderMarkdown(`" + escaped + L"`)";
    s_webView->ExecuteScript(script.c_str(), nullptr);
}

#else

// ponytail: stubs compile cleanly when HAVE_WEBVIEW2 not defined
void PreviewWindow::PushContent(const std::wstring&) {}

#endif // HAVE_WEBVIEW2

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void PreviewWindow::Toggle(HWND nppHwnd, HINSTANCE hInst) {
    if (!s_hwnd)
        Create(nppHwnd, hInst);

    if (IsVisible())
        Hide();
    else
        Show();
}

static void UpdateCurrentFileDir(UINT_PTR bufferID = 0) {
    if (!bufferID)
        bufferID = (UINT_PTR)SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
    wchar_t path[MAX_PATH] = {};
    SendMessage(g_nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, bufferID, (LPARAM)path);
    PathRemoveFileSpecW(path);
    s_currentFileDir = path;
}

void PreviewWindow::Show() {
    UpdateCurrentFileDir();
    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
    SetTimer(s_hwnd, TIMER_POLL, POLL_MS, nullptr);
}

void PreviewWindow::Hide() {
    KillTimer(s_hwnd, TIMER_POLL);
    ShowWindow(s_hwnd, SW_HIDE);
}

bool PreviewWindow::IsVisible() {
    return s_hwnd && IsWindowVisible(s_hwnd);
}

void PreviewWindow::OnModified() {
    if (!IsVisible()) return;
    KillTimer(s_hwnd, TIMER_DEBOUNCE);
    SetTimer(s_hwnd, TIMER_DEBOUNCE, DEBOUNCE_MS, nullptr);
}

void PreviewWindow::OnBufferActivated(UINT_PTR bufferID) {
    UpdateCurrentFileDir(bufferID);  // always, even when hidden
    if (!IsVisible()) return;
    s_lastContent.clear();  // force refresh on tab switch
}

void PreviewWindow::OnFileClosed() {
    if (!IsVisible()) return;
    PushContent(L"");
}

void PreviewWindow::OnScrolled() {
#ifdef HAVE_WEBVIEW2
    if (!s_webView || s_syncFromViewer) return;
    int which = 0;
    SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    HWND sciHwnd = (which == 0) ? g_nppData._scintillaMainHandle
                                 : g_nppData._scintillaSecondHandle;
    int line = (int)SendMessage(sciHwnd, SCI_GETFIRSTVISIBLELINE, 0, 0);
    if (line == s_lastScrollLine) return;
    s_lastScrollLine = line;
    std::wstring script = L"scrollToLine(" + std::to_wstring(line) + L")";
    s_webView->ExecuteScript(script.c_str(), nullptr);
#endif
}

// ---------------------------------------------------------------------------
// GetCurrentMarkdown — reads the active Scintilla buffer as a wstring
// ---------------------------------------------------------------------------

std::wstring PreviewWindow::GetCurrentMarkdown() {
    int which = 0;
    SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    HWND sciHwnd = (which == 0) ? g_nppData._scintillaMainHandle
                                 : g_nppData._scintillaSecondHandle;

    int len = (int)SendMessage(sciHwnd, SCI_GETTEXTLENGTH, 0, 0);
    if (len <= 0) return L"";

    std::vector<char> buf(len + 1, 0);
    SendMessage(sciHwnd, SCI_GETTEXT, len + 1, (LPARAM)buf.data());

    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring result(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, &result[0], wlen);
    return result;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK PreviewWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            s_hwnd = hwnd;
            InitWebView2();
            return 0;

        case WM_TIMER:
            if (wp == TIMER_POLL) {
                std::wstring cur = GetCurrentMarkdown();
                if (cur != s_lastContent) {
                    s_lastContent = cur;
                    PushContent(cur);
                }
            }
            return 0;

        case WM_MOVE:
            SaveWindowPos(hwnd);
            return 0;

        case WM_SIZE: {
            SaveWindowPos(hwnd);
#ifdef HAVE_WEBVIEW2
            if (s_controller) {
                RECT bounds = {};
                GetClientRect(hwnd, &bounds);
                s_controller->put_Bounds(bounds);
            }
#endif
            return 0;
        }

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
#ifdef HAVE_WEBVIEW2
            if (s_webView) {
                s_webView->Release();
                s_webView = nullptr;
            }
            if (s_controller) {
                s_controller->Release();
                s_controller = nullptr;
            }
            if (s_env) {
                s_env->Release();
                s_env = nullptr;
            }
#endif
            s_hwnd = nullptr;
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}
