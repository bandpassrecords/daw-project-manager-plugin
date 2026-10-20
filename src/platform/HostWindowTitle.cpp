#include "HostWindowTitle.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace dpm
{

bool isOwnWindowTitle (const juce::String& title)
{
    // The plugin's own editor windows carry its name.
    return title.containsIgnoreCase ("DAW Project Manager");
}

#if JUCE_WINDOWS

namespace
{
    struct Search
    {
        DWORD processId = 0;
        juce::String best;
        long bestArea = 0;
    };

    BOOL CALLBACK visitWindow (HWND hwnd, LPARAM param)
    {
        auto& search = *reinterpret_cast<Search*> (param);

        DWORD pid = 0;
        GetWindowThreadProcessId (hwnd, &pid);
        if (pid != search.processId || ! IsWindowVisible (hwnd) || GetWindow (hwnd, GW_OWNER) != nullptr)
            return TRUE;

        wchar_t buffer[1024] = {};
        if (GetWindowTextW (hwnd, buffer, (int) std::size (buffer)) <= 0)
            return TRUE;

        const juce::String title (buffer);
        if (isOwnWindowTitle (title))
            return TRUE;

        // The main window is the biggest one: transport bars, mixers and
        // plugin windows are smaller.
        RECT r {};
        GetWindowRect (hwnd, &r);
        const auto area = (long) (r.right - r.left) * (long) (r.bottom - r.top);
        if (area > search.bestArea)
        {
            search.bestArea = area;
            search.best = title;
        }
        return TRUE;
    }
}

juce::String readHostWindowTitle()
{
    Search search;
    search.processId = GetCurrentProcessId();
    EnumWindows (visitWindow, reinterpret_cast<LPARAM> (&search));
    return search.best;
}

#elif ! JUCE_MAC

juce::String readHostWindowTitle()
{
    // X11/Wayland give a plugin no reliable way to its host's window title;
    // linking falls back to the app's launch hint and the picker.
    return {};
}

#endif

} // namespace dpm
