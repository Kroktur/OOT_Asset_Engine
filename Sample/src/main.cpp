#include <exception>
#include <iostream>
#include <string_view>

#ifdef OOT_WITH_IMGUI
#include "AssetBrowser.h"
#endif

int main(int argc, char** argv)
{
    try
    {
#ifdef OOT_WITH_IMGUI
        if (argc < 2 || std::string_view(argv[1]) != "--console")
        {
            return RunAssetBrowser(argc > 1 && std::string_view(argv[1]) == "--smoke-test" ? 5 : 0);
        }
#endif
        return 0;
    }

    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}