#include <windows.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <string>
#include <fstream>
#include <string_view>
#include <utility>
#include <winreg.h>
#include <unordered_map>

#include "WLEDCommon.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "Advapi32.lib")

// -----------------------------
// CONFIG
// -----------------------------
const std::string TAG = "WLED-ON-OFF_Common";
const char* LOG_PATH = "C:\\ProgramData\\WLED-ON-OFF.log";

namespace {
    constexpr wchar_t kRegRoot[] = L"SOFTWARE\\WLED-ON-OFF";
    constexpr wchar_t kRegHostValue[] = L"hostname";
    constexpr wchar_t kRegPortValue[] = L"port";
    constexpr char    kDefaultHost[] = "192.168.1.191";
    constexpr unsigned short kDefaultPort = 80;
}
// -----------------------------
// IMPLEMENTATIONS
// -----------------------------
void Log(const std::string& tag, const std::string & msg) {
    constexpr std::streamsize MAX_LOG_SIZE = 1 * 1024 * 1024; // 1 MB

    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64];
    sprintf_s(buf, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);

    // Check file size before writing
    {
        std::ifstream in(LOG_PATH, std::ios::ate | std::ios::binary);
        if (in.is_open()) {
            auto size = in.tellg();
            if (size > MAX_LOG_SIZE) {
                // Truncate (restart file)
                std::ofstream trunc(LOG_PATH, std::ios::trunc);
                if (trunc.is_open()) {
                    trunc << buf << "Log cleared (exceeded 1 MB)" << std::endl;
                }
            }
        }
    }

    // Append new message
    std::ofstream log(LOG_PATH, std::ios::app);
    if (log.is_open()) {
        log << buf << "[" + tag + "] " << msg << std::endl;
        log.flush();
    }
}

std::string GetWinInetErrorMessage(DWORD errorCode) {
    static const std::unordered_map<DWORD, std::string> errorMap = {
        {ERROR_INTERNET_CANNOT_CONNECT, "Host can't be reached (connection failed)."},
        {ERROR_INTERNET_TIMEOUT, "Request timed out."},
        {ERROR_INTERNET_NAME_NOT_RESOLVED, "DNS lookup failed: host name could not be resolved."},
        {ERROR_INTERNET_INVALID_URL, "Invalid URL."},
        {ERROR_INTERNET_CONNECTION_ABORTED, "Connection was aborted."},
        {ERROR_INTERNET_CONNECTION_RESET, "Connection was reset by the server."},
        {ERROR_INTERNET_SEC_CERT_DATE_INVALID, "SSL certificate date is invalid or expired."},
        {ERROR_INTERNET_SEC_CERT_CN_INVALID, "SSL certificate name does not match the host."},
        {ERROR_INTERNET_SEC_CERT_REV_FAILED, "Failed to check SSL certificate revocation."},
        {ERROR_INTERNET_SEC_CERT_ERRORS, "SSL certificate contains errors."},
        {ERROR_INTERNET_DECODING_FAILED, "Failed to decode server response."},
        {ERROR_INTERNET_INVALID_CA, "Untrusted Certificate Authority (CA)."},
        {ERROR_INTERNET_NOT_INITIALIZED, "WinINet not initialized properly."},
        {ERROR_INTERNET_LOGIN_FAILURE, "Authentication failed."},
        {ERROR_INTERNET_OPERATION_CANCELLED, "Operation was cancelled."}
    };
    auto it = errorMap.find(errorCode);
    if (it != errorMap.end())
        return it->second;

    // Try to get a system description if we don't have a custom one
    LPSTR msg = nullptr;
    if (FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, 0, (LPSTR)&msg, 0, NULL) && msg && *msg)
    {
        std::string result(msg);
        LocalFree(msg);
        return result;
    }

    return "Unknown error.";
}

