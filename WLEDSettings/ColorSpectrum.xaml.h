#pragma once

#include "ColorSpectrum.g.h"

#include <winrt/Windows.Foundation.h>     // must come first!
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include <algorithm> // std::clamp
#include <cmath>
#include <cstdint>


#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif


namespace winrt::WLEDSettings::implementation
{

    using ColorChangedHandler = Windows::Foundation::TypedEventHandler<WLEDSettings::ColorSpectrum, Windows::UI::Color>;
    
    struct ColorSpectrum : ColorSpectrumT<ColorSpectrum>
    {
        ColorSpectrum();

        // Propri simple (getter/setter)
        Windows::UI::Color SelectedColor() const noexcept { return m_selectedColor; }
        void SelectedColor(Windows::UI::Color const& value);

        // �v�nement "ColorChanged(sender, Windows::UI::Color)"
        winrt::event_token ColorChanged(ColorChangedHandler const& handler);

        // ** NEW: Event unregistration (remove handler) **
        void ColorChanged(winrt::event_token const& token) noexcept;

        // ** NEW: Function to raise the event (for use inside the class) **
        void RaiseColorChanged(Windows::UI::Color const& message);

    private:
        // Rendu du spectre dans un WriteableBitmap (Hue x Value, Saturation=1)
        void RenderSpectrum(int width, int height);
        // Conversion HSV -> Color
        static Windows::UI::Color FromHSV(double h, double s, double v);
        winrt::event<ColorChangedHandler> m_colorChangedEvent;

    public:
        // Interaction
        void OnPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void OnPointerMoved(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void OnPointerReleased(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void OnPointerPressed(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void OnPointerMoved(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void OnPointerReleased(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void UpdateFromPoint(Windows::Foundation::Point const& pt);
        void UpdateIndicator(Windows::Foundation::Point const& pt);

    private:
        Windows::UI::Color m_selectedColor{ 255, 255, 255, 255 };
        bool m_isDragging{ false };

        // R�fs XAML
        Microsoft::UI::Xaml::Controls::Image m_img{ nullptr };
        Microsoft::UI::Xaml::Controls::Border m_hit{ nullptr };
        Microsoft::UI::Xaml::Shapes::Ellipse m_indicator{ nullptr };
        Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap m_bitmap{ nullptr };

    };
}

namespace winrt::WLEDSettings::factory_implementation
{
    struct ColorSpectrum : ColorSpectrumT<ColorSpectrum, implementation::ColorSpectrum>
    {
    };
}
