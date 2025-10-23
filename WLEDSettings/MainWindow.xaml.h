#pragma once

#pragma once
#include "MainWindow.g.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Foundation.h>

namespace winrt::WLEDSettings::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void OnSaveClick(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnColorSpectrum_ColorChanged(winrt::WLEDSettings::ColorSpectrum const& sender, Windows::UI::Color const& color);
        void OnConfigTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& e);
        void OnRetryClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnBrightnessSliderChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
   
    private:
        void LoadConfig();
        void SaveConfig();
        winrt::hstring ConfigPath();
        void EnsureRegKey();
        winrt::Windows::Foundation::IAsyncAction UpdateStatusAsync();
        winrt::Windows::Foundation::IAsyncAction SendColorAsync(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

        winrt::Windows::Web::Http::HttpClient m_http{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };

        int m_ledCount = 0;
        winrt::Windows::UI::Color m_color{ 255, 255, 255, 255 };

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_debounceTimer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_throttleTimer{ nullptr };
        bool m_colorUpdatePending = false;
        bool is_connected = false;

        // UI helpers (brushes)
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
