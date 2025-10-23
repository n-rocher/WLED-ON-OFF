#include "pch.h"
#include "ColorSpectrum.xaml.h"
#if __has_include("ColorSpectrum.g.cpp")
#include "ColorSpectrum.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Windows::Foundation;
using namespace Windows::UI;


namespace winrt::WLEDSettings::implementation
{
    ColorSpectrum::ColorSpectrum()
    {
        InitializeComponent();

        m_img = FindName(L"PART_SpectrumImage").as<Image>();
        m_hit = FindName(L"PART_HitSurface").as<Border>();
        m_indicator = FindName(L"PART_Indicator").as<Microsoft::UI::Xaml::Shapes::Ellipse>();

        // Recr�er le spectre quand la taille change
        RootGrid().SizeChanged([this](IInspectable const&, SizeChangedEventArgs const& e)
            {
                const int w = std::max(1, (int)std::round(e.NewSize().Width));
                const int h = std::max(1, (int)std::round(e.NewSize().Height));
                RenderSpectrum(w, h);
            });

        // Premier rendu si on a d�j� une taille
        auto sz = RootGrid().ActualSize();
        int w = (std::max)(1, (int)std::round(sz.x));
        int h = (std::max)(1, (int)std::round(sz.y));
        if (w > 0 && h > 0) RenderSpectrum(w, h);
    }

    void ColorSpectrum::SelectedColor(Color const& value)
    {
        if (m_selectedColor.A != value.A ||
            m_selectedColor.R != value.R ||
            m_selectedColor.G != value.G ||
            m_selectedColor.B != value.B)
        {
            m_selectedColor = value;
            RaiseColorChanged(m_selectedColor);
        }
    }

    // ========================= Rendu du spectre =========================
    void ColorSpectrum::RenderSpectrum(int width, int height)
    {
        if (width <= 0 || height <= 0) return;

        m_bitmap = WriteableBitmap(width, height);
        auto buffer = m_bitmap.PixelBuffer();
        auto bytes = buffer.data();
        const int stride = width * 4; // BGRA8

        for (int y = 0; y < height; ++y)
        {
            // double v = 1.0 - (double)y / (double)(height - 1); // top=1 -> bottom=0

            double s = 1.0 - (double)y / (double)(height - 1);
            double v = 1.0;

            for (int x = 0; x < width; ++x)
            {
                double h = (double)x / (double)(width - 1) * 360.0; // 0..360
                //double s = 1.0; // Saturation fixe = 1
                auto c = FromHSV(h, s, v);

                // BGRA
                size_t idx = (size_t)y * stride + (size_t)x * 4;
                bytes[idx + 0] = c.B;
                bytes[idx + 1] = c.G;
                bytes[idx + 2] = c.R;
                bytes[idx + 3] = c.A;
            }
        }
        m_bitmap.Invalidate(); // push vers l'UI
        if (m_img) m_img.Source(m_bitmap);
    }

    Windows::UI::Color ColorSpectrum::FromHSV(double h, double s, double v)
    {
        // Normalise
        while (h < 0) h += 360.0;
        while (h >= 360.0) h -= 360.0;

        double C = v * s;
        double X = C * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
        double m = v - C;

        double r = 0, g = 0, b = 0;
        if (0.0 <= h && h < 60.0) { r = C; g = X; b = 0; }
        else if (60.0 <= h && h < 120.0) { r = X; g = C; b = 0; }
        else if (120.0 <= h && h < 180.0) { r = 0; g = C; b = X; }
        else if (180.0 <= h && h < 240.0) { r = 0; g = X; b = C; }
        else if (240.0 <= h && h < 300.0) { r = X; g = 0; b = C; }
        else { r = C; g = 0; b = X; }

        auto toByte = [](double f) -> uint8_t
            {
                f = std::clamp(f, 0.0, 1.0);
                return static_cast<uint8_t>(std::round(f * 255.0));
            };

        Color col{};
        col.A = 255;
        col.R = toByte(r + m);
        col.G = toByte(g + m);
        col.B = toByte(b + m);
        return col;
    }

    // ========================= Interaction =========================
    void ColorSpectrum::OnPointerPressed(winrt::Windows::Foundation::IInspectable const& sender, PointerRoutedEventArgs const& e)
    {
        if (!m_hit) return;
        auto pt = e.GetCurrentPoint(m_hit).Position();
        m_isDragging = true;
        UpdateFromPoint(pt);
        UpdateIndicator(pt);
        m_hit.CapturePointer(e.Pointer());
    }

    void ColorSpectrum::OnPointerMoved(winrt::Windows::Foundation::IInspectable const& sender, PointerRoutedEventArgs const& e)
    {
        if (!m_isDragging || !m_hit) return;
        auto pt = e.GetCurrentPoint(m_hit).Position();
        UpdateFromPoint(pt);
        UpdateIndicator(pt);
    }

    void ColorSpectrum::OnPointerReleased(winrt::Windows::Foundation::IInspectable const& sender, PointerRoutedEventArgs const& e)
    {
        m_isDragging = false;
        m_hit.ReleasePointerCapture(e.Pointer());
    }

    void ColorSpectrum::OnPointerPressed(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        if (!m_hit) return;
        auto pt = e.GetCurrentPoint(m_hit).Position();
        m_isDragging = true;
        UpdateFromPoint(pt);
        UpdateIndicator(pt);
        m_hit.CapturePointer(e.Pointer());
    }

    void ColorSpectrum::OnPointerMoved(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        if (!m_isDragging || !m_hit) return;
        auto pt = e.GetCurrentPoint(m_hit).Position();
        UpdateFromPoint(pt);
        UpdateIndicator(pt);
    }

    void ColorSpectrum::OnPointerReleased(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        m_isDragging = false;
        m_hit.ReleasePointerCapture(e.Pointer());
    }

    void ColorSpectrum::UpdateFromPoint(Point const& pt)
    {
        if (!m_hit) return;
        double w = std::max(1.0, m_hit.ActualWidth());
        double h = std::max(1.0, m_hit.ActualHeight());

        double x = std::clamp((double)pt.X, 0.0, w);
        double y = std::clamp((double)pt.Y, 0.0, h);

        double hue = (x / w) * 360.0;
        double val = 1 - (y / (h - 1));
        auto color = FromHSV(hue, val, 1.0);
        SelectedColor(color);
    }

    void ColorSpectrum::UpdateIndicator(Point const& pt)
    {
        if (!m_indicator || !m_hit) return;

        double x = std::clamp((double)pt.X, 0.0, m_hit.ActualWidth());
        double y = std::clamp((double)pt.Y, 0.0, m_hit.ActualHeight());

        Canvas::SetLeft(m_indicator, x - m_indicator.Width() / 2.0);
        Canvas::SetTop(m_indicator, y - m_indicator.Height() / 2.0);
        m_indicator.Visibility(Visibility::Visible);
    }

    // ** NEW: Event registration (Add method) **
    winrt::event_token ColorSpectrum::ColorChanged(ColorChangedHandler const& handler)
    {
        return m_colorChangedEvent.add(handler);
    }

    // ** NEW: Event unregistration (Remove method) **
    void ColorSpectrum::ColorChanged(winrt::event_token const& token) noexcept
    {
        m_colorChangedEvent.remove(token);
    }

    // ** NEW: Helper function to raise the event **
    void ColorSpectrum::RaiseColorChanged(Windows::UI::Color const& message) // <-- Change Windows.UI.Color to Windows::UI::Color
    {
        // Invoke the event: m_colorChangedEvent(sender, args)
        m_colorChangedEvent(*this, message);
    }


}
