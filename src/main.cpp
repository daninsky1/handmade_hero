#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

std::string version();

int WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    char unsigned test;
    char unsigned* p_test;
    p_test = &test;

    // x86 overflows wrap around numbers
    test = 255;
    test = test + 1;
}

std::string version()
{
    std::stringstream ss;
    ss << "Handmade Hero VERSION "
       << HANDMADE_HERO_VERSION_MAJOR << '.'
       << HANDMADE_HERO_VERSION_MINOR << '.'
       << HANDMADE_HERO_VERSION_PATCH << std::endl;
    return ss.str();
}