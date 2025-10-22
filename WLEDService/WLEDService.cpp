#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>

#include <powrprof.h>
#include <iphlpapi.h>

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")

#include "WLEDCommon.h"


// ----------------------------
// CONFIGURATION
// ----------------------------
const wchar_t SERVICE_NAME[] = L"WLEDService";

// ----------------------------
// VARIABLES GLOBALES
// ----------------------------
SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE                g_StopEvent = nullptr;

// ----------------------------
// UTILITAIRES
// ----------------------------

void HandlePowerState(bool isAwake) {

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);

    if (isAwake) {
        Log("System is awake/resumed or starting.");
        SendToWLED(R"({"on":true})");
    }
    else {
        Log("System going to sleep or shutting down.");
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
        Log("Service stopping or system shutting down.");
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
    if (!g_StatusHandle) {
        return;
    }

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    Log("Service started successfully.");

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Démarrage du système → allumer le WLED
    WaitForNetworkReady(120);
    HandlePowerState(true);

    WaitForSingleObject(g_StopEvent, INFINITE);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    Log("Service stopped.");
}

// ----------------------------
// MAIN ENTRY POINT
// ----------------------------

int wmain(int argc, wchar_t* argv[]) {
    SERVICE_TABLE_ENTRYW ServiceTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherW(ServiceTable)) {
        MessageBoxW(NULL, L"Failed to start service control dispatcher", SERVICE_NAME, MB_OK | MB_ICONERROR);
        Log("StartServiceCtrlDispatcher failed.");
    }

    return 0;
}