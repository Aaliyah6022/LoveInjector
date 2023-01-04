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

// Utility function to write a chunk of data to a remote process
__inline bool WriteProcessMemory(HANDLE process_handle, void* base_address, void* buffer, size_t size)
{
    SIZE_T bytes_written;
    return WriteProcessMemory(process_handle, base_address, buffer, size, &bytes_written) && bytes_written == size;
}

// Utility function to read a chunk of data from a remote process
__inline bool ReadProcessMemory(HANDLE process_handle, void* base_address, void* buffer, size_t size)
{
    SIZE_T bytes_read;
    return ReadProcessMemory(process_handle, base_address, buffer, size, &bytes_read) && bytes_read == size;
}

// Utility function to find the base address of a module in a remote process
__inline void* GetModuleBaseAddress(HANDLE process_handle, const char* module_name)
{
    // Create a snapshot of the process's modules
    HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetProcessId(process_handle));
    if (snapshot_handle == INVALID_HANDLE_VALUE)
        return NULL;

    // Iterate through the modules in the snapshot
    MODULEENTRY32 module_entry;
    module_entry.dwSize = sizeof(MODULEENTRY32);
    if (!Module32First(snapshot_handle, &module_entry))
    {
        CloseHandle(snapshot_handle);
        return NULL;
    }

    do
    {
        // Check if this is the module we're looking for
        if (strcmp(module_entry.szModule, module_name) == 0)
        {
            // Found it!
            CloseHandle(snapshot_handle);
            return module_entry.modBaseAddr;
        }
    } while (Module32Next(snapshot_handle, &module_entry));

    // Module not found
    CloseHandle(snapshot_handle);
    return NULL;
}

__inline bool ManualMapMethod(DWORD processId, const char* dll_path)
{
    // Open a handle to the target process
    HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process_handle == NULL)
    {
        printf("Failed to open process!\n");
        return false;
    }

    // Read the contents of the DLL file into a buffer
    HANDLE dll_handle = CreateFileA(dll_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dll_handle == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open DLL file!\n");
        CloseHandle(process_handle);
        return false;
    }

    DWORD dll_size = GetFileSize(dll_handle, NULL);
    if (dll_size == INVALID_FILE_SIZE)
    {
        printf("Failed to get DLL file size!\n");
        CloseHandle(dll_handle);
        CloseHandle(process_handle);
        return false;
    }

    BYTE* dll_buffer = new BYTE[dll_size];
    if (!ReadFile(dll_handle, dll_buffer, dll_size, NULL, NULL))
    {
        printf("Failed to read DLL file!\n");
        delete[] dll_buffer;
        CloseHandle(dll_handle);
        CloseHandle(process_handle);
        return false;
    }

    CloseHandle(dll_handle);

    // Allocate memory for the DLL in the target process
    void* remote_memory = VirtualAllocEx(process_handle, NULL, dll_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (remote_memory == NULL)
    {
        printf("Failed to allocate memory in target process!\n");
        delete[] dll_buffer;
        CloseHandle(process_handle);
        return false;
    }

    // Write the DLL to the allocated memory in the target process
    if (!WriteProcessMemory(process_handle, remote_memory, dll_buffer, dll_size))
    {
        printf("Failed to write to memory in target process!\n");
        VirtualFreeEx(process_handle, remote_memory, dll_size, MEM_RELEASE);
        delete[] dll_buffer;
        CloseHandle(process_handle);
        return false;
    }

    delete[] dll_buffer;

    // Get the address of the LoadLibraryA function in the target process
    LPTHREAD_START_ROUTINE load_library_address = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    if (load_library_address == NULL)
    {
        printf("Failed to get address of LoadLibraryA function!\n");
        VirtualFreeEx(process_handle, remote_memory, strlen(dll_path) + 1, MEM_RELEASE);
        CloseHandle(process_handle);
        return false;
    }

    // Create a remote thread in the target process to execute the LoadLibraryA function
    HANDLE thread_handle = CreateRemoteThread(process_handle, NULL, 0, (LPTHREAD_START_ROUTINE)load_library_address, remote_memory, 0, NULL);
    if (thread_handle == NULL)
    {
        printf("Failed to create remote thread in target process!\n");
        VirtualFreeEx(process_handle, remote_memory, dll_size, MEM_RELEASE);
        CloseHandle(process_handle);
        return false;
    }

    // Wait for the remote thread to finish execution
    WaitForSingleObject(thread_handle, INFINITE);

    // Get the exit code of the remote thread
    DWORD exit_code;
    if (!GetExitCodeThread(thread_handle, &exit_code))
    {
        printf("Failed to get exit code of remote thread!\n");
        VirtualFreeEx(process_handle, remote_memory, dll_size, MEM_RELEASE);
        CloseHandle(thread_handle);
        CloseHandle(process_handle);
        return false;
    }

    // Check if the DLL was successfully loaded
    if (exit_code == NULL)
    {
        printf("Failed to load DLL in target process!\n");
        VirtualFreeEx(process_handle, remote_memory, dll_size, MEM_RELEASE);
        CloseHandle(thread_handle);
        CloseHandle(process_handle);
        return false;
    }

    // Clean up
    VirtualFreeEx(process_handle, remote_memory, dll_size, MEM_RELEASE);
    CloseHandle(thread_handle);
    CloseHandle(process_handle);

    return true;

}

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

