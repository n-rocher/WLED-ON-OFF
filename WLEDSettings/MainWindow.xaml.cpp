#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Composition::SystemBackdrops;
using namespace Microsoft::UI::Windowing;
using namespace Windows::Data::Json;
using namespace Windows::Web::Http;
using namespace Windows::Foundation;

namespace
{

    constexpr wchar_t kRegRoot[] = L"SOFTWARE\\WLED-ON-OFF";
    HKEY kRegHive = HKEY_LOCAL_MACHINE;

    hstring JsonSafeGetString(JsonObject const& obj, hstring const& key, hstring const& def = L"")
    {
        return obj.HasKey(key) && obj.GetNamedValue(key).ValueType() == JsonValueType::String
            ? obj.GetNamedString(key) : def;
    }
    int32_t JsonSafeGetInt(JsonObject const& obj, hstring const& key, int32_t def = 0)
    {
        return obj.HasKey(key) && obj.GetNamedValue(key).ValueType() == JsonValueType::Number
            ? static_cast<int32_t>(obj.GetNamedNumber(key)) : def;
    }
}


namespace winrt::WLEDSettings::implementation
{

    MainWindow::MainWindow() : m_uiQueue(winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread())

    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);

        // Http client
        m_http = HttpClient{};

        HostBox().TextChanged({ this, &MainWindow::OnConfigTextChanged });
        PortBox().TextChanged({ this, &MainWindow::OnConfigTextChanged });

        // --- Throttling Setup (for ColorPicker) ---
        m_throttleTimer = DispatcherTimer{};
        m_throttleTimer.Interval(std::chrono::milliseconds(200)); 
        m_throttleTimer.Tick([this](IInspectable const&, IInspectable const&) {
            // Check if the color has changed since the last tick
            if (m_colorUpdatePending) {

                // Execute the network call
                SendColorAsync(m_color.R, m_color.G, m_color.B, m_color.A);

                // Reset the flag to wait for the next color change
                m_colorUpdatePending = false;
            }
            });
        m_throttleTimer.Start(); // Start the throttle loop immediately

        // Load config & one immediate status check
        LoadConfig();
        UpdateStatusAsync();

    }

    void MainWindow::EnsureRegKey()
    {
        HKEY hKey;
        // Creates the key if it doesn't exist
        RegCreateKeyExW(
            kRegHive,
            kRegRoot,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_ALL_ACCESS,
            nullptr,
            &hKey,
            nullptr
        );
        if (hKey != nullptr)
        {
            RegCloseKey(hKey);
        }
    }

    void MainWindow::LoadConfig()
    {
        HKEY hKey = nullptr;
        DWORD dataSize = 0;

        // Default values if registry read fails
        hstring defaultHost = L"192.168.1.X";
        int32_t defaultPort = 80;

        try
        {
            // Open the key for reading
            if (RegOpenKeyExW(kRegHive, kRegRoot, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            {
                // Key not found or access denied, use defaults
                HostBox().Text(defaultHost);
                PortBox().Text(to_hstring(defaultPort));
                StatusText().Text(L"Connecting...");
                StatusDot().Fill(m_orange);
                return;
            }

            // --- Read Hostname ---
            std::wstring host(256, L'\0');
            dataSize = host.size() * sizeof(wchar_t);
            if (RegQueryValueExW(hKey, L"hostname", nullptr, nullptr, (LPBYTE)host.data(), &dataSize) == ERROR_SUCCESS)
            {
                host.resize(dataSize / sizeof(wchar_t) - 1); // Resize and remove null terminator
                HostBox().Text(host);
            }
            else
            {
                HostBox().Text(defaultHost);
            }

            // --- Read Port ---
            DWORD portValue = 0;
            dataSize = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"port", nullptr, nullptr, (LPBYTE)&portValue, &dataSize) == ERROR_SUCCESS)
            {
                PortBox().Text(to_hstring(static_cast<int32_t>(portValue)));
            }
            else
            {
                PortBox().Text(to_hstring(defaultPort));
            }

            RegCloseKey(hKey);

            // Final status update remains the same for file/key not found logic
            StatusText().Text(L"Connecting...");
            StatusDot().Fill(m_orange);
        }
        catch (...)
        {
            if (hKey != nullptr) RegCloseKey(hKey);
            StatusText().Text(L"Error loading config.");
            StatusDot().Fill(m_red);
        }
    }

    void LogLastError(const wchar_t* context)
    {
        DWORD err = GetLastError();
        wchar_t msg[512];
        swprintf_s(msg, L"[WLED DEBUG] %s failed (Error %lu)\n", context, err);
        OutputDebugStringW(msg);
    }

    void MainWindow::RestartWLEDService()
    {
        SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!scm)
        {
            LogLastError(L"OpenSCManager");
            return;
        }

        SC_HANDLE service = OpenServiceW(scm, L"WLED-ON-OFF_Service", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
        if (!service)
        {
            LogLastError(L"OpenServiceW");
            CloseServiceHandle(scm);
            return;
        }

        SERVICE_STATUS status = {};
        if (!ControlService(service, SERVICE_CONTROL_STOP, &status))
        {
            DWORD err = GetLastError();
            if (err != ERROR_SERVICE_NOT_ACTIVE)  // It's fine if already stopped
                LogLastError(L"ControlService(STOP)");
        }

        // Wait for service to stop (up to ~5 seconds)
        for (int i = 0; i < 50; ++i)
        {
            if (!QueryServiceStatus(service, &status))
            {
                LogLastError(L"QueryServiceStatus");
                break;
            }
            if (status.dwCurrentState == SERVICE_STOPPED)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!StartServiceW(service, 0, nullptr))
            LogLastError(L"StartServiceW");

        CloseServiceHandle(service);
        CloseServiceHandle(scm);

        OutputDebugStringW(L"[WLED DEBUG] WLED-ON-OFF_Service restart attempt complete.\n");
    }

    void MainWindow::RestartWLEDHelper()
    {
        OutputDebugStringW(L"[WLED DEBUG] Restarting WLEDHelper...\n");

        // Get current executable path
        wchar_t exePath[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len == MAX_PATH) {
            LogLastError(L"GetModuleFileNameW");
            return;
        }

        // Explicitly terminate the string
        exePath[len] = L'\0';

        // Now it's safe to use exePath
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        std::wstring helperPath = (exeDir / L"WLED-ON-OFF_Helper.exe").wstring();

        OutputDebugStringW(helperPath.c_str());


        // --- Terminate any running instance of the helper ---
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            LogLastError(L"CreateToolhelp32Snapshot");
            return;
        }

        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, L"WLED-ON-OFF_Helper.exe") == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (hProcess) {
                        if (!TerminateProcess(hProcess, 0))
                            LogLastError(L"TerminateProcess");
                        else
                            OutputDebugStringW(L"[WLED DEBUG] Terminated WLEDHelper.exe\n");

                        CloseHandle(hProcess);
                    }
                    else {
                        LogLastError(L"OpenProcess(PROCESS_TERMINATE)");
                    }
                }
            } while (Process32NextW(snapshot, &entry));
        }
        else {
            LogLastError(L"Process32FirstW");
        }

        CloseHandle(snapshot);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // --- Restart the helper ---
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessW(helperPath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            LogLastError(L"CreateProcessW (WLEDHelper)");
        }
        else {
            OutputDebugStringW(L"[WLED DEBUG] WLEDHelper.exe started successfully.\n");
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    void MainWindow::SaveConfig(winrt::hstring const& host_h, winrt::hstring const& port_h)
    {
        EnsureRegKey();

        HKEY hKey = nullptr;
        if (RegOpenKeyExW(kRegHive, kRegRoot, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            return;

        // Save Host
        const wchar_t* host = host_h.c_str();
        RegSetValueExW(
            hKey, L"hostname", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(host),
            static_cast<DWORD>((wcslen(host) + 1) * sizeof(wchar_t))
        );

        // Save Port
        try
        {
            int portInt = std::stoi(std::wstring{ port_h });
            DWORD portValue = static_cast<DWORD>(portInt);
            RegSetValueExW(hKey, L"port", 0, REG_DWORD,
                reinterpret_cast<const BYTE*>(&portValue), sizeof(DWORD));
        }
        catch (...) { /* ignore bad port */ }

        RegCloseKey(hKey);

        RestartWLEDService();
        RestartWLEDHelper();
    }


    // -------- UI EVENTS --------


// --- DEBOUNCE LOGIC ---
    void MainWindow::OnConfigTextChanged(IInspectable const&, TextChangedEventArgs const&)
    {

        m_latestHost = HostBox().Text();
        m_latestPort = PortBox().Text();

        {
            std::scoped_lock lock(m_debounceMutex);
            m_lastInputTime = std::chrono::steady_clock::now();
            m_debouncePending = true;
        }

        ScheduleDebouncedConfigUpdate();
    }

    void MainWindow::ScheduleDebouncedConfigUpdate()
    {
        static std::atomic<bool> running{ false };
        bool expected = false;
        if (!running.compare_exchange_strong(expected, true)) return;

        std::thread([this]() {
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                if (!m_debouncePending.load()) continue;

                auto now = std::chrono::steady_clock::now();
                auto elapsed = now - m_lastInputTime;

                if (elapsed > std::chrono::milliseconds(200))
                {
                    m_debouncePending = false;

                    // Read the latest cached values (no UI touch)
                    winrt::hstring host, port;
                    {
                        std::scoped_lock lock(m_debounceMutex);
                        host = m_latestHost;
                        port = m_latestPort;
                    }

                    // Do the heavy work off-UI
                    std::thread([this, host, port]() {
                        //SaveConfig(host, port);

                        // Hop to UI thread to refresh UI/state safely
                        if (m_uiQueue)
                        {
                            m_uiQueue.TryEnqueue([weak = get_weak()]() {
                                if (auto strong = weak.get()) {
                                    strong->m_statusRequestId.fetch_add(1);
                                    strong->UpdateStatusAsync();
                                }
                            });
                        }
                        }).detach();
                }
            }
            }).detach();
    }

    void MainWindow::OnSaveClick(IInspectable const&, RoutedEventArgs const&)
    {

        m_latestHost = HostBox().Text();
        m_latestPort = PortBox().Text();

        SaveConfig(m_latestHost, m_latestPort);
        UpdateStatusAsync();
    }

    void MainWindow::OnBrightnessSliderChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        if (is_connected) {
            m_color.A = e.NewValue();
            m_colorUpdatePending = true;
        }
    }

   void MainWindow::OnColorSpectrum_ColorChanged(winrt::WLEDSettings::ColorSpectrum const& sender, Windows::UI::Color const& color)
    {

        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush brush{ color };
        ColorPreview().Background(brush);

        m_color.R = color.R;
        m_color.G = color.G;
        m_color.B = color.B;

        m_colorUpdatePending = true;
    }

    // -------- NETWORK CALLS --------

   IAsyncAction MainWindow::UpdateStatusAsync()
   {
       auto lifetime = get_strong();

       // capture the generation id at the time this started
       const uint64_t thisId = m_statusRequestId.load();

       // immediately mark UI as connecting
       StatusDot().Fill(m_orange);
       StatusText().Text(L"Connecting...");
       InfoText().Text(L"");
       is_connected = false;

       try
       {
           hstring host = HostBox().Text();
           hstring port = PortBox().Text();

           // if a new update has started since, stop early
           if (thisId != m_statusRequestId.load()) co_return;

           hstring url_info = L"http://" + host + L":" + port + L"/json/info";

           HttpResponseMessage resp_info = co_await m_http.GetAsync(Windows::Foundation::Uri{ url_info });

           // cancel check after the await
           if (thisId != m_statusRequestId.load()) co_return;

           if (!resp_info.IsSuccessStatusCode()) co_return;

           hstring body_info = co_await resp_info.Content().ReadAsStringAsync();
           if (thisId != m_statusRequestId.load()) co_return;

           auto info = JsonObject::Parse(body_info);

           auto ver = JsonSafeGetString(info, L"ver", L"?");
           int leds = 0;

           if (info.HasKey(L"leds") && info.GetNamedValue(L"leds").ValueType() == JsonValueType::Object)
           {
               auto ledsObj = info.GetNamedObject(L"leds");
               leds = JsonSafeGetInt(ledsObj, L"count", 0);
           }

           if (thisId != m_statusRequestId.load()) co_return;
           m_ledCount = leds;

           // --- STATE CALL ---
           hstring url_state = L"http://" + host + L":" + port + L"/json/state";
           HttpResponseMessage resp_state = co_await m_http.GetAsync(Windows::Foundation::Uri{ url_state });
           if (thisId != m_statusRequestId.load()) co_return;

           if (!resp_state.IsSuccessStatusCode()) co_return;

           hstring body_state = co_await resp_state.Content().ReadAsStringAsync();
           if (thisId != m_statusRequestId.load()) co_return;

           auto state = JsonObject::Parse(body_state);

           // Brightness
           if (state.HasKey(L"bri") && state.GetNamedValue(L"bri").ValueType() == JsonValueType::Number)
           {
               m_color.A = static_cast<uint8_t>(state.GetNamedNumber(L"bri"));
               BrightnessSlider().Value(m_color.A);
           }

           // Color
           if (state.HasKey(L"seg") && state.GetNamedValue(L"seg").ValueType() == JsonValueType::Array)
           {
               auto segArray = state.GetNamedArray(L"seg");
               if (segArray.Size() > 0)
               {
                   auto segObj = segArray.GetObjectAt(0);
                   if (segObj.HasKey(L"col"))
                   {
                       auto colArray = segObj.GetNamedArray(L"col");
                       if (colArray.Size() > 0 && colArray.GetAt(0).ValueType() == JsonValueType::Array)
                       {
                           auto rgb = colArray.GetAt(0).GetArray();
                           if (rgb.Size() >= 3)
                           {
                               m_color.R = static_cast<uint8_t>(rgb.GetNumberAt(0));
                               m_color.G = static_cast<uint8_t>(rgb.GetNumberAt(1));
                               m_color.B = static_cast<uint8_t>(rgb.GetNumberAt(2));

                               Windows::UI::Color newColor{ 255, m_color.R, m_color.G, m_color.B };
                               winrt::Microsoft::UI::Xaml::Media::SolidColorBrush brush{ newColor };
                               ColorPreview().Background(brush);
                           }
                       }
                   }
               }
           }

           // cancel check before finishing
           if (thisId != m_statusRequestId.load()) co_return;

           StatusDot().Fill(m_green);
           StatusText().Text(L"Connected");
           InfoText().Text(L"v" + ver + L" • " + to_hstring(leds) + L" LEDs");
           is_connected = true;
       }
       catch (...)
       {
           // only show fail if this is still the latest request
           if (thisId == m_statusRequestId.load())
           {
               StatusDot().Fill(m_red);
               StatusText().Text(L"Failed to connect");
               InfoText().Text(L"");
               is_connected = false;
           }
       }

       co_return;
   }


    IAsyncAction MainWindow::SendColorAsync(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        auto lifetime = get_strong();
        try
        {
            hstring host = HostBox().Text();
            hstring port = PortBox().Text();

            if (host.empty() || port.empty()) co_return;

            hstring url = L"http://" + host + L":" + port + L"/json/state";

            std::wstring jsonBody = L"{\"on\":true,\"bri\":" + std::to_wstring(a) + L",\"seg\":[{\"id\":0,\"start\":0,\"stop\":" + std::to_wstring(m_ledCount) + L",\"col\":[["
                + std::to_wstring(r) + L","
                + std::to_wstring(g) + L","
                + std::to_wstring(b) + L"]]}]}";

            {
                std::wstringstream ss;
                ss << L"[SendColorAsync] Sending to " << url.c_str() << L"\n"
                    << L"  RGB=(" << (int)r << L"," << (int)g << L"," << (int)b << L")"
                    << L"  Brightness=" << (int)a
                    << L"  LEDCount=" << m_ledCount << L"\n"
                    << L"  Body=" << jsonBody << L"\n";
                OutputDebugStringW(ss.str().c_str());
            }

            HttpStringContent content{ jsonBody, Windows::Storage::Streams::UnicodeEncoding::Utf8, L"application/json" };
            co_await m_http.PostAsync(Windows::Foundation::Uri{ url }, content);
        }
        catch (...) { /* ignore transient errors */ }
        co_return;
    }

}