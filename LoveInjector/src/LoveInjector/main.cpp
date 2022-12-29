#include "ccpch.h"
#include "gui/gui.h"

HWND window = nullptr;

int __stdcall wWinMain(
    HINSTANCE instace,
    HINSTANCE previousInstance,
    PWSTR arguments,
    int commandShow)
{
    gui::CreateHWindow(xorstr_("Menu"), xorstr_("Menu Class"));
    gui::CreateDevice();
    gui::CreateImGui();

    while (gui::isRunning)
    {
        gui::BeginRender();
        gui::Render();
        gui::EndRender();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    gui::DestroyImGui();
    gui::DestoryDevice();
    gui::DestroyHWindow();



    return EXIT_SUCCESS;
}