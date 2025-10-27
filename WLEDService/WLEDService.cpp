#include "WLEDCommon.h"

#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <powrprof.h>
#include <iphlpapi.h>

#include <stdio.h>

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

#define SERVICE_NAME L"WLED-ON-OFF_Service"

// ----------------------------
// CONFIGURATION
// ----------------------------
const std::string TAG = "WLED-ON-OFF_Service";
const wchar_t SERVICE_DISPLAY_NAME[] = L"WLED ON/OFF Automation for Windows";

// ----------------------------
// GLOBAL VARIABLES
// ----------------------------
SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE                g_StopEvent = nullptr;

// ----------------------------
// UTILITIES
// ----------------------------

// Check if the process has admin privileges
bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

// Relaunch the process elevated (UAC)
bool RelaunchElevated(int argc, wchar_t* argv[]) {
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) return false;

    std::wstring params;
    for (int i = 1; i < argc; i++) {
        params += L"\"";
        params += argv[i];
        params += L"\" ";
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";  // triggers UAC
    sei.lpFile = exePath;
    sei.lpParameters = params.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            MessageBoxW(NULL, L"Administrator privileges are required.\nOperation canceled.", SERVICE_DISPLAY_NAME, MB_ICONERROR);
        }
        return false;
    }
    return true;
}

// Execute a system command quietly
bool RunCommand(const std::wstring& command) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::wstring cmd = L"cmd.exe /c " + command;

    BOOL ok = CreateProcessW(
        NULL, &cmd[0], NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0);
}

void HandlePowerState(bool isAwake) {

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

    if (isAwake) {
        Log(TAG, "System is awake/resumed or starting.");
        SendToWLED(R"({"on":true})");
    }
    else {
        Log(TAG, "System going to sleep or shutting down.");
        SendToWLED(R"({"on":false})");

        // Empêche l'arrêt immédiat pendant 2 secondes
        Sleep(2000);
    }
}


// ----------------------------
// SERVICE HANDLER
// ----------------------------

DWORD WINAPI ServiceHandler(DWORD control, DWORD eventType, LPVOID lpEventData, LPVOID lpContext) {
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        Log(TAG, "Service stopping or system shutting down.");
        HandlePowerState(false);
        SetEvent(g_StopEvent);
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return NO_ERROR;

    case SERVICE_CONTROL_POWEREVENT:
        if (eventType == PBT_APMSUSPEND) {
            HandlePowerState(false);
        }
        else if (eventType == PBT_APMRESUMEAUTOMATIC || eventType == PBT_APMRESUMECRITICAL) {
            WaitForNetworkReady(120);
            HandlePowerState(true);
        }
        return NO_ERROR;

    default:
        break;
    }
    return NO_ERROR;
}



// ----------------------------
// SERVICE MAIN
// ----------------------------

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceHandler, nullptr);
    if (!g_StatusHandle)  return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    Log(TAG, "Service started successfully.");

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Démarrage du système → allumer le WLED
    WaitForNetworkReady(120);
    HandlePowerState(true);

    WaitForSingleObject(g_StopEvent, INFINITE);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    Log(TAG, "Service stopped.");
}


// ----------------------------
// SERVICE INSTALL / UNINSTALL
// ----------------------------

bool InstallService() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) {
        wprintf(L"Failed to get service path. (%lu)\n", GetLastError());
        return false;
    }

    // Open connection to Service Control Manager
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        wprintf(L"OpenSCManager failed. (%lu)\n", GetLastError());
        return false;
    }

    // If service exists, delete it first
    SC_HANDLE oldSvc = OpenServiceW(scm, SERVICE_NAME, DELETE | SERVICE_STOP);
    if (oldSvc) {
        SERVICE_STATUS status;
        ControlService(oldSvc, SERVICE_CONTROL_STOP, &status);
        DeleteService(oldSvc);
        CloseServiceHandle(oldSvc);
    }

    // Create new service
    SC_HANDLE service = CreateServiceW(
        scm,
        SERVICE_NAME,
        SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        path,
        NULL, NULL, NULL, NULL, NULL);

    if (!service) {
        wprintf(L"CreateService failed. (%lu)\n", GetLastError());
        CloseServiceHandle(scm);
        return false;
    }

    // Set the service description
    SERVICE_DESCRIPTIONW desc;
    desc.lpDescription = const_cast<LPWSTR>(L"Controls WLED power state on sleep/wake events");
    ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc);

    // Start the service
    if (!StartServiceW(service, 0, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING)
            wprintf(L"StartService failed. (%lu)\n", err);
    }

    wprintf(L"Service installed successfully.\n");

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

bool KillProcessByName(const std::wstring& processName)
{
    bool found = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W entry = { sizeof(entry) };

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 0)) {
                        wprintf(L"Terminated process: %s (PID %lu)\n", entry.szExeFile, entry.th32ProcessID);
                        found = true;
                    }
                    else {
                        wprintf(L"Failed to terminate %s (Error %lu)\n", entry.szExeFile, GetLastError());
                    }
                    CloseHandle(hProcess);
                }
                else {
                    wprintf(L"OpenProcess failed for %s (Error %lu)\n", entry.szExeFile, GetLastError());
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

bool UninstallService()
{
    // --- Open the Service Control Manager ---
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        wprintf(L"OpenSCManager failed. (%lu)\n", GetLastError());
        return false;
    }

    // --- Open and stop the service if it exists ---
    SC_HANDLE service = OpenServiceW(scm, SERVICE_NAME, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (service) {
        SERVICE_STATUS status = {};
        ControlService(service, SERVICE_CONTROL_STOP, &status);

        // Try to delete the service
        if (!DeleteService(service))
            wprintf(L"DeleteService failed. (%lu)\n", GetLastError());
        else
            wprintf(L"Service uninstalled successfully.\n");

        CloseServiceHandle(service);
    }
    else {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST)
            wprintf(L"Service not found.\n");
        else
            wprintf(L"OpenService failed. (%lu)\n", err);
    }

    CloseServiceHandle(scm);

    // --- Kill leftover processes ---
    wprintf(L"Killing related processes...\n");
    bool killedHelper = KillProcessByName(L"WLED-ON-OFF_Helper.exe");
    bool killedService = KillProcessByName(L"WLED-ON-OFF_Service.exe");

    if (!killedHelper && !killedService)
        wprintf(L"No matching processes were running.\n");

    return true;
}

// ----------------------------
// MAIN ENTRY POINT
// ----------------------------
int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1) {
        std::wstring arg = argv[1];

        // Check for admin rights before install/uninstall
        if ((arg == L"install" || arg == L"uninstall") && !IsRunAsAdmin()) {
            if (!RelaunchElevated(argc, argv))
                return 1; // user refused elevation
            return 0; // elevated instance will handle the rest
        }

        if (arg == L"install") {
            InstallService();
            return 0;
        }
        else if (arg == L"uninstall") {
            UninstallService();
            return 0;
        }
    }

    // Regular service start
    SERVICE_TABLE_ENTRYW ServiceTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherW(ServiceTable)) {
        MessageBoxW(NULL, L"Failed to start service control dispatcher", SERVICE_NAME, MB_OK | MB_ICONERROR);
        Log(TAG, "StartServiceCtrlDispatcher failed.");
    }

    return 0;
}