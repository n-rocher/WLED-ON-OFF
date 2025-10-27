#pragma once

#pragma once
#include "MainWindow.g.h"

#include <windows.h>
#include <microsoft.ui.xaml.window.h>
#include <winreg.h>

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>

#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Foundation.h>

#include <filesystem>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <sstream>

#include <TlHelp32.h>

#pragma comment(lib, "Advapi32.lib")

namespace winrt::WLEDSettings::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_uiQueue{ nullptr };

        std::atomic<bool> m_debouncePending{ false };
        std::chrono::steady_clock::time_point m_lastInputTime;
        std::mutex m_debounceMutex;

        winrt::hstring m_latestHost;
        winrt::hstring m_latestPort;

        void OnSaveClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnColorSpectrum_ColorChanged(winrt::WLEDSettings::ColorSpectrum const& sender, Windows::UI::Color const& color);
        void OnConfigTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& e);
        void OnRetryClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnBrightnessSliderChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
   

    private:
        void EnsureRegKey();
     
        void RestartWLEDHelper();
        void RestartWLEDService();

        void ScheduleDebouncedConfigUpdate();

        void LoadConfig();
        void SaveConfig(winrt::hstring const& host_h, winrt::hstring const& port_h);

        winrt::Windows::Foundation::IAsyncAction UpdateStatusAsync();
        winrt::Windows::Foundation::IAsyncAction SendColorAsync(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

        winrt::Windows::Web::Http::HttpClient m_http{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };

        winrt::Windows::UI::Color m_color{ 255, 255, 255, 255 };

        int m_ledCount = 0;
        bool m_colorUpdatePending = false;
        bool is_connected = false;

        std::atomic<uint64_t> m_statusRequestId{ 0 };

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_debounceTimer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_throttleTimer{ nullptr };


        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_green{ winrt::Microsoft::UI::Colors::Green() };
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_orange{ winrt::Microsoft::UI::Colors::Orange() };
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_red{ winrt::Microsoft::UI::Colors::Red() };
    };
}

namespace winrt::WLEDSettings::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
