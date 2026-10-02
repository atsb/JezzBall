#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "win32.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    win32::Application app(hInstance);
    return app.run();
}
