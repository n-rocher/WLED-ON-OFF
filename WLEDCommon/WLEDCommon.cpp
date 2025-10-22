#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <string>
#include <fstream>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")

// -----------------------------
// CONFIG
// -----------------------------
const char* WLED_IP = "192.168.1.191";
const int   WLED_PORT = 80;
const char* LOG_PATH = "C:\\ProgramData\\WLEDService.log";

// -----------------------------
// IMPLEMENTATIONS
// -----------------------------
void Log(const std::string& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    sprintf_s(buf, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    std::ofstream log(LOG_PATH, std::ios::app);
    if (log.is_open())
        log << buf << msg << std::endl;
}

bool SendToWLED(const std::string& jsonPayload) {
    HINTERNET hInternet = InternetOpenA("WLEDCommon", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) { Log("InternetOpenA failed."); return false; }

    HINTERNET hConnect = InternetConnectA(hInternet, WLED_IP, WLED_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { Log("InternetConnectA failed."); InternetCloseHandle(hInternet); return false; }

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/json/state", NULL, NULL, NULL,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hRequest) { Log("HttpOpenRequestA failed."); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }

    const char* headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers),
        (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.size());

    if (!sent) Log("HttpSendRequestA failed.");
    else Log("Sent to WLED: " + jsonPayload);

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return sent;
}

bool IsNetworkConnected() {
    DWORD flags = 0;
    return InternetGetConnectedState(&flags, 0);
}

void WaitForNetworkReady(int maxWaitSeconds = 60) {
    Log("Waiting for network to be ready...");
    int elapsed = 0;
    while (elapsed < maxWaitSeconds) {
        if (IsNetworkConnected()) {
            Log("Network is connected.");
            return;
        }

        HANDLE hAddrChange = NULL;
        OVERLAPPED overlap = {};
        DWORD ret = NotifyAddrChange(&hAddrChange, &overlap);

        if (ret == ERROR_IO_PENDING) {
            DWORD waitRes = WaitForSingleObject(hAddrChange, 1000); // 1-sec interval
            elapsed += 1;
            if (waitRes == WAIT_OBJECT_0) {
                // A change occurred — re-check connectivity
                if (IsNetworkConnected()) {
                    Log("Network became ready after change event.");
                    return;
                }
                Log("Address changed but network still not ready, retrying...");
            }
        }
        else if (ret == NO_ERROR) {
            if (IsNetworkConnected()) {
                Log("Network ready immediately.");
                return;
            }
        }
        else {
            Log("NotifyAddrChange failed, retrying...");
            Sleep(1000);
            elapsed += 1;
        }
    }
    Log("Network not ready after timeout, continuing anyway.");
}