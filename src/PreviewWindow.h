#pragma once
#include <windows.h>
#include <string>

#ifdef HAVE_WEBVIEW2
#include <WebView2.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;
#endif

class PreviewWindow {
public:
    static void Toggle(HWND nppHwnd, HINSTANCE hInst);
    static void OnModified();
    static void OnBufferActivated();
    static void OnFileClosed();
    static void OnScrolled();

    static void PushContent(const std::wstring& markdown);

private:
    static HWND      s_hwnd;
    static HINSTANCE s_hInst;
    static HWND      s_nppHwnd;

#ifdef HAVE_WEBVIEW2
    static ICoreWebView2Controller* s_controller;
    static ICoreWebView2*           s_webView;
    static ICoreWebView2Environment* s_env;
#endif

    static void Create(HWND nppHwnd, HINSTANCE hInst);
    static void Show();
    static void Hide();
    static bool IsVisible();
    static void InitWebView2();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static void RegisterClass(HINSTANCE hInst);
    static std::wstring GetCurrentMarkdown();
};
