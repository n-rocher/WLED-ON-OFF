#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <fstream>
#include <iterator>
#include <string>
#include <winreg.h>

#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Interop.h>

#include <windows.h> // Required for OutputDebugStringW
#include <sstream>     // Required for wstringstream

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

    constexpr wchar_t kRegRoot[] = L"SOFTWARE\\WLEDController";
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

    MainWindow::MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(true);

        // Http client
        m_http = HttpClient{};

        HostBox().TextChanged({ this, &MainWindow::OnConfigTextChanged });
        PortBox().TextChanged({ this, &MainWindow::OnConfigTextChanged });

        // --- 1. Debounce Setup (for Host/Port text input) ---
        m_debounceTimer = DispatcherTimer{};
        m_debounceTimer.Interval(std::chrono::milliseconds(500));
        m_debounceTimer.Tick([this](IInspectable const&, IInspectable const&) {
            m_debounceTimer.Stop(); // Stop the timer once it fires
            SaveConfig();           // Save the config
            UpdateStatusAsync();    // Check connection/status
          });


        // --- 2. Throttling Setup (for ColorPicker) ---
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

    void MainWindow::SaveConfig()
    {
        EnsureRegKey();

        HKEY hKey = nullptr;

        if (RegOpenKeyExW(kRegHive, kRegRoot, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        {
            // Failed to open key for writing (likely lack of admin rights if using HKLM)
            return;
        }

        // --- Save Hostname ---
        std::wstring host = HostBox().Text().c_str();
        RegSetValueExW(
            hKey,
            L"hostname",
            0,
            REG_SZ,
            (const BYTE*)host.c_str(),
            (DWORD)(host.size() + 1) * sizeof(wchar_t) // +1 for null terminator
        );

        // --- Save Port ---
        try
        {
            DWORD portValue = std::stoi(PortBox().Text().c_str());
            RegSetValueExW(
                hKey,
                L"port",
                0,
                REG_DWORD,
                (const BYTE*)&portValue,
                sizeof(DWORD)
            );
        }
        catch (...)
        {
            // Handle std::stoi exception if PortBox contains non-numeric text
        }

        RegCloseKey(hKey);

    }

    // -------- UI EVENTS --------


// --- DEBOUNCE LOGIC ---
    void MainWindow::OnConfigTextChanged(IInspectable const&, TextChangedEventArgs const&)
    {
        // Every time text changes, stop the running timer and restart it.
        // This delays the Save/Update until 500ms after the last key press.
        m_debounceTimer.Stop();
        m_debounceTimer.Start();
    }

    // --- RETRY BUTTON LOGIC ---
    void MainWindow::OnRetryClick(IInspectable const&, RoutedEventArgs const&)
    {
        // The retry button simply forces a save and an immediate status update/check.
        SaveConfig();
        UpdateStatusAsync();
    }



    void MainWindow::OnSaveClick(IInspectable const&, RoutedEventArgs const&)
    {
        SaveConfig();
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
        StatusDot().Fill(m_orange);
        StatusText().Text(L"Connecting...");
        InfoText().Text(L"");
        is_connected = false;

        try
        {
            hstring host = HostBox().Text();
            hstring port = PortBox().Text();
            hstring url_info = L"http://" + host + L":" + port + L"/json/info";

            HttpResponseMessage resp_info = co_await m_http.GetAsync(Windows::Foundation::Uri{ url_info });
            if (!resp_info.IsSuccessStatusCode()) co_return;

            hstring body_info = co_await resp_info.Content().ReadAsStringAsync();
            auto info = JsonObject::Parse(body_info);

            auto ver = JsonSafeGetString(info, L"ver", L"?");
            int leds = 0;

            if (info.HasKey(L"leds") && info.GetNamedValue(L"leds").ValueType() == JsonValueType::Object) {
                auto ledsObj = info.GetNamedObject(L"leds");
                leds = JsonSafeGetInt(ledsObj, L"count", 0);
            }
            m_ledCount = leds;

            // --- STATE CALL (for color/brightness) ---
            hstring url_state = L"http://" + host + L":" + port + L"/json/state";
            HttpResponseMessage resp_state = co_await m_http.GetAsync(Windows::Foundation::Uri{ url_state });
            if (!resp_state.IsSuccessStatusCode()) co_return;

            hstring body_state = co_await resp_state.Content().ReadAsStringAsync();
            auto state = JsonObject::Parse(body_state);

            // Brightness
            if (state.HasKey(L"bri") && state.GetNamedValue(L"bri").ValueType() == JsonValueType::Number)
            {
                m_color.A = static_cast<uint8_t>(state.GetNamedNumber(L"bri"));
                BrightnessSlider().Value(m_color.A);
            }

            // Solid color (first segment, first color)
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

                                // Update preview UI
                                Windows::UI::Color newColor{
                                    255,
                                    m_color.R,
                                    m_color.G,
                                    m_color.B
                                };
                                winrt::Microsoft::UI::Xaml::Media::SolidColorBrush brush{ newColor };
                                ColorPreview().Background(brush);
                            }
                        }
                    }
                }
            }

            StatusDot().Fill(m_green);
            StatusText().Text(L"Connected");
            InfoText().Text(L"v" + ver + L" • " + to_hstring(leds) + L" LEDs");
            is_connected = true;

        }
        catch (...)
        {
            StatusDot().Fill(m_red);
            StatusText().Text(L"Failed to connect");
            InfoText().Text(L"");
            is_connected = false;
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