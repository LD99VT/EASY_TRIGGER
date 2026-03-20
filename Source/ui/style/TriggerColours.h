#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trigger
{
// ─── Centralized colour palette ───────────────────────────────────────────────
inline const juce::Colour kBg      = juce::Colour::fromRGB (0x17, 0x17, 0x17);
inline const juce::Colour kRow     = juce::Colour::fromRGB (0x3a, 0x3a, 0x3a);
inline const juce::Colour kSection = juce::Colour::fromRGB (0x65, 0x65, 0x65);
inline const juce::Colour kInput   = juce::Colour::fromRGB (0x24, 0x24, 0x24);
inline const juce::Colour kHeader  = juce::Colour::fromRGB (0x3a, 0x3a, 0x3a);
inline const juce::Colour kTeal    = juce::Colour::fromRGB (0x3d, 0x80, 0x70);

inline const juce::Colour kWindowBorder      = juce::Colour::fromRGB (0x3c, 0x3e, 0x42);
inline const juce::Colour kRowOutline        = juce::Colour::fromRGB (0x2f, 0x2f, 0x2f);
inline const juce::Colour kControlOutline    = juce::Colour::fromRGB (0x5a, 0x5a, 0x5a);
inline const juce::Colour kControlFill       = juce::Colour::fromRGB (0x2a, 0x2a, 0x2a);
inline const juce::Colour kControlIcon       = juce::Colour::fromRGB (0x90, 0x90, 0x90);
inline const juce::Colour kControlArrow      = juce::Colour::fromRGB (0x9a, 0xa1, 0xac);
inline const juce::Colour kMenuHover         = juce::Colour::fromRGBA (0x66, 0x66, 0x66, 0xa0);
inline const juce::Colour kMenuPressed       = juce::Colour::fromRGBA (0x55, 0x55, 0x55, 0xc0);
inline const juce::Colour kSelectionAmber    = juce::Colour::fromRGB (0xc9, 0xa4, 0x3b);

inline const juce::Colour kClipIdle              = juce::Colour::fromRGB (0x24, 0x24, 0x24);
inline juce::Colour       kClipConnectedTc       = juce::Colour::fromRGB (0x42, 0x82, 0x53);
inline juce::Colour       kClipConnectedPlain    = juce::Colour::fromRGB (0x42, 0x66, 0x82);
inline juce::Colour       kClipConnectedCustom   = juce::Colour::fromRGB (0x82, 0x62, 0x62);
inline juce::Colour       kClipFired             = juce::Colour::fromRGB (0xce, 0x9c, 0x00);

// Factory defaults — used by Preferences "Reset"
inline const juce::Colour kDefaultClipFired           = juce::Colour::fromRGB (0xce, 0x9c, 0x00);
inline const juce::Colour kDefaultClipConnectedTc     = juce::Colour::fromRGB (0x42, 0x82, 0x53);
inline const juce::Colour kDefaultClipConnectedPlain  = juce::Colour::fromRGB (0x42, 0x66, 0x82);
inline const juce::Colour kDefaultClipConnectedCustom = juce::Colour::fromRGB (0x82, 0x62, 0x62);

inline const juce::Colour kTextPrimary      = juce::Colour::fromRGB (0xe0, 0xe0, 0xe0);
inline const juce::Colour kTextMuted        = juce::Colour::fromRGB (0xca, 0xca, 0xca);
inline const juce::Colour kTextSecondary    = juce::Colour::fromRGB (0xe4, 0xe4, 0xe4);
inline const juce::Colour kTextDim          = juce::Colour::fromRGB (0x8a, 0x8a, 0x8a);
inline const juce::Colour kTextDisabled     = juce::Colour::fromRGB (0x58, 0x58, 0x60);
inline const juce::Colour kTextDisabledAlt  = juce::Colour::fromRGB (0x9a, 0x9a, 0xa5);
inline const juce::Colour kTextDarkActive   = juce::Colour::fromRGB (0x18, 0x18, 0x18);
inline const juce::Colour kTextAmberDark    = juce::Colour::fromRGB (0x20, 0x14, 0x00);
inline const juce::Colour kBadgeBorder      = juce::Colour::fromRGB (0x50, 0x50, 0x50);
inline const juce::Colour kArrowCircleFill  = juce::Colour::fromRGB (0x48, 0x48, 0x48);
inline const juce::Colour kArrowCircleStroke= juce::Colour::fromRGB (0x5a, 0x5a, 0x5a);
inline const juce::Colour kArrowExpanded    = juce::Colour::fromRGB (0xf2, 0xf2, 0xf2);
inline const juce::Colour kArrowCollapsed   = juce::Colour::fromRGB (0x8b, 0x8b, 0x8b);

inline const juce::Colour kStatusWarn       = juce::Colour::fromRGB (0xec, 0x48, 0x3c);
inline const juce::Colour kStatusError      = juce::Colour::fromRGB (0xde, 0x9b, 0x3c);
inline const juce::Colour kStatusOk         = juce::Colour::fromRGB (0x51, 0xc8, 0x7b);
inline const juce::Colour kStatusInfo       = juce::Colour::fromRGB (0xa0, 0xa4, 0xac);

inline constexpr float kTableRowFontSize    = 14.0f;
inline constexpr float kGroupHeaderFontSize = 13.0f;
inline constexpr float kBadgeFontSize       = 11.0f;
inline constexpr float kSectionHeaderFontSize = 14.0f;
} // namespace trigger
