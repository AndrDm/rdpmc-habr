#include <windows.h>
#include <iostream>

bool LoadKernelDriver(const std::wstring& driverName, const std::wstring& driverPath) {
    SC_HANDLE schSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        std::wcerr << L"OpenSCManager failed: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE schService = CreateService(
        schSCManager,
        driverName.c_str(),
        driverName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_IGNORE,
        driverPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!schService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            // Service exists, try to open it
            schService = OpenService(schSCManager, driverName.c_str(), SERVICE_ALL_ACCESS);
            if (!schService) {
                std::wcerr << L"OpenService failed: " << GetLastError() << std::endl;
                CloseServiceHandle(schSCManager);
                return false;
            }
        }
        else {
            std::wcerr << L"CreateService failed: " << err << std::endl;
            CloseServiceHandle(schSCManager);
            return false;
        }
    }

    if (!StartService(schService, 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            std::wcerr << L"StartService failed: " << err << std::endl; // ERROR_ACCESS_DENIED
            return false;
        }
    }

    std::wcout << L"Driver started successfully." << std::endl;

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

int main() {
    std::wstring driverName = L"MSR"; // Kernel driver service name
    std::wstring driverFile = L"RDPMC.sys"; // Full path to the .sys file

    std::wcout << L"Driver will be loaded" << std::endl;
    if (!LoadKernelDriver(driverName, driverFile)) {
        std::wcerr << L"Failed to load driver " << driverFile << std::endl;
        return 1;
    }
    std::wcout << L"Driver loaded" << std::endl; // Driver is now loaded and running.
    return 0;
}
