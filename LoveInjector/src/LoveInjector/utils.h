#pragma once

#include "../imgui/imgui-1.89.1/imgui.h"
#include <windows.h>
#include <Winternl.h>
#include <string.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sstream>

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

__inline std::vector<std::pair<std::string, DWORD>> GetProcessInfo()
{
    std::vector<std::pair<std::string, DWORD>> process_info;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do 
        {
            
            std::stringstream ss;
            ss << pe.szExeFile << " (ID: " << pe.th32ProcessID << ")";
            std::string info = ss.str();


            // Check if the process is running on x86 or x64 architecture
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (hProcess != NULL) {
                BOOL isWow64 = FALSE;
                if (IsWow64Process(hProcess, &isWow64)) {
                    if (isWow64) {
                        info += "     [32-bit]";
                        process_info.push_back(std::make_pair(info, pe.th32ProcessID));
                    }
                    else {
                        info += "     [64-bit]";
                        process_info.push_back(std::make_pair(info, pe.th32ProcessID));
                    }
                }
                CloseHandle(hProcess);
            }

        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);

    return process_info;
}

__inline bool ManualMapMethod(DWORD processId, const wchar_t* dllPath)
{

}

// Function to inject a DLL into a specified process
__inline bool LoadLibraryAMethod(DWORD process_id, const char* dll_path)
{
    // Open the target process
    HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
    if (process_handle == NULL)
    {
        printf("Failed to open process!\n");
        return false;
    }

    // Allocate memory for the DLL path in the target process's address space
    LPVOID remote_memory = VirtualAllocEx(process_handle, NULL, strlen(dll_path) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (remote_memory == NULL)
    {
        CloseHandle(process_handle);
        printf("Failed to allocate memory!\n");
        return false;
    }

    // Write the DLL path to the allocated memory in the target process
    SIZE_T bytes_written;
    if (!WriteProcessMemory(process_handle, remote_memory, dll_path, strlen(dll_path) + 1, &bytes_written))
    {
        VirtualFreeEx(process_handle, remote_memory, strlen(dll_path) + 1, MEM_RELEASE);
        CloseHandle(process_handle);
        return false;
    }

    // Get the address of the LoadLibraryA function in the target process
    LPTHREAD_START_ROUTINE load_library_address = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    if (load_library_address == NULL)
    {
        VirtualFreeEx(process_handle, remote_memory, strlen(dll_path) + 1, MEM_RELEASE);
        CloseHandle(process_handle);
        return false;
    }

    // Create a remote thread in the target process to load the DLL
    HANDLE remote_thread = CreateRemoteThread(process_handle, NULL, 0, load_library_address, remote_memory, 0, NULL);
    if (remote_thread == NULL)
    {
        VirtualFreeEx(process_handle, remote_memory, strlen(dll_path) + 1, MEM_RELEASE);
        CloseHandle(process_handle);
        return false;
    }

    // Wait for the remote thread to finish execution
    WaitForSingleObject(remote_thread, INFINITE);

    // Free the allocated memory in the target process
    VirtualFreeEx(process_handle, remote_memory, strlen(dll_path) + 1, MEM_RELEASE);

    // Close handles
    CloseHandle(remote_thread);
    CloseHandle(process_handle);

    return true;
}
