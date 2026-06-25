#include "Plugin.h"
#include "../include/Scintilla.h"
#include "../res/resource.h"
#include "PreviewWindow.h"
#include "Commands.h"

NppData   g_nppData   = {};
HINSTANCE g_hInstance = nullptr;

static FuncItem g_funcs[1];

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        g_hInstance = hModule;
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}

extern "C" __declspec(dllexport) const TCHAR* getName() {
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) void setInfo(NppData data) {
    g_nppData = data;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* count) {
    lstrcpy(g_funcs[0]._itemName, L"Toggle Preview");
    g_funcs[0]._pFunc        = CmdTogglePreview;
    g_funcs[0]._cmdID        = 0;
    g_funcs[0]._init2Check   = false;
    g_funcs[0]._pShKey       = nullptr;
    *count = 1;
    return g_funcs;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notif) {
    switch (notif->nmhdr.code) {
        case NPPN_TBMODIFICATION: {
            HICON hIcon = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_PREVIEW),
                                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
            // Build a 16x16 HBITMAP so NPP's classic bitmap toolbar mode works too
            HDC hdcScr = GetDC(NULL);
            HDC hdcMem = CreateCompatibleDC(hdcScr);
            HBITMAP hBmp = CreateCompatibleBitmap(hdcScr, 16, 16);
            HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
            RECT rc = {0, 0, 16, 16};
            FillRect(hdcMem, &rc, GetSysColorBrush(COLOR_BTNFACE));
            DrawIconEx(hdcMem, 0, 0, hIcon, 16, 16, 0, NULL, DI_NORMAL);
            SelectObject(hdcMem, hOld);
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScr);

            toolbarIconsWithDarkMode tbIcons = {};
            tbIcons.hToolbarBmp          = hBmp;
            tbIcons.hToolbarIcon         = hIcon;
            tbIcons.hToolbarIconDarkMode = hIcon;
            ::SendMessage(g_nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE,
                          (WPARAM)g_funcs[0]._cmdID, (LPARAM)&tbIcons);
            break;
        }
        case NPPN_BUFFERACTIVATED:
            PreviewWindow::OnBufferActivated();
            break;
        case NPPN_FILEBEFORECLOSE:
            PreviewWindow::OnFileClosed();
            break;
        case SCN_MODIFIED:
            if (notif->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
                PreviewWindow::OnModified();
            break;
        default:
            break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {
    return FALSE;
}
