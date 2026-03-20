#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>

DWORD GetProcessId(const char* processName)
{
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to create process snapshot. Error: " << GetLastError() << std::endl;
        return 0;
    }

    PROCESSENTRY32 processEntry = { 0 };
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &processEntry))
    {
        do
        {
            if (_stricmp(processEntry.szExeFile, processName) == 0)
            {
                processId = processEntry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return processId;
}

bool IsModuleLoaded(DWORD processId, const std::string& moduleName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    MODULEENTRY32 moduleEntry = { 0 };
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    bool found = false;
    if (Module32First(snapshot, &moduleEntry))
    {
        do
        {
            if (_stricmp(moduleEntry.szModule, moduleName.c_str()) == 0)
            {
                found = true;
                break;
            }
        } while (Module32Next(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return found;
}

bool LaunchGame()
{
    std::cout << "Game not found. Launching PAYDAY 3 via Steam..." << std::endl;
    
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION pi = { 0 };
    
    char command[] = "cmd.exe /c start \"\" steam://run/1272080";
    
    if (!CreateProcessA(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        std::cerr << "Failed to launch game. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    std::cout << "Steam launch command sent successfully." << std::endl;
    return true;
}

DWORD WaitForProcess(const char* processName, int timeoutSeconds)
{
    std::cout << "Waiting for game to start (timeout: " << timeoutSeconds << " seconds)..." << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeoutSeconds);
    
    int dots = 0;
    while (true)
    {
        DWORD processId = GetProcessId(processName);
        if (processId != 0)
        {
            std::cout << "\n";
            return processId;
        }
        
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);
        
        if (elapsed >= timeoutDuration)
        {
            std::cout << "\n";
            return 0;
        }
        
        if (dots % 4 == 0)
            std::cout << "\rWaiting";
        std::cout << ".";
        dots++;
        std::cout.flush();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool InjectDLL(DWORD processId, const std::string& dllPath)
{
    bool success = false;
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    LPVOID pRemoteMemory = nullptr;

    auto cleanup = [&]()
    {
        if (pRemoteMemory)
        {
            VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
            pRemoteMemory = nullptr;
        }

        if (hThread)
        {
            CloseHandle(hThread);
            hThread = nullptr;
        }

        if (hProcess)
        {
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
    };

    do
    {
        hProcess = OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
            FALSE,
            processId
        );

        if (!hProcess || hProcess == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Failed to open target process. Error: " << GetLastError() << std::endl;
            break;
        }

        const SIZE_T dllPathBytes = dllPath.size() + 1;
        pRemoteMemory = VirtualAllocEx(hProcess, nullptr, dllPathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pRemoteMemory)
        {
            std::cerr << "Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
            break;
        }

        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath.c_str(), dllPathBytes, &bytesWritten) || bytesWritten != dllPathBytes)
        {
            std::cerr << "Failed to write DLL path to target process memory. Error: " << GetLastError() << std::endl;
            break;
        }

        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        if (!hKernel32)
        {
            std::cerr << "Failed to get kernel32.dll handle. Error: " << GetLastError() << std::endl;
            break;
        }

        LPVOID pLoadLibrary = reinterpret_cast<LPVOID>(GetProcAddress(hKernel32, "LoadLibraryA"));
        if (!pLoadLibrary)
        {
            std::cerr << "Failed to get LoadLibraryA address. Error: " << GetLastError() << std::endl;
            break;
        }

        hThread = CreateRemoteThread(
            hProcess,
            nullptr,
            0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibrary),
            pRemoteMemory,
            0,
            nullptr
        );

        if (!hThread || hThread == INVALID_HANDLE_VALUE)
        {
            std::cerr << "Failed to create remote thread. Error: " << GetLastError() << std::endl;
            break;
        }

        DWORD waitResult = WaitForSingleObject(hThread, 30000);
        if (waitResult != WAIT_OBJECT_0)
        {
            std::cerr << "Remote thread did not finish successfully. Wait result: " << waitResult << ", Error: " << GetLastError() << std::endl;
            break;
        }

        DWORD exitCode = 0;
        if (!GetExitCodeThread(hThread, &exitCode))
        {
            std::cerr << "GetExitCodeThread failed. Error: " << GetLastError() << std::endl;
            break;
        }

        if (exitCode == 0)
        {
            const std::string moduleName = std::filesystem::path(dllPath).filename().string();
            if (IsModuleLoaded(processId, moduleName))
            {
                std::cout << "LoadLibraryA returned 0, but module appears loaded: " << moduleName << std::endl;
                success = true;
                break;
            }

            std::cerr << "LoadLibraryA returned NULL in target process." << std::endl;
            std::cerr << "Common causes: missing DLL dependencies, blocked injection, or an invalid DLL path." << std::endl;
            std::cerr << "DLL path used: " << dllPath << std::endl;
            break;
        }

        success = true;
    } while (false);

    cleanup();
    return success;
}

int main()
{
    const char* targetProcess = "PAYDAY3-Win64-Shipping.exe";
    const char* dllName = "Payday3-Internal.dll";

    std::cout << "========================================" << std::endl;
    std::cout << "  PAYDAY 3 DLL Injector" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Target Process: " << targetProcess << std::endl;
    std::cout << "DLL to Inject: " << dllName << std::endl;
    std::cout << "========================================" << std::endl << std::endl;

    std::filesystem::path dllPath = std::filesystem::current_path() / dllName;
    std::string dllPathString = dllPath.string();
    
    if (!std::filesystem::exists(dllPath))
    {
        std::cerr << "Error: DLL not found at: " << dllPathString << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "DLL Path: " << dllPathString << std::endl << std::endl;

    std::cout << "Searching for process: " << targetProcess << std::endl;
    DWORD processId = GetProcessId(targetProcess);

    if (processId == 0)
    {
        std::cout << "Process not found." << std::endl << std::endl;
        
        if (!LaunchGame())
        {
            std::cerr << "Failed to launch the game." << std::endl;
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        
        std::cout << std::endl;
        
        processId = WaitForProcess(targetProcess, 180);
        
        if (processId == 0)
        {
            std::cerr << "Timeout: Game process not found after 3 minutes." << std::endl;
            std::cerr << "Please make sure Steam is running and try again." << std::endl;
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
    }

    std::cout << "Process found! PID: " << processId << std::endl << std::endl;

    std::cout << "Injecting DLL..." << std::endl;
    if (InjectDLL(processId, dllPathString))
    {
        std::cout << "Success! DLL injected successfully." << std::endl;
    }
    else
    {
        std::cerr << "Failed to inject DLL." << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}
