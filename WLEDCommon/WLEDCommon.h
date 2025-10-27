#pragma once
#include <string>
#include <windows.h>

// Shared configuration
void Log(const std::string& tag, const std::string& msg);

std::string GetWinInetErrorMessage(DWORD errorCode);

struct WLEDConnectionSettings {
    std::string host;
    unsigned short port;
};

bool LoadWLEDConfig(WLEDConnectionSettings& settings);


bool SendToWLED(const std::string& jsonPayload);

bool IsNetworkConnected();
void WaitForNetworkReady(int maxWaitSeconds);