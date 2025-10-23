#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <filesystem> // C++17 library for robust path manipulation

namespace fs = std::filesystem;

// The name of your main executable, assumed to be in the parent directory of the launcher.
constexpr wchar_t MAIN_APP_NAME[] = L"WLEDSettings\\WLEDSettings.exe";

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    // 1. Get the full path to the currently running executable (the launcher).
    wchar_t launcherPath[MAX_PATH];
    GetModuleFileNameW(NULL, launcherPath, MAX_PATH);

    // 2. Use std::filesystem to find the directory of the launcher.
    fs::path currentDir = fs::path(launcherPath).parent_path();

    // 4. Construct the full path to the main application: ../WLEDSettings.exe
    fs::path mainAppPath = currentDir / MAIN_APP_NAME;

    // Convert the path to a wstring for the Win32 API
    std::wstring pathStr = mainAppPath.wstring();

    // 5. Setup the SHELLEXECUTEINFOW structure.
    SHELLEXECUTEINFOW sei = { 0 };

    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"runas";          // CRITICAL: Requests elevation (UAC prompt).
    sei.lpFile = pathStr.c_str();   // The dynamically calculated path.
    sei.lpParameters = nullptr;     // Arguments for the main app.
    sei.lpDirectory = nullptr;
    sei.nShow = SW_SHOWNORMAL;

    // 6. Attempt to launch the main application.
    if (!ShellExecuteExW(&sei))
    {
        DWORD error = GetLastError();

        // Check if the user canceled the UAC prompt (Error 1223)
        if (error != ERROR_CANCELLED)
        {
            // Error handling, e.g., if the main app was not found
            std::wcerr << L"Failed to launch main app '" << mainAppPath.c_str() << L"' with error: " << error << std::endl;
            return 1;
        }
        return 0;
    }

    return 0;
}