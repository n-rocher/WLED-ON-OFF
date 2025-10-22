#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <ctime>
#include <powrprof.h>
#include <WtsApi32.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "Wtsapi32.lib")

#include <WLEDCommon.h>

// Global handle for power notifications so we can unregister it
HPOWERNOTIFY g_hNotify = NULL;

// ---- Window proc ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_POWERBROADCAST:
        if (wParam == PBT_POWERSETTINGCHANGE) {
            auto* pbs = (POWERBROADCAST_SETTING*)lParam;
            if (IsEqualGUID(pbs->PowerSetting, GUID_CONSOLE_DISPLAY_STATE)) {
                DWORD state = *(DWORD*)pbs->Data;
                if (state == 0) {
                    Log("Display turned OFF (user session).");
                    SendToWLED(R"({"on":false})");
                }
                else if (state == 1) {
                    Log("Display turned ON (user session).");
                    WaitForNetworkReady(60);
                    Sleep(1500);
                    SendToWLED(R"({"on":true})");
                }
            }
        }
        break;

    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_UNLOCK || wParam == WTS_SESSION_LOGON) {
            Log("User session unlocked/logon detected.");
            WaitForNetworkReady(60);
            SendToWLED(R"({"on":true})");
        }
        else if (wParam == WTS_SESSION_LOCK || wParam == WTS_SESSION_LOGOFF) {
            Log("User session locked/logoff detected.");
            SendToWLED(R"({"on":false})");
        }
        break;

    case WM_DESTROY:
        if (g_hNotify) {
            UnregisterPowerSettingNotification(g_hNotify);
        }
        WTSUnRegisterSessionNotification(hwnd);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}


// ---- Entry point ----
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

    const wchar_t CLASS_NAME[] = L"WLEDHelperWindow";
    WNDCLASSW wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // Create a message-only window (it's invisible)
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"WLEDHelper", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create message window", L"WLEDHelper", MB_OK);
        return 1;
    }

    // Register for user session events (lock, unlock, etc.)
    if (!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)) {
        Log("Failed to register for session notifications (user helper). Exiting.");
        return 1;
    }

    // Register for display power notifications
    g_hNotify = RegisterPowerSettingNotification(
        hwnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    if (!g_hNotify) {
        Log("Failed to register for display notifications (user helper). Exiting.");
        WTSUnRegisterSessionNotification(hwnd);
        return 1;
    }

    Log("WLEDHelper started and running in background.");

    // Standard message loop to process window messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hNotify) {
        UnregisterPowerSettingNotification(g_hNotify);
    }
    WTSUnRegisterSessionNotification(hwnd);

    Log("WLEDHelper stopped.");
    return (int)msg.wParam;
}