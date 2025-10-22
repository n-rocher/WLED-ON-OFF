#pragma once
#include <string>

// Shared configuration
extern const char* WLED_IP;
extern const int   WLED_PORT;
extern const char* LOG_PATH;

// Shared helpers
void Log(const std::string& msg);
bool SendToWLED(const std::string& jsonPayload);

bool IsNetworkConnected();
void WaitForNetworkReady(int maxWaitSeconds = 60);