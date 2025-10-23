#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include <winrt/Microsoft.UI.Windowing.h> // For AppWindow
#include <winrt/Microsoft.UI.h>           // For WindowId
#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Interop.h>

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

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::WLEDSettings::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = make<MainWindow>();
        window.Activate();

        const int windowWidth = 600;
        const int windowHeight = 550;

        auto windowNative = window.as<IWindowNative>();
        HWND m_hwnd;
        windowNative->get_WindowHandle(&m_hwnd);
        auto windowId = winrt::Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
        auto appWindow = winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
       
        OverlappedPresenter presenter = OverlappedPresenter::Create();

        presenter.IsAlwaysOnTop(false);
        presenter.IsMaximizable(false);
        presenter.IsMinimizable(false);
        presenter.IsResizable(false);
        presenter.SetBorderAndTitleBar(true, true);

        appWindow.SetPresenter(presenter);

        appWindow.Resize({ windowWidth, windowHeight });
        auto displayArea = DisplayArea::GetFromWindowId(windowId, DisplayAreaFallback::Primary);
        auto workArea = displayArea.WorkArea();
        int x = (workArea.Width - windowWidth) / 2;
        int y = (workArea.Height - windowHeight) / 2;
        appWindow.Move({ x, y });
    }
}
