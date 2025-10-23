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
        void OnColorChanged(winrt::Microsoft::UI::Xaml::Controls::ColorPicker const&, winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const&);
        void OnConfigTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& e);
        void OnRetryClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        void LoadConfig();
        void SaveConfig();
        winrt::hstring ConfigPath();
        void EnsureRegKey();
        winrt::Windows::Foundation::IAsyncAction UpdateStatusAsync();
        winrt::Windows::Foundation::IAsyncAction SendColorAsync(uint8_t r, uint8_t g, uint8_t b);

        winrt::Windows::Web::Http::HttpClient m_http{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_timer{ nullptr };

        int m_ledCount = 0;

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_debounceTimer{ nullptr };
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_throttleTimer{ nullptr };
        bool m_colorUpdatePending = false;
        winrt::Windows::UI::Color m_currentColor{};

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
