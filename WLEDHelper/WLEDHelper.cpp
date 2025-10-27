#include <windows.h>
#include <wininet.h>
#include <string>
#include <fstream>
#include <ctime>
#include <powrprof.h>
#include <WtsApi32.h>

#include <thread>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "Wtsapi32.lib")

#include <WLEDCommon.h>

const std::string TAG = "WLED-ON-OFF_Helper";

HPOWERNOTIFY g_hNotifyConsole = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_POWERBROADCAST:
        if (wParam == PBT_POWERSETTINGCHANGE) {
            auto* pbs = reinterpret_cast<POWERBROADCAST_SETTING*>(lParam);
            if (IsEqualGUID(pbs->PowerSetting, GUID_CONSOLE_DISPLAY_STATE)) {
                DWORD state = *reinterpret_cast<const DWORD*>(pbs->Data);
                if (state == 0) {
                    Log(TAG, "Display turned OFF (user session).");
                    std::thread([] { SendToWLED(R"({"on":false})"); }).detach();
                }
                else if (state == 1) {
                    Log(TAG, "Display turned ON (user session).");
                    std::thread([] {
                        WaitForNetworkReady(60);
                        Sleep(1500);
                        SendToWLED(R"({"on":true})");
                     }).detach();
                }
                return TRUE;
            }
        }
        return TRUE;

    case WM_WTSSESSION_CHANGE:
        switch (wParam) {
            case WTS_SESSION_UNLOCK:
            case WTS_SESSION_LOGON:
                Log(TAG, "User session unlocked/logon detected.");
                std::thread([] {
                    WaitForNetworkReady(60);
                    SendToWLED(R"({"on":true})");
                }).detach();
                return 0;

            case WTS_SESSION_LOCK:
            case WTS_SESSION_LOGOFF:
                Log(TAG, "User session locked/logoff detected.");
                std::thread([] { SendToWLED(R"({"on":false})"); }).detach();
                return 0;

            default:
                break; // fall through to DefWindowProc
        }
        break;

    case WM_DESTROY:
        if (g_hNotifyConsole) {
            UnregisterPowerSettingNotification(g_hNotifyConsole);
            g_hNotifyConsole = NULL;                 // IMPORTANT
        }
        WTSUnRegisterSessionNotification(hwnd);       // Unregister ONCE here
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    const wchar_t CLASS_NAME[] = L"WLED-ON-OFF_Helper";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // Create a HIDDEN TOP-LEVEL window (not HWND_MESSAGE)
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"WLED-ON-OFF_Helper",
        WS_OVERLAPPEDWINDOW,        // top-level
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window", L"WLED-ON-OFF_Helper", MB_OK);
        return 1;
    }
    ShowWindow(hwnd, SW_HIDE);

    if (!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)) {
        Log(TAG, "Failed to register for session notifications (user helper). Exiting.");
        DestroyWindow(hwnd);
        return 1;
    }

    g_hNotifyConsole = RegisterPowerSettingNotification(
        hwnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!g_hNotifyConsole) {
        Log(TAG, "RegisterPowerSettingNotification failed.");
        // continue anyway; you still get WTS events
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // DO NOT unregister again here (already done in WM_DESTROY)
    Log(TAG, "stopped.");
    return static_cast<int>(msg.wParam);
}