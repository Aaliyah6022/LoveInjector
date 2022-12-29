#pragma once

#include "../imgui/imgui-1.89.1/imgui.h"
#include <windows.h>
#include <string.h>
#include <tlhelp32.h>
#include <psapi.h>

#define MAX_PROCESSES 1024

__inline void ShowFileOpenDialog(char* szFile, int nMaxFile, const char* szFilter)
{
    static bool showDialog = false;

    if (ImGui::Button("Open File..."))
    {
        showDialog = true;
    }

    if (showDialog)
    {
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = nMaxFile;
        ofn.lpstrFilter = szFilter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            showDialog = false;
        }
    }
}

__inline std::vector<std::string> GetProcessList()
{
    std::vector<std::string> process_names;
    DWORD parent_pid = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 process;
        process.dwSize = sizeof(process);
        if (Process32First(snapshot, &process))
        {
            do
            {
                if (process.th32ProcessID != process.th32ParentProcessID)
                {
                    std::string process_name = process.szExeFile;
                    process_name += " (" + std::to_string(process.th32ProcessID) + ")";
                    process_names.push_back(process_name);
                }
            } while (Process32Next(snapshot, &process));
        }
        CloseHandle(snapshot);
    }

    return process_names;
}