bool LoadWLEDConfig(WLEDConnectionSettings& settings) {
    settings.host = kDefaultHost;
    settings.port = kDefaultPort;

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kRegRoot, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        Log(TAG, "Failed to open registry key for WLED configuration.");
        return false;
    }

    bool success = true;

    wchar_t hostBuffer[256] = {};
    DWORD hostSize = sizeof(hostBuffer);
    LONG hostRes = RegGetValueW(
        hKey,
        nullptr,
        kRegHostValue,
        RRF_RT_REG_SZ,
        nullptr,
        hostBuffer,
        &hostSize);

    if (hostRes == ERROR_SUCCESS && hostBuffer[0] != L'\0') {
        std::wstring hostW(hostBuffer);
        std::string str(hostW.begin(), hostW.end());
        settings.host = str;
        success = true;
    }
    else {
        success = false;
        Log(TAG, "Failed to read WLED hostname from registry. Using default host.");
    }

    DWORD portValue = 0;
    DWORD portSize = sizeof(portValue);
    LONG portRes = RegGetValueW(
        hKey,
        nullptr,
        kRegPortValue,
        RRF_RT_REG_DWORD,
        nullptr,
        &portValue,
        &portSize);

    if (portRes == ERROR_SUCCESS && portValue > 0 && portValue <= 65535) {
        settings.port = static_cast<unsigned short>(portValue);
    }
    else {
        success = false;
        Log(TAG, "Failed to read WLED port from registry. Using default port.");
    }

    RegCloseKey(hKey);
    return success;
}


bool SendToWLED(const std::string& jsonPayload) {
    WLEDConnectionSettings settings;
    if (!LoadWLEDConfig(settings)) {
        Log(TAG, "Failed getting registry keys.");
        return false;
    }

    HINTERNET hInternet = InternetOpenA("WLEDCommon", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) { Log(TAG, "InternetOpenA failed."); return false; }

    HINTERNET hConnect = InternetConnectA(
        hInternet,
        settings.host.c_str(),
        static_cast<INTERNET_PORT>(settings.port),
        NULL, NULL,
        INTERNET_SERVICE_HTTP,
        0, 0);
    if (!hConnect) { Log(TAG, "InternetConnectA failed."); InternetCloseHandle(hInternet); return false; }

    HINTERNET hRequest = HttpOpenRequestA(
        hConnect, "POST", "/json/state",
        NULL, NULL, NULL,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hRequest) {
        DWORD err = GetLastError();
        Log(TAG, "HttpOpenRequestA failed (" + settings.host + ":" + std::to_string(settings.port) +
            "). " + GetWinInetErrorMessage(err) + " (Error code: " + std::to_string(err) + ")");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return false;
    }

    const char* headers = "Content-Type: application/json\r\n";
    BOOL sent = HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers),
        (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.size());

    if (!sent) {
        DWORD err = GetLastError();
        std::string errMsg = GetWinInetErrorMessage(err); // already safe & human-friendly
        Log(TAG, "Sending request to WLED failed (" + settings.host + ":" + std::to_string(settings.port) +
            "). " + errMsg + " (Error code: " + std::to_string(err) + ")");
    }
    else {
        Log(TAG, "Sent to WLED : " + settings.host + " : " + std::to_string(settings.port) + " : " + jsonPayload);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return sent == TRUE;
}

bool IsNetworkConnected() {
    DWORD flags = 0;
    return InternetGetConnectedState(&flags, 0);
}

void WaitForNetworkReady(int maxWaitSeconds = 60) {
    Log(TAG, "Waiting for network to be ready...");
    int elapsed = 0;
    while (elapsed < maxWaitSeconds) {
        if (IsNetworkConnected()) {
            Log(TAG, "Network is connected.");
            return;
        }

        HANDLE hAddrChange = NULL;
        OVERLAPPED overlap = {};
        DWORD ret = NotifyAddrChange(&hAddrChange, &overlap);

        if (ret == ERROR_IO_PENDING) {
            DWORD waitRes = WaitForSingleObject(hAddrChange, 1000); // 1-sec interval
            elapsed += 1;
            if (waitRes == WAIT_OBJECT_0) {
                // A change occurred - re-check connectivity
                if (IsNetworkConnected()) {
                    Log(TAG, "Network became ready after change event.");
                    return;
                }
                Log(TAG, "Address changed but network still not ready, retrying...");
            }
        }
        else if (ret == NO_ERROR) {
            if (IsNetworkConnected()) {
                Log(TAG, "Network ready immediately.");
                return;
            }
        }
        else {
            Log(TAG, "NotifyAddrChange failed, retrying...");
            Sleep(1000);
            elapsed += 1;
        }
    }
    Log(TAG, "Network not ready after timeout, continuing anyway.");
}
