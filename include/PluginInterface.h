#pragma once
#include <windows.h>
#include <tchar.h>

const int nbChar = 64;

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

struct ShortcutKey {
    bool _isAlt;
    bool _isCtrl;
    bool _isShift;
    UCHAR _key;
};

typedef void (*PFUNCVOID)();

struct FuncItem {
    TCHAR _itemName[nbChar];
    PFUNCVOID _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey *_pShKey;
};
