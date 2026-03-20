#pragma once

#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace trigger
{
inline void applyNativeDarkTitleBar (juce::DocumentWindow& window)
{
#if JUCE_WINDOWS
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return;

    auto* hwnd = static_cast<HWND> (peer->getNativeHandle());
    if (hwnd == nullptr)
        return;

    auto* dwm = ::LoadLibraryW (L"dwmapi.dll");
    if (dwm == nullptr)
        return;

    using DwmSetWindowAttributeFn = HRESULT (WINAPI*) (HWND, DWORD, LPCVOID, DWORD);
    auto setAttr = reinterpret_cast<DwmSetWindowAttributeFn> (::GetProcAddress (dwm, "DwmSetWindowAttribute"));
    if (setAttr != nullptr)
    {
        const BOOL enabled = TRUE;
        constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
        constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19;
        setAttr (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof (enabled));
        setAttr (hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &enabled, sizeof (enabled));
    }

    ::FreeLibrary (dwm);
#else
    juce::ignoreUnused (window);
#endif
}

inline void hideWindowForShutdown (juce::DocumentWindow& window)
{
    window.setVisible (false);
}

inline bool isNativeWindowMaximizedForShutdown (juce::DocumentWindow& window)
{
#if JUCE_WINDOWS
    if (auto* peer = window.getPeer())
        if (auto* hwnd = static_cast<HWND> (peer->getNativeHandle()))
            return ::IsZoomed (hwnd) != FALSE;
#else
    juce::ignoreUnused (window);
#endif

    return false;
}
} // namespace trigger
