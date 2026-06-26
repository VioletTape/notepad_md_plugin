#pragma once
#include <windows.h>

typedef UINT_PTR uptr_t;
typedef INT_PTR  sptr_t;

#define NPPMSG  (WM_USER + 1000)

#define NPPM_GETPLUGINSCONFIGDIR    (NPPMSG + 97)
#define NPPM_GETCURRENTSCINTILLA    (NPPMSG + 4)
#define NPPM_GETFULLCURRENTPATH     (NPPMSG + 16)
#define NPPM_GETCURRENTBUFFERID     (NPPMSG + 60)
#define NPPM_GETFULLPATHFROMBUFFERID (NPPMSG + 58)
#define NPPM_DOOPEN                 (NPPMSG + 77)

// Notifications
#define NPPN_FIRST                  1000
#define NPPN_READY                  (NPPN_FIRST + 1)
#define NPPN_TBMODIFICATION         (NPPN_FIRST + 2)
#define NPPN_FILEBEFORECLOSE        (NPPN_FIRST + 3)
#define NPPN_FILEOPENED             (NPPN_FIRST + 4)
#define NPPN_FILECLOSED             (NPPN_FIRST + 5)
#define NPPN_FILEBEFOREOPEN         (NPPN_FIRST + 6)
#define NPPN_FILEBEFORESAVE         (NPPN_FIRST + 7)
#define NPPN_FILESAVED              (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN               (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED        (NPPN_FIRST + 10)
#define NPPN_LANGCHANGED            (NPPN_FIRST + 11)
#define NPPN_WORDSTYLESUPDATED      (NPPN_FIRST + 12)
#define NPPN_SHORTCUTREMAPPED       (NPPN_FIRST + 13)
#define NPPN_FILEBEFORELOAD         (NPPN_FIRST + 14)
#define NPPN_FILELOADFAILED         (NPPN_FIRST + 15)
#define NPPN_READONLYCHANGED        (NPPN_FIRST + 16)
#define NPPN_DOCORDERCHANGED        (NPPN_FIRST + 17)
#define NPPN_SNAPSHOTDIRTYFILELOADED (NPPN_FIRST + 18)
#define NPPN_BEFORESHUTDOWN         (NPPN_FIRST + 19)
#define NPPN_CANCELSHUTDOWN         (NPPN_FIRST + 20)
#define NPPN_FILEBEFORERENAME       (NPPN_FIRST + 21)
#define NPPN_FILERENAMECANCEL       (NPPN_FIRST + 22)
#define NPPN_FILERENAMED            (NPPN_FIRST + 23)
#define NPPN_FILEBEFOREDELETE       (NPPN_FIRST + 24)
#define NPPN_FILEDELETEFAILED       (NPPN_FIRST + 25)
#define NPPN_FILEDELETED            (NPPN_FIRST + 26)

#define NPPM_ADDTOOLBARICON_FORDARKMODE (NPPMSG + 101)

struct toolbarIconsWithDarkMode {
    HBITMAP hToolbarBmp;
    HICON   hToolbarIcon;
    HICON   hToolbarIconDarkMode;
};

// SCN_MODIFIED mask flags
#define SC_MOD_INSERTTEXT           0x1
#define SC_MOD_DELETETEXT           0x2

struct SCNotification {
    NMHDR       nmhdr;
    int         position;
    int         ch;
    int         modifiers;
    int         modificationType;
    const char *text;
    int         length;
    int         linesAdded;
    int         message;
    uptr_t      wParam;
    sptr_t      lParam;
    int         line;
    int         foldLevelNow;
    int         foldLevelPrev;
    int         margin;
    int         listType;
    int         x;
    int         y;
    int         token;
    int         annotationLinesAdded;
    int         updated;
};

#define SCN_UPDATEUI                2007
#define SCI_GETFIRSTVISIBLELINE     2152
#define SCI_SETFIRSTVISIBLELINE     2613
