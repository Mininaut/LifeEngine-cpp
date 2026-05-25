#include "lifeengine/gui/application.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    return lifeengine::gui::runNativeGui(SW_SHOWDEFAULT);
#else
    return lifeengine::gui::runNativeGui(0);
#endif
}
