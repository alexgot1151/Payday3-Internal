#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>

DWORD GetProcessId(const wchar_t* processName)
{
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"Failed to create process snapshot. Error: " << GetLastError() << std::endl;
        return 0;
    }

    PROCESSENTRY32W processEntry = { 0 };
    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &processEntry))
    {
        do
        {
            if (_wcsicmp(processEntry.szExeFile, processName) == 0)
            {
                processId = processEntry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return processId;
}

bool LaunchGame()
{
    std::wcout << L"Game not found. Launching PAYDAY 3 via Steam..." << std::endl;
    
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };
    
    wchar_t command[] = L"cmd.exe /c start steam://run/1272080";
    
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        std::wcerr << L"Failed to launch game. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    std::wcout << L"Steam launch command sent successfully." << std::endl;
    return true;
}

DWORD WaitForProcess(const wchar_t* processName, int timeoutSeconds)
{
    std::wcout << L"Waiting for game to start (timeout: " << timeoutSeconds << L" seconds)..." << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeoutSeconds);
    
    int dots = 0;
    while (true)
    {
        DWORD processId = GetProcessId(processName);
        if (processId != 0)
        {
            std::wcout << L"\n";
            return processId;
        }
        
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);
        
        if (elapsed >= timeoutDuration)
        {
            std::wcout << L"\n";
            return 0;
        }
        
        if (dots % 4 == 0)
            std::wcout << L"\rWaiting";
        std::wcout << L".";
        dots++;
        std::wcout.flush();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool InjectDLL(DWORD processId, const std::wstring& dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"Failed to open target process. Error: " << GetLastError() << std::endl;
        return false;
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, dllPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string dllPathA(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, dllPath.c_str(), -1, &dllPathA[0], size, nullptr, nullptr);

    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, nullptr, dllPathA.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMemory)
    {
        std::wcerr << L"Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPathA.c_str(), dllPathA.size(), &bytesWritten))
    {
        std::wcerr << L"Failed to write to target process memory. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA");
    if (!pLoadLibrary)
    {
        std::wcerr << L"Failed to get LoadLibraryA address. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMemory, 0, nullptr);
    if (!hThread || hThread == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"Failed to create remote thread. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    if (exitCode == 0)
    {
        std::wcerr << L"LoadLibrary failed in target process." << std::endl;
        return false;
    }

    return true;
}

int main()
{
    const wchar_t* targetProcess = L"PAYDAY3-Win64-Shipping.exe";
    const wchar_t* dllName = L"Payday3-Internal.dll";

    std::wcout << L"========================================" << std::endl;
    std::wcout << L"  PAYDAY 3 DLL Injector" << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Target Process: " << targetProcess << std::endl;
    std::wcout << L"DLL to Inject: " << dllName << std::endl;
    std::wcout << L"========================================" << std::endl << std::endl;

    std::wstring dllPath = std::filesystem::current_path().wstring() + L"\\" + dllName;
    
    if (!std::filesystem::exists(dllPath))
    {
        std::wcerr << L"Error: DLL not found at: " << dllPath << std::endl;
        std::wcout << L"Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::wcout << L"DLL Path: " << dllPath << std::endl << std::endl;

    std::wcout << L"Searching for process: " << targetProcess << std::endl;
    DWORD processId = GetProcessId(targetProcess);

    if (processId == 0)
    {
        std::wcout << L"Process not found." << std::endl << std::endl;
        
        if (!LaunchGame())
        {
            std::wcerr << L"Failed to launch the game." << std::endl;
            std::wcout << L"Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        
        std::wcout << std::endl;
        
        processId = WaitForProcess(targetProcess, 180);
        
        if (processId == 0)
        {
            std::wcerr << L"Timeout: Game process not found after 3 minutes." << std::endl;
            std::wcerr << L"Please make sure Steam is running and try again." << std::endl;
            std::wcout << L"Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
    }

    std::wcout << L"Process found! PID: " << processId << std::endl << std::endl;

    std::wcout << L"Injecting DLL..." << std::endl;
    if (InjectDLL(processId, dllPath))
    {
        std::wcout << L"Success! DLL injected successfully." << std::endl;
    }
    else
    {
        std::wcerr << L"Failed to inject DLL." << std::endl;
        std::wcout << L"Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::wcout << L"\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}
