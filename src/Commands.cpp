#include "Plugin.h"
#include "PreviewWindow.h"
#include "Commands.h"

void CmdTogglePreview() {
    PreviewWindow::Toggle(g_nppData._nppHandle, g_hInstance);
}
