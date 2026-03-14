#include "TriggerMainWindow.h"
using trigger::styleCombo;
using trigger::styleEditor;
using trigger::styleSlider;
#include "Version.h"
#include "core/ConfigStore.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <thread>

#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace trigger
{
namespace
{
juce::Colour clipIdleFillFor (bool isCustom, bool hasOffset, bool included)
{
    juce::ignoreUnused (isCustom, hasOffset, included);
    return juce::Colour::fromRGB (0x24, 0x24, 0x24);
}

juce::Colour clipConnectedFillFor (bool isCustom, bool hasOffset)
{
    if (isCustom)
        return juce::Colour::fromRGB (0x82, 0x62, 0x62);

    if (hasOffset)
        return juce::Colour::fromRGB (0x42, 0x82, 0x53);

    return juce::Colour::fromRGB (0x42, 0x66, 0x82);
}

juce::FontOptions clipRowFont()
{
    return juce::FontOptions (14.0f).withStyle ("Bold");
}
}

void FpsIndicatorStrip::setActiveFps (std::optional<FrameRate> fps)
{
    if (activeFps_ == fps)
        return;

    activeFps_ = fps;
    repaint();
}

juce::String FpsIndicatorStrip::getActiveFpsText() const
{
    return activeFps_.has_value() ? frameRateToString (*activeFps_) : "--";
}

FpsConvertStrip::FpsConvertStrip (std::initializer_list<FrameRate> availableRates)
{
    for (auto fps : availableRates)
        availableRates_.add (fps);
}

void FpsConvertStrip::setSelectedFps (std::optional<FrameRate> fps)
{
    if (selectedFps_ == fps)
        return;

    selectedFps_ = fps;
    repaint();
}

void FpsIndicatorStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    if (area.isEmpty())
        return;

    const juce::String labels[] = { "23.976 FPS", "24 FPS", "25 FPS", "29.97 FPS", "30 FPS" };
    const FrameRate values[] = { FrameRate::FPS_2398, FrameRate::FPS_24, FrameRate::FPS_25, FrameRate::FPS_2997, FrameRate::FPS_30 };

    constexpr int gap = 6;
    auto buttonsArea = area;
    const int buttonWidth = juce::jmax (1, (buttonsArea.getWidth() - gap * 4) / 5);
    const auto inactiveBg = juce::Colour::fromRGB (0x1a, 0x1a, 0x1a);
    const auto inactiveText = juce::Colour::fromRGB (0xca, 0xca, 0xca);
    const auto activeBg = kSection;
    const auto activeText = juce::Colour::fromRGB (0x18, 0x18, 0x18);
    const auto border = juce::Colour::fromRGB (0x54, 0x54, 0x54);
    auto font = juce::FontOptions (13.5f);
    font = font.withStyle ("Medium");

    for (int i = 0; i < 5; ++i)
    {
        auto cell = buttonsArea.removeFromLeft (buttonWidth);
        if (i < 4)
            buttonsArea.removeFromLeft (gap);

        const bool isActive = activeFps_.has_value() && *activeFps_ == values[i];
        g.setColour (isActive ? activeBg : inactiveBg);
        g.fillRoundedRectangle (cell.toFloat(), 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (cell.toFloat(), 6.0f, 1.0f);
        g.setColour (isActive ? activeText : inactiveText);
        g.setFont (font);
        g.drawFittedText (labels[i], cell, juce::Justification::centred, 1);
    }
}

void FpsConvertStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    if (area.isEmpty() || availableRates_.isEmpty())
        return;

    constexpr int gap = 6;
    const int buttonCount = availableRates_.size();
    const int buttonWidth = juce::jmax (1, (area.getWidth() - gap * (buttonCount - 1)) / buttonCount);
    auto buttonsArea = area;
    const auto inactiveBg = kInput;
    const auto inactiveText = juce::Colour::fromRGB (0xca, 0xca, 0xca);
    const auto activeBg = kTeal;
    const auto activeText = kBg;
    auto font = juce::FontOptions (14.0f);
    font = font.withStyle ("Medium");

    for (int i = 0; i < buttonCount; ++i)
    {
        auto cell = buttonsArea.removeFromLeft (buttonWidth);
        if (i < buttonCount - 1)
            buttonsArea.removeFromLeft (gap);

        const bool isActive = selectedFps_.has_value() && *selectedFps_ == availableRates_[i];
        g.setColour (isActive ? activeBg : inactiveBg);
        g.fillRoundedRectangle (cell.toFloat(), 6.0f);
        g.setColour (isActive ? activeText : inactiveText);
        g.setFont (font);
        g.drawFittedText (frameRateToString (availableRates_[i]), cell, juce::Justification::centred, 1);
    }
}

void FpsConvertStrip::mouseUp (const juce::MouseEvent& event)
{
    auto area = getLocalBounds();
    if (area.isEmpty() || availableRates_.isEmpty())
        return;

    constexpr int gap = 6;
    const int buttonCount = availableRates_.size();
    const int buttonWidth = juce::jmax (1, (area.getWidth() - gap * (buttonCount - 1)) / buttonCount);
    auto buttonsArea = area;

    for (int i = 0; i < buttonCount; ++i)
    {
        auto cell = buttonsArea.removeFromLeft (buttonWidth);
        if (i < buttonCount - 1)
            buttonsArea.removeFromLeft (gap);

        if (! cell.contains (event.getPosition()))
            continue;

        const auto clicked = availableRates_[i];
        const auto next = (selectedFps_.has_value() && *selectedFps_ == clicked)
            ? std::optional<FrameRate> {}
            : std::optional<FrameRate> { clicked };
        setSelectedFps (next);
        if (onChange)
            onChange (next);
        return;
    }
}

namespace
{
constexpr int kPlaceholderItemId = 10000;
constexpr int kCompactBarHeight = 24;
constexpr int kStartupHeightExtra = 8;
constexpr int kConfigModeSettings = 1;
constexpr int kConfigModeClips = 2;
constexpr int kConfigModeAll = 3;

juce::String configModeKey (int modeId)
{
    if (modeId == kConfigModeSettings) return "settings";
    if (modeId == kConfigModeClips) return "clips";
    return "all";
}

juce::String parseBindIpFromAdapterLabel (juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        return "0.0.0.0";

    const auto lower = text.toLowerCase();
    if (lower.contains ("loopback"))
        return "127.0.0.1";
    if (lower.contains ("all interfaces"))
        return "0.0.0.0";

    const int open = text.lastIndexOfChar ('(');
    const int close = text.lastIndexOfChar (')');
    if (open >= 0 && close > open)
    {
        const auto ip = text.substring (open + 1, close).trim();
        if (ip.isNotEmpty())
            return ip;
    }

    return "0.0.0.0";
}

void setupDbSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    s.setRange (-24, 24, 0.1);
    s.setValue (0.0);
    s.setTextValueSuffix (" dB");
    s.setDoubleClickReturnValue (true, 0.0);
}

void fillRateCombo (juce::ComboBox& combo)
{
    combo.clear();
    combo.addItem ("Default", 1);
    combo.addItem ("44100", 2);
    combo.addItem ("48000", 3);
    combo.addItem ("88200", 4);
    combo.addItem ("96000", 5);
    combo.addItem ("176400", 6);
    combo.addItem ("192000", 7);
    combo.setSelectedId (1, juce::dontSendNotification);
}

void fillChannelCombo (juce::ComboBox& combo, int channelCount)
{
    combo.clear();
    if (channelCount <= 0)
    {
        combo.addItem ("(No channels)", kPlaceholderItemId);
        combo.setSelectedId (kPlaceholderItemId, juce::dontSendNotification);
        return;
    }

    for (int i = 1; i <= channelCount; ++i)
        combo.addItem (juce::String (i), i);

    for (int start = 0; start + 1 < channelCount; start += 2)
        combo.addItem (juce::String (start + 1) + "+" + juce::String (start + 2), 1000 + start);

    combo.setSelectedId (1, juce::dontSendNotification);
}

juce::String normalizeDriverKey (juce::String s)
{
    s = s.toLowerCase().trim();
    if (s.contains ("asio")) return "asio";
    if (s.contains ("directsound")) return "directsound";
    if (s.contains ("coreaudio")) return "coreaudio";
    if (s.contains ("alsa")) return "alsa";
    if (s.contains ("wasapi") || s.contains ("windows audio")) return "windowsaudio";
    return s;
}

bool matchesDriverFilter (const juce::String& driverUi, const juce::String& typeName)
{
    const auto d = normalizeDriverKey (driverUi);
    if (d.startsWith ("default"))
        return true;
    const auto t = normalizeDriverKey (typeName);
    return t == d;
}

void fillDriverCombo (juce::ComboBox& combo, const juce::Array<bridge::engine::AudioChoice>& choices, const juce::String& previousText)
{
    combo.clear();
    combo.addItem ("Default (all devices)", 1);

    juce::StringArray seen;
    for (const auto& c : choices)
    {
        if (c.typeName.isNotEmpty() && ! seen.contains (c.typeName))
            seen.add (c.typeName);
    }

    seen.sortNatural();
    for (int i = 0; i < seen.size(); ++i)
        combo.addItem (seen[i], i + 2);

    if (previousText.isNotEmpty())
    {
        for (int i = 0; i < combo.getNumItems(); ++i)
        {
            if (combo.getItemText (i) == previousText)
            {
                combo.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
        }
    }

    combo.setSelectedId (1, juce::dontSendNotification);
}

float dbToLinearGain (double db)
{
    return (float) std::pow (10.0, db / 20.0);
}

int offsetFromEditor (const juce::TextEditor& editor)
{
    return juce::jlimit (-30, 30, editor.getText().getIntValue());
}

void selectComboItemByText (juce::ComboBox& combo, const juce::String& text)
{
    for (int i = 0; i < combo.getNumItems(); ++i)
    {
        if (combo.getItemText (i) == text)
        {
            combo.setSelectedItemIndex (i, juce::dontSendNotification);
            return;
        }
    }
}

juce::String formatDisplayedInputFps (bool hasLatchedTc, FrameRate latchedFps, FpsIndicatorStrip* strip)
{
    if (strip != nullptr)
        return strip->getActiveFpsText();

    return hasLatchedTc ? frameRateToString (latchedFps) : "--";
}

std::optional<FrameRate> fpsFromString (juce::String text)
{
    text = text.trim();
    if (text.isEmpty())
        return std::nullopt;

    if (text == "23.976") return FrameRate::FPS_2398;
    if (text == "24")     return FrameRate::FPS_24;
    if (text == "25")     return FrameRate::FPS_25;
    if (text == "29.97")  return FrameRate::FPS_2997;
    if (text == "30")     return FrameRate::FPS_30;
    return std::nullopt;
}

int globalOffsetFramesFromEditor (const juce::TextEditor& editor, int fps)
{
    auto text = editor.getText().trim();
    if (text.isEmpty())
        return 0;

    int sign = 1;
    if (text.startsWithChar ('-'))
    {
        sign = -1;
        text = text.fromFirstOccurrenceOf ("-", false, false).trim();
    }
    else if (text.startsWithChar ('+'))
    {
        text = text.fromFirstOccurrenceOf ("+", false, false).trim();
    }

    juce::StringArray p;
    p.addTokens (text, ":", "");
    p.removeEmptyStrings();
    if (p.size() != 4)
        return 0;

    const int hh = p[0].getIntValue();
    const int mm = p[1].getIntValue();
    const int ss = p[2].getIntValue();
    const int ff = p[3].getIntValue();
    if (hh < 0 || mm < 0 || mm > 59 || ss < 0 || ss > 59 || ff < 0 || ff >= fps)
        return 0;

    return sign * ((((hh * 60) + mm) * 60 + ss) * fps + ff);
}

#if JUCE_WINDOWS
void applyNativeDarkTitleBar (juce::DocumentWindow& window)
{
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
}

bool isNativeWindowMaximized (juce::DocumentWindow& window)
{
    if (auto* peer = window.getPeer())
        if (auto* hwnd = static_cast<HWND> (peer->getNativeHandle()))
            return ::IsZoomed (hwnd) != FALSE;
    return false;
}
#endif

juce::File findUiBaseDirFromExe()
{
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    juce::Array<juce::File> roots;
    roots.add (exeDir);
    roots.add (juce::File::getCurrentWorkingDirectory());

    auto p = exeDir;
    for (int i = 0; i < 8 && p.exists(); ++i)
    {
        roots.addIfNotAlreadyThere (p);
        p = p.getParentDirectory();
    }

    for (auto r : roots)
    {
        auto hasAssets = [&] (const juce::File& dir)
        {
            return dir.getChildFile ("Fonts").exists()
                && dir.getChildFile ("Help").exists();
        };

        if (hasAssets (r))
            return r;

        auto trigger = r.getChildFile ("EASYTRIGGER-JYCE");
        if (trigger.exists() && hasAssets (trigger))
            return trigger;

        trigger = r.getChildFile ("EasyTrigger");
        if (trigger.exists() && hasAssets (trigger))
            return trigger;

    }

    return {};
}

juce::Image loadTriggerAppIcon()
{
    auto base = findUiBaseDirFromExe();
    juce::File icon;

#if JUCE_WINDOWS
    if (! base.exists())
        return {};
    icon = base.getChildFile ("Icon/Icon Trigger.ico");
#elif JUCE_MAC
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto resourcesDir = exeDir.getParentDirectory().getChildFile ("Resources");
    if (resourcesDir.isDirectory())
        icon = resourcesDir.getChildFile ("Icon Trigger.icns");

    if (! icon.existsAsFile() && base.exists())
        icon = base.getChildFile ("Icon/Icon Trigger.icns");
#else
    if (! base.exists())
        return {};
    icon = base.getChildFile ("Icon/Icon Trigger.png");
#endif

    if (! icon.existsAsFile() && base.exists())
        icon = base.getChildFile ("Icon/Icon.png");
    if (! icon.existsAsFile() && base.exists())
        icon = base.getChildFile ("Icons/App_Icon.png");
    if (! icon.existsAsFile())
        return {};

    auto in = std::unique_ptr<juce::FileInputStream> (icon.createInputStream());
    if (in == nullptr)
        return {};
    return juce::ImageFileFormat::loadFrom (*in);
}

juce::File getRuntimePrefsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("EasyTrigger")
        .getChildFile ("runtime_prefs.json");
}

class TriggerTrayIcon final : public juce::SystemTrayIconComponent
{
public:
    explicit TriggerTrayIcon (MainWindow& owner) : owner_ (owner) {}

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Show");
            menu.addSeparator();
            menu.addItem (2, "Quit");
            menu.showMenuAsync (juce::PopupMenu::Options(),
                                [safeOwner = juce::Component::SafePointer<MainWindow> (&owner_)] (int result)
                                {
                                    if (safeOwner == nullptr)
                                        return;
                                    if (result == 1)
                                        safeOwner->showFromTray();
                                    else if (result == 2)
                                        safeOwner->quitFromTray();
                                });
            return;
        }

        owner_.showFromTray();
    }

private:
    MainWindow& owner_;
};

class InlineTextCell final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> onCommit;
    InlineTextCell()
    {
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        setBorder (juce::BorderSize<int> (0));
        setIndents (4, 0);
        setJustification (juce::Justification::centredLeft);
        onReturnKey = [this] { if (onCommit) onCommit (getText()); };
        onFocusLost = [this] { if (onCommit) onCommit (getText()); };
    }
};

class InlineButtonCell final : public juce::TextButton
{
public:
    std::function<void()> onPress;
    InlineButtonCell()
    {
        onClick = [this] { if (onPress) onPress(); };
    }
};

class InlineTestButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;

    InlineTestButtonCell() : juce::Button ("test")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (30.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());

        auto fill = juce::Colour::fromRGB (0x2a, 0x2a, 0x2a);
        auto stroke = juce::Colour::fromRGB (0x4a, 0x4a, 0x4a);
        auto icon = juce::Colour::fromRGB (0x90, 0x90, 0x90);
        if (isHovered) fill = fill.brighter (0.12f);
        if (isDown) fill = fill.brighter (0.18f);

        g.setColour (fill);
        g.fillRoundedRectangle (sq, 5.0f);
        g.setColour (stroke);
        g.drawRoundedRectangle (sq, 5.0f, 1.0f);

        juce::Path tri;
        const auto cx = sq.getCentreX();
        const auto cy = sq.getCentreY();
        const float w = 7.0f;
        const float h = 9.0f;
        tri.startNewSubPath (cx - w * 0.5f, cy - h * 0.5f);
        tri.lineTo (cx - w * 0.5f, cy + h * 0.5f);
        tri.lineTo (cx + w * 0.6f, cy);
        tri.closeSubPath();
        g.setColour (icon);
        g.fillPath (tri);
    }
};

class InlineDeleteButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;
    InlineDeleteButtonCell() : juce::Button ("del")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (28.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());
        auto fill   = juce::Colour::fromRGB (0x2a, 0x2a, 0x2a);
        auto stroke  = juce::Colour::fromRGB (0x4a, 0x4a, 0x4a);
        auto icon    = juce::Colour::fromRGB (0x90, 0x90, 0x90);
        if (isHovered || isDown)
        {
            fill = isDown ? kTeal.darker (0.15f) : kTeal;
            icon = kBg;
        }
        g.setColour (fill);
        g.fillRoundedRectangle (sq, 5.0f);
        g.setColour (stroke);
        g.drawRoundedRectangle (sq, 5.0f, 1.0f);
        const float cx = sq.getCentreX();
        const float cy = sq.getCentreY();
        g.setColour (icon);
        g.fillRect (juce::Rectangle<float> (cx - 5.5f, cy - 1.0f, 11.0f, 2.0f));
    }
};

class InlineAddButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;
    InlineAddButtonCell() : juce::Button ("add")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (28.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());
        auto fill   = juce::Colour::fromRGB (0x2a, 0x2a, 0x2a);
        auto stroke  = juce::Colour::fromRGB (0x4a, 0x4a, 0x4a);
        auto icon    = juce::Colour::fromRGB (0x90, 0x90, 0x90);
        if (isHovered || isDown)
        {
            fill = isDown ? kTeal.darker (0.15f) : kTeal;
            icon = kBg;
        }
        g.setColour (fill);   g.fillRoundedRectangle (sq, 5.0f);
        g.setColour (stroke); g.drawRoundedRectangle (sq, 5.0f, 1.0f);
        const float cx = sq.getCentreX(), cy = sq.getCentreY();
        g.setColour (icon);
        g.fillRect (juce::Rectangle<float> (cx - 5.5f, cy - 1.0f, 11.0f, 2.0f));
        g.fillRect (juce::Rectangle<float> (cx - 1.0f, cy - 5.5f,  2.0f, 11.0f));
    }
};

// Custom trigger source selector: Col toggle (Col / L/C) + inline number fields
class InlineCustomTypeCell final : public juce::Component
{
public:
    std::function<void(const juce::String&, const juce::String&, const juce::String&, const juce::String&)> onChanged;
    // callback params: type("col"/"lc"), sourceCol, sourceLayer, sourceClip

    InlineCustomTypeCell()
    {
        addAndMakeVisible (typeBtn_);
        addAndMakeVisible (field1_);
        addAndMakeVisible (field2_);

        typeBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
        typeBtn_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        typeBtn_.onClick = [this]
        {
            type_ = (type_ == "col") ? "lc" : "col";
            applyTypeToUi();
            emitChanged();
        };

        auto styleField = [] (juce::TextEditor& e)
        {
            e.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0x24, 0x24, 0x24));
            e.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
            e.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
            e.setJustification (juce::Justification::centredLeft);
            e.setBorder (juce::BorderSize<int> (0));
            e.setIndents (4, 0);
            e.setFont (juce::FontOptions (13.0f));
            e.onReturnKey = [&e] { e.giveAwayKeyboardFocus(); };
        };
        styleField (field1_);
        styleField (field2_);
        field1_.onFocusLost = [this] { emitChanged(); };
        field2_.onFocusLost = [this] { emitChanged(); };
    }

    void setState (const juce::String& type, const juce::String& col,
                   const juce::String& layer, const juce::String& clip)
    {
        type_  = (type == "lc") ? "lc" : "col";
        col_   = col;
        layer_ = layer;
        clip_  = clip;
        field1_.setText (type_ == "col" ? col_ : layer_, juce::dontSendNotification);
        field2_.setText (clip_, juce::dontSendNotification);
        applyTypeToUi();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1, 4);
        typeBtn_.setBounds (r.removeFromLeft (62));
        r.removeFromLeft (4);
        if (type_ == "lc")
        {
            const int w = juce::jmin (52, juce::jmax (38, (r.getWidth() - 4) / 2));
            field1_.setBounds (r.removeFromLeft (w));
            r.removeFromLeft (4);
            field2_.setBounds (r.removeFromLeft (w));
        }
        else
        {
            field1_.setBounds (r.removeFromLeft (juce::jmin (66, r.getWidth())));
            field2_.setBounds (0, 0, 0, 0);
        }
    }

private:
    void applyTypeToUi()
    {
        if (type_ == "lc")
        {
            typeBtn_.setButtonText ("L/C");
            field1_.setVisible (true);
            field2_.setVisible (true);
        }
        else
        {
            type_ = "col";
            typeBtn_.setButtonText ("Col");
            field1_.setVisible (true);
            field2_.setVisible (false);
        }
        resized();
    }

    void emitChanged()
    {
        if (type_ == "col")
        {
            col_   = field1_.getText().trim();
            layer_.clear();
            clip_.clear();
        }
        else
        {
            layer_ = field1_.getText().trim();
            clip_  = field2_.getText().trim();
            col_.clear();
        }
        if (onChanged) onChanged (type_, col_, layer_, clip_);
    }

    juce::TextButton typeBtn_;
    juce::TextEditor field1_;
    juce::TextEditor field2_;
    juce::String type_  { "col" };
    juce::String col_, layer_, clip_;
};

// ─── Generic dark-themed modal dialog ────────────────────────────────────────
// Self-owning (heap-allocated, deletes itself on close).
// Usage:  DarkDialog::show ("Title", "Message text", parentComponent);
class DarkDialog final : public juce::DocumentWindow
{
public:
    static void show (const juce::String& title,
                      const juce::String& message,
                      juce::Component*    relativeTo = nullptr,
                      const juce::String& buttonText = "OK")
    {
        new DarkDialog (title, message, buttonText, relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<DarkDialog> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

private:
    DarkDialog (const juce::String& title,
                const juce::String& message,
                const juce::String& buttonText,
                juce::Component*    relativeTo)
        : juce::DocumentWindow (title,
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, message, buttonText), true);
        centreWithSize (400, 160);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 200, rc.getCentreY() - 80, 400, 160);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
        if (auto* hwnd = (HWND) getWindowHandle())
        {
            ::SendMessageW (hwnd, WM_SETICON, 0, 0);
            ::SendMessageW (hwnd, WM_SETICON, 1, 0);
            constexpr long kGwlStyle = -16;
            long st = (long) ::GetWindowLongPtrW (hwnd, kGwlStyle);
            st &= ~(long) 0x00040000L;  // WS_THICKFRAME
            st &= ~(long) 0x00010000L;  // WS_MAXIMIZEBOX
            ::SetWindowLongPtrW (hwnd, kGwlStyle, st);
            ::SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif
        toFront (true);
    }

    struct Content final : juce::Component
    {
        DarkDialog&      owner_;
        juce::Label      msg_;
        juce::TextButton btn_;

        Content (DarkDialog& o, const juce::String& message, const juce::String& btnText)
            : owner_ (o), btn_ (btnText)
        {
            msg_.setText (message, juce::dontSendNotification);
            msg_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe0, 0xe0, 0xe0));
            msg_.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (msg_);

            btn_.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btn_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btn_.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btn_.setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btn_.onClick = [this] { owner_.closeButtonPressed(); };
            addAndMakeVisible (btn_);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            constexpr int kPad = 20;
            msg_.setBounds (kPad, kPad, getWidth() - kPad * 2, getHeight() - 70);
            btn_.setBounds ((getWidth() - 100) / 2, getHeight() - 42, 100, 32);
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkDialog)
};

// ─── Live Trigger Status Monitor window ─────────────────────────────────────
class StatusMonitorWindow final : public juce::DocumentWindow,
                                  private juce::Timer
{
public:
    using Getter = std::function<void (juce::Array<juce::String>&, juce::Array<juce::String>&)>;

    StatusMonitorWindow (Getter getter, juce::Component* relativeTo)
        : juce::DocumentWindow ("Trigger Status Monitor",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton),
          getter_ (std::move (getter))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this), true);
        centreWithSize (420, 390);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 210, rc.getCentreY() - 195, 420, 390);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
        if (auto* hwnd = (HWND) getWindowHandle())
        {
            ::SendMessageW (hwnd, WM_SETICON, 0, 0);
            ::SendMessageW (hwnd, WM_SETICON, 1, 0);
            constexpr long kGwlStyle = -16;
            long st = (long) ::GetWindowLongPtrW (hwnd, kGwlStyle);
            st &= ~(long) 0x00040000L;  // WS_THICKFRAME
            st &= ~(long) 0x00010000L;  // WS_MAXIMIZEBOX
            ::SetWindowLongPtrW (hwnd, kGwlStyle, st);
            ::SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif
        toFront (true);
        startTimerHz (5);
    }

    void closeButtonPressed() override { delete this; }

    void timerCallback() override
    {
        if (auto* c = dynamic_cast<Content*> (getContentComponent()))
            c->refresh();
    }

    void getValues (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals)
    {
        getter_ (keys, vals);
    }

private:
    Getter getter_;

    static constexpr int kRows = 10;

    struct Content final : juce::Component
    {
        StatusMonitorWindow& win_;

        juce::Label keyLbls_[kRows];
        juce::Label valLbls_[kRows];
        juce::TextButton ok_ { "OK" };

        explicit Content (StatusMonitorWindow& w) : win_ (w)
        {
            const juce::Colour keyCol = juce::Colour::fromRGB (0x84, 0x84, 0x84);
            const juce::Colour valCol = juce::Colour::fromRGB (0xe0, 0xe0, 0xe0);

            for (int i = 0; i < kRows; ++i)
            {
                keyLbls_[i].setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
                keyLbls_[i].setColour (juce::Label::textColourId, keyCol);
                keyLbls_[i].setJustificationType (juce::Justification::centredRight);
                addAndMakeVisible (keyLbls_[i]);

                valLbls_[i].setFont (juce::FontOptions (12.5f));
                valLbls_[i].setColour (juce::Label::textColourId, valCol);
                valLbls_[i].setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (valLbls_[i]);
            }

            ok_.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            ok_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            ok_.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
            ok_.setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
            ok_.onClick = [this]
            {
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
            addAndMakeVisible (ok_);

            refresh();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            constexpr int kRowH = 30;
            constexpr int kPad  = 14;
            constexpr int kKeyW = 120;
            constexpr int kGap  = 10;
            const int valW = getWidth() - kPad * 2 - kKeyW - kGap;

            for (int i = 0; i < kRows; ++i)
            {
                const int y = kPad + i * kRowH;
                keyLbls_[i].setBounds (kPad, y, kKeyW, kRowH);
                valLbls_[i].setBounds (kPad + kKeyW + kGap, y, valW, kRowH);
            }

            const int btnY = kPad + kRows * kRowH + kPad;
            ok_.setBounds ((getWidth() - 100) / 2, btnY, 100, 32);
        }

        void refresh()
        {
            juce::Array<juce::String> keys, vals;
            win_.getValues (keys, vals);
            for (int i = 0; i < juce::jmin (kRows, keys.size()); ++i)
            {
                keyLbls_[i].setText (keys[i], juce::dontSendNotification);
                valLbls_[i].setText (vals[i], juce::dontSendNotification);
            }
        }
    };
};
// ─────────────────────────────────────────────────────────────────────────────

class GetClipsOptionsWindow final : public juce::DocumentWindow
{
public:
    using ApplyHandler = std::function<void (bool, bool)>;

    GetClipsOptionsWindow (bool includeWithOffset,
                           bool includeWithoutOffset,
                           ApplyHandler onApply,
                           juce::Component* relativeTo)
        : juce::DocumentWindow ("Get Clips",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton),
          onApply_ (std::move (onApply))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, includeWithOffset, includeWithoutOffset), true);
        centreWithSize (360, 190);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 180, rc.getCentreY() - 95, 360, 190);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
        if (auto* hwnd = (HWND) getWindowHandle())
        {
            ::SendMessageW (hwnd, WM_SETICON, 0, 0);
            ::SendMessageW (hwnd, WM_SETICON, 1, 0);
            constexpr long kGwlStyle = -16;
            long st = (long) ::GetWindowLongPtrW (hwnd, kGwlStyle);
            st &= ~(long) 0x00040000L;
            st &= ~(long) 0x00010000L;
            ::SetWindowLongPtrW (hwnd, kGwlStyle, st);
            ::SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif
        toFront (true);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<GetClipsOptionsWindow> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

    void applySelection (bool includeWithOffset, bool includeWithoutOffset)
    {
        if (! includeWithOffset && ! includeWithoutOffset)
        {
            DarkDialog::show ("Get Clips", "Select at least one option.", this);
            return;
        }

        if (onApply_ != nullptr)
            onApply_ (includeWithOffset, includeWithoutOffset);
        closeButtonPressed();
    }

private:
    ApplyHandler onApply_;

    struct Content final : juce::Component
    {
        GetClipsOptionsWindow& owner_;
        juce::Label title_ { {}, "Select which clips to import" };
        juce::ToggleButton withOffset_;
        juce::ToggleButton withoutOffset_;
        juce::TextButton apply_ { "Apply" };
        juce::TextButton cancel_ { "Cancel" };

        Content (GetClipsOptionsWindow& owner, bool includeWithOffset, bool includeWithoutOffset)
            : owner_ (owner)
        {
            title_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe0, 0xe0, 0xe0));
            title_.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
            title_.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (title_);

            withOffset_.setButtonText ("Get Clips with timecode");
            withoutOffset_.setButtonText ("Get Clips without timecode");
            withOffset_.setToggleState (includeWithOffset, juce::dontSendNotification);
            withoutOffset_.setToggleState (includeWithoutOffset, juce::dontSendNotification);
            addAndMakeVisible (withOffset_);
            addAndMakeVisible (withoutOffset_);

            for (auto* b : { &apply_, &cancel_ })
            {
                b->setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                b->setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                addAndMakeVisible (*b);
            }

            apply_.onClick = [this]
            {
                owner_.applySelection (withOffset_.getToggleState(),
                                       withoutOffset_.getToggleState());
            };
            cancel_.onClick = [this] { owner_.closeButtonPressed(); };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (16);
            title_.setBounds (area.removeFromTop (24));
            area.removeFromTop (10);
            withOffset_.setBounds (area.removeFromTop (26));
            area.removeFromTop (8);
            withoutOffset_.setBounds (area.removeFromTop (26));
            area.removeFromTop (18);
            auto buttons = area.removeFromTop (32);
            cancel_.setBounds (buttons.removeFromRight (100));
            buttons.removeFromRight (10);
            apply_.setBounds (buttons.removeFromRight (100));
        }
    };
};

class InlineEndActionCell final : public juce::Component
{
public:
    std::function<void(const juce::String&, const juce::String&, const juce::String&, const juce::String&)> onChanged;

    InlineEndActionCell()
    {
        addAndMakeVisible (modeBtn_);
        addAndMakeVisible (value1_);
        addAndMakeVisible (value2_);

        modeBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
        modeBtn_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        modeBtn_.onClick = [this]
        {
            if (mode_ == "off") mode_ = "col";
            else if (mode_ == "col") mode_ = "lc";
            else mode_ = "off";
            applyModeToUi();
            emitChanged();
        };

        auto styleEditor = [] (juce::TextEditor& e)
        {
            e.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0x24, 0x24, 0x24));
            e.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
            e.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
            e.setJustification (juce::Justification::centredLeft);
            e.setBorder (juce::BorderSize<int> (0));
            e.setIndents (4, 0); // 0 top-indent lets centredLeft do vertical centering
            e.setFont (juce::FontOptions (13.0f));
            e.onReturnKey = [&e] { e.giveAwayKeyboardFocus(); };
        };
        styleEditor (value1_);
        styleEditor (value2_);

        value1_.onFocusLost = [this] { emitChanged(); };
        value2_.onFocusLost = [this] { emitChanged(); };
    }

    void setState (const juce::String& mode, const juce::String& col, const juce::String& layer, const juce::String& clip)
    {
        mode_ = mode.isNotEmpty() ? mode : "off";
        col_ = col;
        layer_ = layer;
        clip_ = clip;
        value1_.setText (mode_ == "col" ? col_ : layer_, juce::dontSendNotification);
        value2_.setText (clip_, juce::dontSendNotification);
        applyModeToUi();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1, 4);
        modeBtn_.setBounds (r.removeFromLeft (62));
        r.removeFromLeft (4);
        if (mode_ == "col")
        {
            value1_.setBounds (r.removeFromLeft (juce::jmin (66, r.getWidth())));
            value2_.setBounds (0, 0, 0, 0);
        }
        else if (mode_ == "lc")
        {
            const int w = juce::jmin (56, juce::jmax (42, (r.getWidth() - 4) / 2));
            value1_.setBounds (r.removeFromLeft (w));
            r.removeFromLeft (4);
            value2_.setBounds (r.removeFromLeft (w));
        }
        else
        {
            value1_.setBounds (0, 0, 0, 0);
            value2_.setBounds (0, 0, 0, 0);
        }
    }

private:
    void applyModeToUi()
    {
        const auto m = mode_.toLowerCase();
        if (m == "col")
        {
            mode_ = "col";
            modeBtn_.setButtonText ("Col");
            value1_.setVisible (true);
            value2_.setVisible (false);
        }
        else if (m == "lc")
        {
            mode_ = "lc";
            modeBtn_.setButtonText ("L/C");
            value1_.setVisible (true);
            value2_.setVisible (true);
        }
        else
        {
            mode_ = "off";
            modeBtn_.setButtonText ("Off");
            value1_.setVisible (false);
            value2_.setVisible (false);
        }
        resized();
    }

    void emitChanged()
    {
        if (mode_ == "col")
        {
            col_ = value1_.getText().trim();
            layer_.clear();
            clip_.clear();
        }
        else if (mode_ == "lc")
        {
            layer_ = value1_.getText().trim();
            clip_ = value2_.getText().trim();
            col_.clear();
        }
        else
        {
            col_.clear();
            layer_.clear();
            clip_.clear();
        }

        if (onChanged)
            onChanged (mode_, col_, mode_ == "lc" ? layer_ : juce::String(), mode_ == "lc" ? clip_ : juce::String());
    }

    juce::TextButton modeBtn_;
    juce::TextEditor value1_;
    juce::TextEditor value2_;
    juce::String mode_ { "off" };
    juce::String col_;
    juce::String layer_;
    juce::String clip_;
};
}

class TriggerContentComponent::AudioScanThread final : public juce::Thread
{
public:
    explicit AudioScanThread (TriggerContentComponent* owner)
        : juce::Thread ("TriggerAudioScan"),
          safeOwner_ (owner)
    {}

    void run() override
    {
        juce::Array<bridge::engine::AudioChoice> inputs, outputs;

        if (auto* owner = safeOwner_.getComponent())
        {
            inputs = owner->bridgeEngine_.scanAudioInputs();
            if (threadShouldExit())
                return;
            outputs = owner->bridgeEngine_.scanAudioOutputs();
        }

        if (threadShouldExit())
            return;

        auto safe = safeOwner_;
        juce::MessageManager::callAsync ([safe, inputs, outputs]()
        {
            if (auto* owner = safe.getComponent())
                owner->onAudioScanComplete (inputs, outputs);
        });
    }

private:
    juce::Component::SafePointer<TriggerContentComponent> safeOwner_;
};

TriggerContentComponent::TriggerContentComponent()
{
    ltcOutputApplyThread_ = std::thread ([this] { ltcOutputApplyLoop(); });
    setOpaque (true);
    loadFonts();
    applyTheme();
    addAndMakeVisible (leftViewport_);
    leftViewport_.setViewedComponent (&leftViewportContent_, false);
    leftViewport_.setScrollBarsShown (true, false);
    leftViewport_.setScrollBarThickness (8);

    easyLabel_.setText ("EASY ", juce::dontSendNotification);
    easyLabel_.setJustificationType (juce::Justification::centredLeft);
    easyLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xce, 0xce, 0xce));
    easyLabel_.setFont (headerBold_.withHeight (32.0f));
    triggerLabel_.setText ("TRIGGER", juce::dontSendNotification);
    triggerLabel_.setJustificationType (juce::Justification::centredLeft);
    triggerLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0x8a, 0x8a, 0x8a));
    triggerLabel_.setFont (headerLight_.withHeight (32.0f));
    versionLabel_.setJustificationType (juce::Justification::centredLeft);
    versionLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0x8a, 0x8a, 0x8a));
    versionLabel_.setFont (juce::FontOptions (12.0f));
    tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
    tcLabel_.setJustificationType (juce::Justification::centred);
    tcLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    tcLabel_.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    tcLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    tcLabel_.setFont (mono_.withHeight (68.0f));
    fpsIndicatorStrip_ = std::make_unique<FpsIndicatorStrip>();

    sourceCombo_.addItem ("LTC", 1);
    sourceCombo_.addItem ("MTC", 2);
    sourceCombo_.addItem ("ArtNet", 3);
    sourceCombo_.addItem ("OSC", 4);
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (sourceCombo_);
    leftViewportContent_.addAndMakeVisible (sourceHeaderLabel_);
    sourceExpandBtn_.setExpanded (true);
    sourceExpandBtn_.onClick = [this]
    {
        sourceExpanded_ = ! sourceExpanded_;
        sourceExpandBtn_.setExpanded (sourceExpanded_);
        updateWindowHeight();
        resized();
        repaint();
    };
    leftViewportContent_.addAndMakeVisible (sourceExpandBtn_);
    leftViewportContent_.addAndMakeVisible (sourceCombo_);
    sourceHeaderLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
    sourceHeaderLabel_.setFont (juce::FontOptions (14.0f));
    sourceHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    sourceHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 6, 0, 0));
    juce::Label* sourceRowLabels[] = {
        &inDriverLbl_, &inDeviceLbl_, &inChannelLbl_, &inRateLbl_, &inLevelLbl_, &inGainLbl_,
        &mtcInLbl_, &artInLbl_, &artInListenIpLbl_, &oscAdapterLbl_, &oscIpLbl_, &oscPortLbl_, &oscFpsLbl_, &oscStrLbl_, &oscFloatLbl_, &oscFloatTypeLbl_, &oscFloatMaxLbl_,
        &outDriverLbl_, &outDeviceLbl_, &outChannelLbl_, &outRateLbl_, &outConvertLbl_, &outOffsetLbl_, &outLevelLbl_,
        &resAdapterLbl_, &resSendIpLbl_, &resSendIpLbl2_, &resSendIpLbl3_, &resSendIpLbl4_, &resSendIpLbl5_,
        &resSendAdapterLbl1_, &resSendAdapterLbl2_, &resSendAdapterLbl3_, &resSendAdapterLbl4_, &resSendAdapterLbl5_,
        &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_, &resGlobalOffsetLbl_
    };
    for (auto* l : sourceRowLabels)
    {
        l->setColour (juce::Label::backgroundColourId, row_);
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        l->setJustificationType (juce::Justification::centredLeft);
        leftViewportContent_.addAndMakeVisible (*l);
    }

    for (auto* c : { &ltcInDriverCombo_, &ltcOutDriverCombo_ })
    {
        c->addItem ("Default (all devices)", 1);
        c->setSelectedId (1, juce::dontSendNotification);
        styleCombo (*c);
    }
    styleCombo (ltcInDeviceCombo_);
    styleCombo (ltcInChannelCombo_);
    styleCombo (ltcInSampleRateCombo_);
    styleCombo (mtcInCombo_);
    styleCombo (artnetInCombo_);
    styleCombo (oscAdapterCombo_);
    styleCombo (resolumeAdapterCombo_);
    styleCombo (resolumeSendAdapterCombo1_);
    styleCombo (resolumeSendAdapterCombo2_);
    styleCombo (resolumeSendAdapterCombo3_);
    styleCombo (resolumeSendAdapterCombo4_);
    styleCombo (resolumeSendAdapterCombo5_);
    styleCombo (oscFpsCombo_);
    styleCombo (ltcOutDeviceCombo_);
    styleCombo (ltcOutChannelCombo_);
    styleCombo (ltcOutSampleRateCombo_);
    leftViewportContent_.addAndMakeVisible (ltcConvertStrip_);
    leftViewportContent_.addAndMakeVisible (ltcInDriverCombo_);
    leftViewportContent_.addAndMakeVisible (ltcOutDriverCombo_);

    fillChannelCombo (ltcInChannelCombo_, 0);
    fillChannelCombo (ltcOutChannelCombo_, 0);
    fillRateCombo (ltcInSampleRateCombo_);
    fillRateCombo (ltcOutSampleRateCombo_);

    oscPortEditor_.setText ("9000");
    oscIpEditor_.setText ("0.0.0.0");
    oscPortEditor_.setInputRestrictions (5, "0123456789");
    oscAddrStrEditor_.setText ("/frames/str");
    oscAddrFloatEditor_.setText ("/time");
    oscFloatTypeCombo_.addItem ("Seconds",    1);
    oscFloatTypeCombo_.addItem ("Frames",     2);
    oscFloatTypeCombo_.addItem ("Normalized", 3);
    oscFloatTypeCombo_.setSelectedId (1, juce::dontSendNotification);
    oscFloatMaxEditor_.setText ("3600");
    // Allow both dot and comma as decimal separators (e.g. 7.15 or 7,15).
    oscFloatMaxEditor_.setInputRestrictions (10, "0123456789.,");
    styleEditor (oscIpEditor_);
    styleEditor (oscPortEditor_);
    styleEditor (oscAddrStrEditor_);
    styleEditor (oscAddrFloatEditor_);
    styleCombo  (oscFloatTypeCombo_);
    styleEditor (oscFloatMaxEditor_);
    styleEditor (artnetListenIpEditor_);
    artnetListenIpEditor_.setText ("0.0.0.0");

    oscFpsCombo_.addItem ("24", 1);
    oscFpsCombo_.addItem ("25", 2);
    oscFpsCombo_.addItem ("29.97", 3);
    oscFpsCombo_.addItem ("30", 4);
    oscFpsCombo_.setSelectedId (2, juce::dontSendNotification);

    setupDbSlider (ltcInGainSlider_);
    ltcInLevelBar_.setMeterColour (juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    setupDbSlider (ltcOutLevelSlider_);
    ltcOffsetEditor_.setInputRestrictions (4, "-0123456789");
    ltcOffsetEditor_.setText ("0");
    styleEditor (ltcOffsetEditor_);
    styleSlider (ltcInGainSlider_, true);
    styleSlider (ltcOutLevelSlider_, true);

    for (auto* c : { &ltcInDeviceCombo_, &ltcInChannelCombo_, &ltcInSampleRateCombo_, &oscAdapterCombo_, &mtcInCombo_, &artnetInCombo_, &oscFpsCombo_,
                     &resolumeAdapterCombo_, &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_ })
    {
        leftViewportContent_.addAndMakeVisible (*c);
        c->onChange = [this] { onInputSettingsChanged(); };
    }
    for (auto* c : { &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_ })
    {
        leftViewportContent_.addAndMakeVisible (*c);
        c->onChange = [this] { onOutputSettingsChanged(); };
    }
    ltcConvertStrip_.onChange = [this] (std::optional<FrameRate> fps)
    {
        bridgeEngine_.setLtcOutputConvertFps (fps);
        onOutputSettingsChanged();
    };
    artnetListenIpEditor_.onTextChange = [this] { onInputSettingsChanged(); };
    oscFloatTypeCombo_.onChange = [this]
    {
        updateWindowHeight();
        resized();
        leftViewportContent_.repaint();
        leftViewport_.repaint();
        repaint();
        onInputSettingsChanged();
    };
    oscFloatMaxEditor_.onTextChange = [this] { onInputSettingsChanged(); };

    ltcInDriverCombo_.onChange = [this]
    {
        refreshLtcDeviceListsByDriver();
        refreshLtcChannelCombos();
        onInputSettingsChanged();
    };
    ltcOutDriverCombo_.onChange = [this]
    {
        refreshLtcDeviceListsByDriver();
        refreshLtcChannelCombos();
        onOutputSettingsChanged();
    };
    ltcInDeviceCombo_.onChange = [this]
    {
        refreshLtcChannelCombos();
        onInputSettingsChanged();
    };
    ltcOutDeviceCombo_.onChange = [this]
    {
        refreshLtcChannelCombos();
        onOutputSettingsChanged();
    };
    oscAdapterCombo_.onChange = [this]
    {
        syncOscIpWithAdapter();
        onInputSettingsChanged();
    };

    sourceCombo_.onChange = [this]
    {
        restartSelectedSource();
        updateWindowHeight();
        resized();
        repaint();
    };
    ltcOutSwitch_.onToggle = [this] (bool) { onOutputToggleChanged(); };
    ltcThruDot_.onToggle = [this] (bool) { queueLtcOutputApply(); };

    leftViewportContent_.addAndMakeVisible (ltcInGainSlider_);
    leftViewportContent_.addAndMakeVisible (ltcInLevelBar_);
    leftViewportContent_.addAndMakeVisible (ltcOutLevelSlider_);
    leftViewportContent_.addAndMakeVisible (ltcOffsetEditor_);
    leftViewportContent_.addAndMakeVisible (oscIpEditor_);
    leftViewportContent_.addAndMakeVisible (oscPortEditor_);
    leftViewportContent_.addAndMakeVisible (oscAddrStrEditor_);
    leftViewportContent_.addAndMakeVisible (oscAddrFloatEditor_);
    leftViewportContent_.addAndMakeVisible (oscFloatTypeCombo_);
    leftViewportContent_.addAndMakeVisible (oscFloatMaxEditor_);
    leftViewportContent_.addAndMakeVisible (artnetListenIpEditor_);

    resolumeSendIp_.setText ("127.0.0.1");
    resolumeSendPort_.setText ("7000");
    resAdapterLbl_.setText ("Adapter:", juce::dontSendNotification);
    resSendIpLbl_.setText ("Send 1:", juce::dontSendNotification);
    resSendIpLbl2_.setText ("Send 2:", juce::dontSendNotification);
    resSendIpLbl3_.setText ("Send 3:", juce::dontSendNotification);
    resSendIpLbl4_.setText ("Send 4:", juce::dontSendNotification);
    resSendIpLbl5_.setText ("Send 5:", juce::dontSendNotification);
    for (auto* b : { &resolumeSendExpandBtn1_, &resolumeSendExpandBtn2_, &resolumeSendExpandBtn3_, &resolumeSendExpandBtn4_, &resolumeSendExpandBtn5_ })
        b->setColours (input_, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a), juce::Colour::fromRGB (0xf2, 0xf2, 0xf2));
    resolumeSendIp2_.setText ("127.0.0.1");
    resolumeSendPort2_.setText ("7000");
    resolumeSendIp3_.setText ("127.0.0.1");
    resolumeSendPort3_.setText ("7000");
    resolumeSendIp4_.setText ("127.0.0.1");
    resolumeSendPort4_.setText ("7000");
    resolumeSendIp5_.setText ("127.0.0.1");
    resolumeSendPort5_.setText ("7000");
    resListenIpLbl_.setText ("Listen:", juce::dontSendNotification);
    for (auto* e : { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_ })
        e->setInputRestrictions (5, "0123456789");
    resolumeListenIp_.setText ("127.0.0.1");
    resolumeListenPort_.setText ("7001");
    resolumeMaxLayers_.setText ("4");
    resolumeMaxClips_.setText ("32");
    resolumeGlobalOffset_.setText ("00:00:00:00");
    resolumeAddTargetBtn_.onClick = [this] { addResolumeSendTarget(); };
    resolumeDelTargetBtn2_.onClick = [this] { removeResolumeSendTarget (1); };
    resolumeDelTargetBtn3_.onClick = [this] { removeResolumeSendTarget (2); };
    resolumeDelTargetBtn4_.onClick = [this] { removeResolumeSendTarget (3); };
    resolumeDelTargetBtn5_.onClick = [this] { removeResolumeSendTarget (4); };
    resolumeAdapterCombo_.onChange = [this] { syncResolumeListenIpWithAdapter(); };
    resolumeSendExpandBtn1_.onClick = [this] { toggleSendAdapterExpanded (0); };
    resolumeSendExpandBtn2_.onClick = [this] { toggleSendAdapterExpanded (1); };
    resolumeSendExpandBtn3_.onClick = [this] { toggleSendAdapterExpanded (2); };
    resolumeSendExpandBtn4_.onClick = [this] { toggleSendAdapterExpanded (3); };
    resolumeSendExpandBtn5_.onClick = [this] { toggleSendAdapterExpanded (4); };
    resolumeSendExpandBtn1_.setExpanded (resolumeSendExpanded1_);
    resolumeSendExpandBtn2_.setExpanded (resolumeSendExpanded2_);
    resolumeSendExpandBtn3_.setExpanded (resolumeSendExpanded3_);
    resolumeSendExpandBtn4_.setExpanded (resolumeSendExpanded4_);
    resolumeSendExpandBtn5_.setExpanded (resolumeSendExpanded5_);
    getTriggersBtn_.onClick = [this] { openGetClipsOptions(); };
    createCustomBtn_.onClick = [this]
    {
        if (! hasCustomGroup())
            addCustomColTrigger();
        createCustomBtn_.setEnabled (! hasCustomGroup());
    };
    resolumeExpandBtn_.setExpanded (false);
    helpButton_.onClick = [this] { openHelpPage(); };
    resolumeExpandBtn_.onClick = [this] { resolumeExpanded_ = ! resolumeExpanded_; resolumeExpandBtn_.setExpanded (resolumeExpanded_); updateWindowHeight(); resized(); repaint(); };
    triggerOutHeaderLabel_.setColour (juce::Label::backgroundColourId, row_);
    triggerOutHeaderLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
    triggerOutHeaderLabel_.setFont (juce::FontOptions (14.0f));
    triggerOutHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    triggerOutHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 6, 0, 0));
    triggerOutExpandBtn_.setExpanded (false);
    triggerOutExpandBtn_.onClick = [this]
    {
        triggerOutExpanded_ = ! triggerOutExpanded_;
        triggerOutExpandBtn_.setExpanded (triggerOutExpanded_);
        updateWindowHeight();
        resized();
        repaint();
    };
    outLtcHeaderLabel_.setColour (juce::Label::backgroundColourId, row_);
    outLtcHeaderLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
    outLtcHeaderLabel_.setFont (juce::FontOptions (14.0f));
    outLtcHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    outLtcHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 42, 0, 0));
    leftViewportContent_.addAndMakeVisible (outLtcHeaderLabel_);
    leftViewportContent_.addAndMakeVisible (outLtcExpandBtn_);
    outLtcExpandBtn_.setExpanded (false);
    outLtcExpandBtn_.onClick = [this]
    {
        outLtcExpanded_ = ! outLtcExpanded_;
        outLtcExpandBtn_.setExpanded (outLtcExpanded_);
        updateWindowHeight();
        resized();
        repaint();
    };
    leftViewportContent_.addAndMakeVisible (ltcOutSwitch_);
    leftViewportContent_.addAndMakeVisible (ltcThruDot_);
    ltcThruLbl_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
    ltcThruLbl_.setJustificationType (juce::Justification::centredLeft);
    leftViewportContent_.addAndMakeVisible (ltcThruLbl_);
    settingsButton_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    settingsButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    settingsButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    settingsButton_.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    settingsButton_.onClick = [this] { openSettingsMenu(); };
    quitButton_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0xb6, 0x45, 0x40));
    quitButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0xb6, 0x45, 0x40));
    quitButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    quitButton_.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    quitButton_.onClick = [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
    {
        if (safe == nullptr)
            return;

        if (auto* window = safe->findParentComponentOfClass<MainWindow>())
            window->quitFromTray();
        else
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
    };
    triggerTable_.setModel (this);
    triggerTable_.setRowHeight (36);
    triggerTable_.setOutlineThickness (0);
    triggerTable_.setColour (juce::ListBox::outlineColourId, juce::Colour::fromRGB (0x3f, 0x3f, 0x3f));
    triggerTable_.setColour (juce::ListBox::backgroundColourId, bg_);
    triggerTable_.getHorizontalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
    triggerTable_.getHorizontalScrollBar().setColour (juce::ScrollBar::trackColourId, row_);
    triggerTable_.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
    triggerTable_.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, row_);
    auto& h = triggerTable_.getHeader();
    h.addColumn ("",           1,  40,  30);
    h.addColumn ("In",         2,  46,  34);
    h.addColumn ("Name",       3, 260,  90);
    h.addColumn ("Count",      4, 110,  92);
    h.addColumn ("Range",      5,  70,  58);
    h.addColumn ("Trigger",    6, 110,  92);
    h.addColumn ("Duration",   7, 110,  92);
    h.addColumn ("End Action", 8, 180,  72);  // dynamic min enforced in tableColumnsResized
    h.addColumn ("Test",       9,  56,  56, 56, juce::TableHeaderComponent::notResizable);
    h.addListener (this);
    h.setStretchToFitActive (false);
    h.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    h.setColour (juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));

    clipCollector_.onChanged = [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
    {
        if (safe == nullptr)
            return;
        safe->queryPending_ = false;
        juce::MessageManager::callAsync ([safe]
        {
            // Only rebuild rows while a query is active (clipReceiveEnabled_).
            // This prevents Resolume OSC feedback from custom triggers
            // recreating clip rows after the user has cleared them.
            if (safe != nullptr && safe->clipReceiveEnabled_)
                safe->refreshTriggerRows();
        });
    };

    addAndMakeVisible (easyLabel_);
    addAndMakeVisible (triggerLabel_);
    addAndMakeVisible (versionLabel_);
    addAndMakeVisible (helpButton_);
    addAndMakeVisible (tcLabel_);
    addAndMakeVisible (*fpsIndicatorStrip_);
    addAndMakeVisible (resolumeStatusLabel_);
    addAndMakeVisible (statusLabel_);
    leftViewportContent_.addAndMakeVisible (resolumeHeader_);
    leftViewportContent_.addAndMakeVisible (resolumeExpandBtn_);
    leftViewportContent_.addAndMakeVisible (triggerOutHeaderLabel_);
    leftViewportContent_.addAndMakeVisible (triggerOutExpandBtn_);
    leftViewportContent_.addAndMakeVisible (resAdapterLbl_);
    leftViewportContent_.addAndMakeVisible (resSendIpLbl_);
    leftViewportContent_.addAndMakeVisible (resSendIpLbl2_);
    leftViewportContent_.addAndMakeVisible (resSendIpLbl3_);
    leftViewportContent_.addAndMakeVisible (resSendIpLbl4_);
    leftViewportContent_.addAndMakeVisible (resSendIpLbl5_);
    leftViewportContent_.addAndMakeVisible (resSendAdapterLbl1_);
    leftViewportContent_.addAndMakeVisible (resSendAdapterLbl2_);
    leftViewportContent_.addAndMakeVisible (resSendAdapterLbl3_);
    leftViewportContent_.addAndMakeVisible (resSendAdapterLbl4_);
    leftViewportContent_.addAndMakeVisible (resSendAdapterLbl5_);
    leftViewportContent_.addAndMakeVisible (resSendPortLbl_);
    leftViewportContent_.addAndMakeVisible (resListenIpLbl_);
    leftViewportContent_.addAndMakeVisible (resListenPortLbl_);
    leftViewportContent_.addAndMakeVisible (resMaxLayersLbl_);
    leftViewportContent_.addAndMakeVisible (resMaxClipsLbl_);
    leftViewportContent_.addAndMakeVisible (resGlobalOffsetLbl_);
    leftViewportContent_.addAndMakeVisible (resolumeSendIp_);
    leftViewportContent_.addAndMakeVisible (resolumeSendPort_);
    leftViewportContent_.addAndMakeVisible (resolumeSendIp2_);
    leftViewportContent_.addAndMakeVisible (resolumeSendPort2_);
    leftViewportContent_.addAndMakeVisible (resolumeSendIp3_);
    leftViewportContent_.addAndMakeVisible (resolumeSendPort3_);
    leftViewportContent_.addAndMakeVisible (resolumeSendIp4_);
    leftViewportContent_.addAndMakeVisible (resolumeSendPort4_);
    leftViewportContent_.addAndMakeVisible (resolumeSendIp5_);
    leftViewportContent_.addAndMakeVisible (resolumeSendPort5_);
    leftViewportContent_.addAndMakeVisible (resolumeSendExpandBtn1_);
    leftViewportContent_.addAndMakeVisible (resolumeSendExpandBtn2_);
    leftViewportContent_.addAndMakeVisible (resolumeSendExpandBtn3_);
    leftViewportContent_.addAndMakeVisible (resolumeSendExpandBtn4_);
    leftViewportContent_.addAndMakeVisible (resolumeSendExpandBtn5_);
    leftViewportContent_.addAndMakeVisible (resolumeAddTargetBtn_);
    leftViewportContent_.addAndMakeVisible (resolumeDelTargetBtn2_);
    leftViewportContent_.addAndMakeVisible (resolumeDelTargetBtn3_);
    leftViewportContent_.addAndMakeVisible (resolumeDelTargetBtn4_);
    leftViewportContent_.addAndMakeVisible (resolumeDelTargetBtn5_);
    leftViewportContent_.addAndMakeVisible (resolumeListenIp_);
    leftViewportContent_.addAndMakeVisible (resolumeListenPort_);
    leftViewportContent_.addAndMakeVisible (resolumeMaxLayers_);
    leftViewportContent_.addAndMakeVisible (resolumeMaxClips_);
    leftViewportContent_.addAndMakeVisible (resolumeGlobalOffset_);
    addAndMakeVisible (getTriggersBtn_);
    addAndMakeVisible (createCustomBtn_);
    addAndMakeVisible (settingsButton_);
    addAndMakeVisible (quitButton_);
    addAndMakeVisible (triggerTable_);
    if (auto* vp = triggerTable_.getViewport())
        vp->setScrollBarsShown (true, false);

    setSize (1240, 820);
    resized();
    setResolumeStatusText ("Resolume idle", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    setTimecodeStatusText ("SAFE START", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    loadRuntimePrefs();
    startAudioDeviceScan();
    startTimerHz (60);
}

TriggerContentComponent::~TriggerContentComponent()
{
    {
        const std::lock_guard<std::mutex> lock (ltcOutputApplyMutex_);
        ltcOutputApplyExit_ = true;
        ltcOutputApplyPending_ = false;
    }
    ltcOutputApplyCv_.notify_all();
    if (ltcOutputApplyThread_.joinable())
        ltcOutputApplyThread_.join();

    if (scanThread_ != nullptr && scanThread_->isThreadRunning())
        scanThread_->stopThread (2000);
    triggerTable_.getHeader().removeListener (this);
    clipCollector_.stopListening();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

int TriggerContentComponent::calcPreferredHeight() const
{
    return calcHeightForState (sourceExpanded_, sourceCombo_.getSelectedId(), triggerOutExpanded_, outLtcExpanded_, resolumeExpanded_);
}

int TriggerContentComponent::calcHeightForState (bool sourceExpanded, int sourceId, bool triggerOutExpanded, bool outLtcExpanded, bool resolumeExpanded) const
{
    auto rowsForSource = [sourceId, this]() -> int
    {
        if (sourceId == 1) return 6;
        if (sourceId == 2) return 1;
        if (sourceId == 3) return 2;
        const bool showMax = oscFloatTypeCombo_.getSelectedId() == 3;
        return showMax ? 8 : 7;
    };

    const int triggerOutRows = juce::jlimit (1, 5, resolumeSendTargetCount_)
                             + (resolumeSendExpanded1_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_ ? 1 : 0);
    const int resolumeRows = 5;

    int h = 16;
    h += 40 + 4;
    h += 90;
    h += 4 + kCompactBarHeight + 4;
    h += 40 + 4;
    if (sourceExpanded)
        h += rowsForSource() * (40 + 4);
    h += 40 + 4;
    if (resolumeExpanded)
        h += resolumeRows * (40 + 4);
    h += 40 + 4;
    if (triggerOutExpanded)
        h += triggerOutRows * (40 + 4);
    h += 40 + 4;
    if (outLtcExpanded)
        h += 7 * (40 + 4);
    h += 132;
    h += 24;
    h += kStartupHeightExtra;
    return juce::jlimit (420, 1400, h);
}

void TriggerContentComponent::updateWindowHeight()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        if (window->isMinimised())
            return;
#if JUCE_WINDOWS
        if (isNativeWindowMaximized (*window))
            return;
#endif

        auto* content = window->getContentComponent();
        const int chrome = content != nullptr ? (window->getHeight() - content->getHeight()) : 0;
        const int minContent = calcPreferredHeight();
        const int minTotal = minContent + chrome;

        constexpr int kBaseMinW  = 1160;
        constexpr int kCustomExtraW = 184 + 42;
        const int minW = kBaseMinW + (hasCustomGroup() ? kCustomExtraW : 0);
        window->setResizeLimits (minW, minTotal, 1800, 1400);

        if (window->getHeight() < minTotal)
            window->setSize (window->getWidth(), minTotal);
    }
}

void TriggerContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg_);
    if (! headerRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
        g.fillRoundedRectangle (headerRect_.toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x3c, 0x3e, 0x42));
        g.drawRoundedRectangle (headerRect_.toFloat(), 5.0f, 1.0f);
    }
    if (! timerRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));
        g.fillRoundedRectangle (timerRect_.toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x33, 0x33, 0x33));
        g.drawRoundedRectangle (timerRect_.toFloat(), 5.0f, 1.0f);
    }
    {
        juce::Graphics::ScopedSaveState savedState (g);
        auto clip = leftViewportRect_;
        g.reduceClipRegion (clip);
        const auto viewPos = leftViewport_.getViewPosition();
        g.setColour (juce::Colour::fromRGB (0x65, 0x65, 0x65));
        for (auto r : sectionRowRects_)
            g.fillRoundedRectangle (r.translated (clip.getX() - viewPos.x, clip.getY() - viewPos.y).toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
        for (auto r : leftRowRects_)
            g.fillRoundedRectangle (r.translated (clip.getX() - viewPos.x, clip.getY() - viewPos.y).toFloat(), 5.0f);
    }
    g.setColour (bg_);
    for (auto r : rightSectionRects_)
        g.fillRoundedRectangle (r.toFloat(), 6.0f);

    if (! statusBarRect_.isEmpty())
    {
        g.setColour (row_);
        g.fillRect (statusBarRect_);
        g.setColour (juce::Colour::fromRGB (0x2f, 0x2f, 0x2f));
        g.drawLine ((float) statusBarRect_.getX(), (float) statusBarRect_.getY(),
                    (float) statusBarRect_.getRight(), (float) statusBarRect_.getY(), 1.0f);
    }
}

void TriggerContentComponent::resized()
{
    leftRowRects_.clear();
    sectionRowRects_.clear();
    rightSectionRects_.clear();
    headerRect_ = {};
    timerRect_ = {};
    auto bounds = getLocalBounds();
    statusBarRect_ = bounds.removeFromBottom (24);
    statusLeftRect_ = statusBarRect_;
    statusRightRect_ = {};
    if (! statusBarRect_.isEmpty())
    {
        statusRightRect_ = statusLeftRect_.removeFromRight (juce::jmax (260, statusBarRect_.getWidth() / 2));
    }
    auto content = bounds.reduced (8);
    const int totalW = content.getWidth();
    int leftW = juce::jlimit (330, 390, (int) std::round ((double) totalW * 0.40));
    if (totalW - leftW < 420)
        leftW = juce::jmax (300, totalW - 420);
    auto left = content.removeFromLeft (leftW);
    auto right = content.reduced (6, 0);

    headerRect_ = left.removeFromTop (40);
    auto top = headerRect_.reduced (6, 0);
    auto help = top.removeFromRight (28);
    helpButton_.setBounds (help.withSizeKeepingCentre (28, 28));
    const int easyW = juce::jmax (46, easyLabel_.getFont().getStringWidth ("EASY ") + 6);
    const int trigW = juce::jmax (90, triggerLabel_.getFont().getStringWidth ("TRIGGER") + 6);
    const int versionW = juce::jmax (58, versionLabel_.getFont().getStringWidth (versionLabel_.getText()) + 4);
    const int startX = top.getX() + 2;
    const int yOff = 6;
    const int titleH = juce::jmax (1, top.getHeight() - yOff);
    easyLabel_.setBounds (startX, top.getY() + yOff, easyW, titleH);
    triggerLabel_.setBounds (startX + easyW, top.getY() + yOff, trigW, titleH);
    versionLabel_.setBounds (startX + easyW + trigW + 2, top.getY() + yOff, versionW, titleH);
    left.removeFromTop (4);
    timerRect_ = left.removeFromTop (90);
    tcLabel_.setBounds (timerRect_);
    left.removeFromTop (4);
    auto fpsRect = left.removeFromTop (kCompactBarHeight);
    if (fpsIndicatorStrip_ != nullptr)
        fpsIndicatorStrip_->setBounds (fpsRect);
    left.removeFromTop (4);

    auto footerArea = left.removeFromBottom (132);
    auto actionRow = footerArea.removeFromBottom (40);
    footerArea.removeFromBottom (4);
    auto createCustomRow = footerArea.removeFromBottom (40);
    footerArea.removeFromBottom (4);
    auto getTriggersRow = footerArea.removeFromBottom (40);
    footerArea.removeFromBottom (4); // 4px top gap above Get Clips
    leftViewportRect_ = left;
    leftViewport_.setBounds (leftViewportRect_);
    const auto src = sourceCombo_.getSelectedId();
    auto rowsForSource = [this, src]() -> int
    {
        if (src == 1) return 6;
        if (src == 2) return 1;
        if (src == 3) return 2;
        return oscFloatTypeCombo_.getSelectedId() == 3 ? 8 : 7;
    };
    const int triggerOutRows = juce::jlimit (1, 5, resolumeSendTargetCount_)
                             + (resolumeSendExpanded1_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_ ? 1 : 0);
    const int resolumeRows = 5;
    const int contentRows = 1 + (sourceExpanded_ ? rowsForSource() : 0)
                          + 1 + (resolumeExpanded_ ? resolumeRows : 0)
                          + 1 + (triggerOutExpanded_ ? triggerOutRows : 0)
                          + 1 + (outLtcExpanded_ ? 7 : 0);
    const int viewportScrollWidth = leftViewport_.getScrollBarThickness();
    const bool vScrollNeeded = (contentRows * 44 > leftViewportRect_.getHeight());
    auto leftLayoutArea = juce::Rectangle<int> (0, 0, leftViewportRect_.getWidth() - (vScrollNeeded ? viewportScrollWidth : 0) - 4, 0);
    int contentY = 0;
    auto setCompBounds = [&] (juce::Component& c, juce::Rectangle<int> r, bool wanted = true)
    {
        c.setBounds (r);
        c.setVisible (wanted);
    };
    auto pushRowRect = [&] (juce::Rectangle<int> r, bool section)
    {
        if (section) sectionRowRects_.add (r);
        else leftRowRects_.add (r);
    };
    auto nextRow = [&] (int h = 40)
    {
        auto r = juce::Rectangle<int> (leftLayoutArea.getX(), contentY, leftLayoutArea.getWidth(), h);
        contentY += h + 4;
        return r;
    };
    auto layoutParam = [&] (juce::Label& lbl, juce::Component& c, bool wanted = true, int h = 40)
    {
        if (! wanted)
        {
            lbl.setVisible (false);
            c.setVisible (false);
            return;
        }
        auto r = nextRow (h);
        pushRowRect (r, false);
        auto l = r.removeFromLeft (112);
        setCompBounds (lbl, l.reduced (10, 0), wanted);
        auto control = r.reduced (0, 3).reduced (2, 0);
        setCompBounds (c, control, wanted);
    };
    auto layoutTriggerTargetRow = [&] (juce::Label& lbl,
                                       ExpandCircleButton& expandBtn,
                                       juce::TextEditor& ipEd,
                                       juce::TextEditor& portEd,
                                       juce::Label& adapterLbl,
                                       juce::ComboBox& adapterCombo,
                                       juce::Component& actionBtn,
                                       bool adapterExpanded,
                                       bool wanted = true)
    {
        if (! wanted)
        {
            lbl.setVisible (false);
            expandBtn.setVisible (false);
            ipEd.setVisible (false);
            portEd.setVisible (false);
            adapterCombo.setVisible (false);
            actionBtn.setVisible (false);
            return;
        }

        auto r = nextRow();
        pushRowRect (r, false);
        auto left = r.removeFromLeft (112);
        auto expandArea = left.removeFromLeft (36);
        const int expandSize = 28;
        setCompBounds (expandBtn, { expandArea.getX() + 3 + (expandArea.getWidth() - expandSize) / 2,
                                    expandArea.getY() + (expandArea.getHeight() - expandSize) / 2,
                                    expandSize, expandSize }, true);
        setCompBounds (lbl, left.reduced (6, 0), true);

        auto control = r.reduced (0, 3).reduced (2, 0);
        const int d = control.getHeight();
        auto action = control.removeFromRight (d);
        setCompBounds (actionBtn, { action.getX(), action.getY(), d, d }, true);
        control.removeFromRight (4);
        auto port = control.removeFromRight (78);
        control.removeFromRight (4);
        setCompBounds (ipEd, control, true);
        setCompBounds (portEd, port, true);

        if (adapterExpanded)
        {
            auto adapterRow = nextRow();
            pushRowRect (adapterRow, false);
            auto adapterLeft = adapterRow.removeFromLeft (112);
            adapterLeft.removeFromLeft (36);
            setCompBounds (adapterLbl, adapterLeft.reduced (10, 0), true);
            setCompBounds (adapterCombo, adapterRow.reduced (0, 3).reduced (2, 0), true);
        }
        else
        {
            adapterLbl.setVisible (false);
            adapterCombo.setVisible (false);
        }
    };
    auto headerRow = [&] (juce::Label& lbl, ExpandCircleButton& btn, bool wanted = true)
    {
        auto r = nextRow();
        pushRowRect (r, true);
        auto bh = r.removeFromLeft (36);
        const int d = 28;
        setCompBounds (btn, { bh.getX() + 3 + (bh.getWidth() - d) / 2, bh.getY() + (bh.getHeight() - d) / 2, d, d }, wanted);
        setCompBounds (lbl, r.reduced (6, 0), wanted);
    };

    auto sourceRow = nextRow();
    pushRowRect (sourceRow, true);
    auto sourceLabelZone = sourceRow.removeFromLeft (112);
    {
        auto btnHost = sourceLabelZone.removeFromLeft (36);
        const int d = 28;
        setCompBounds (sourceExpandBtn_, { btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
                                           btnHost.getY() + (btnHost.getHeight() - d) / 2, d, d });
    }
    setCompBounds (sourceHeaderLabel_, sourceLabelZone);
    setCompBounds (sourceCombo_, sourceRow.reduced (2, 3));
    sourceHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    sourceExpandBtn_.setExpanded (sourceExpanded_);

    if (sourceExpanded_ && src == 1)
    {
        layoutParam (inDriverLbl_, ltcInDriverCombo_);
        layoutParam (inDeviceLbl_, ltcInDeviceCombo_);
        layoutParam (inChannelLbl_, ltcInChannelCombo_);
        layoutParam (inRateLbl_, ltcInSampleRateCombo_);
        auto meterRow = nextRow();
        pushRowRect (meterRow, false);
        auto meterLabelArea = meterRow.removeFromLeft (112);
        setCompBounds (inLevelLbl_, meterLabelArea.reduced (10, 0));
        auto meterControl = meterRow.reduced (0, 3).reduced (2, 0).reduced (6, 0);
        const int meterH = 8;
        setCompBounds (ltcInLevelBar_, juce::Rectangle<int> (meterControl.getX(), meterControl.getCentreY() - meterH / 2, meterControl.getWidth(), meterH));
        layoutParam (inGainLbl_, ltcInGainSlider_);
    }
    else if (sourceExpanded_ && src == 2)
    {
        layoutParam (mtcInLbl_, mtcInCombo_);
    }
    else if (sourceExpanded_ && src == 3)
    {
        layoutParam (artInLbl_, artnetInCombo_);
        layoutParam (artInListenIpLbl_, artnetListenIpEditor_);
    }
    else if (sourceExpanded_)
    {
        layoutParam (oscAdapterLbl_, oscAdapterCombo_);
        layoutParam (oscIpLbl_, oscIpEditor_);
        layoutParam (oscPortLbl_, oscPortEditor_);
        layoutParam (oscFpsLbl_, oscFpsCombo_);
        layoutParam (oscStrLbl_, oscAddrStrEditor_);
        layoutParam (oscFloatLbl_, oscAddrFloatEditor_);
        layoutParam (oscFloatTypeLbl_, oscFloatTypeCombo_);
        const bool showMax = oscFloatTypeCombo_.getSelectedId() == 3;
        layoutParam (oscFloatMaxLbl_, oscFloatMaxEditor_, showMax);
    }

    headerRow (resolumeHeader_, resolumeExpandBtn_);
    resolumeHeader_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    resolumeExpandBtn_.setExpanded (resolumeExpanded_);
    if (resolumeExpanded_)
    {
        layoutParam (resAdapterLbl_, resolumeAdapterCombo_);
        auto listenRow = nextRow();
        pushRowRect (listenRow, false);
        auto listenLbl = listenRow.removeFromLeft (112);
        setCompBounds (resListenIpLbl_, listenLbl.reduced (10, 0), true);
        auto listenControl = listenRow.reduced (0, 3).reduced (2, 0);
        auto listenPort = listenControl.removeFromRight (92);
        listenControl.removeFromRight (4);
        setCompBounds (resolumeListenIp_, listenControl, true);
        setCompBounds (resolumeListenPort_, listenPort, true);
        layoutParam (resMaxLayersLbl_, resolumeMaxLayers_);
        layoutParam (resMaxClipsLbl_, resolumeMaxClips_);
        layoutParam (resGlobalOffsetLbl_, resolumeGlobalOffset_);
    }

    headerRow (triggerOutHeaderLabel_, triggerOutExpandBtn_);
    triggerOutHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    triggerOutExpandBtn_.setExpanded (triggerOutExpanded_);
    if (triggerOutExpanded_)
    {
        layoutTriggerTargetRow (resSendIpLbl_,  resolumeSendExpandBtn1_, resolumeSendIp_,  resolumeSendPort_,  resSendAdapterLbl1_, resolumeSendAdapterCombo1_, resolumeAddTargetBtn_,  resolumeSendExpanded1_, true);
        layoutTriggerTargetRow (resSendIpLbl2_, resolumeSendExpandBtn2_, resolumeSendIp2_, resolumeSendPort2_, resSendAdapterLbl2_, resolumeSendAdapterCombo2_, resolumeDelTargetBtn2_, resolumeSendExpanded2_, resolumeSendTargetCount_ >= 2);
        layoutTriggerTargetRow (resSendIpLbl3_, resolumeSendExpandBtn3_, resolumeSendIp3_, resolumeSendPort3_, resSendAdapterLbl3_, resolumeSendAdapterCombo3_, resolumeDelTargetBtn3_, resolumeSendExpanded3_, resolumeSendTargetCount_ >= 3);
        layoutTriggerTargetRow (resSendIpLbl4_, resolumeSendExpandBtn4_, resolumeSendIp4_, resolumeSendPort4_, resSendAdapterLbl4_, resolumeSendAdapterCombo4_, resolumeDelTargetBtn4_, resolumeSendExpanded4_, resolumeSendTargetCount_ >= 4);
        layoutTriggerTargetRow (resSendIpLbl5_, resolumeSendExpandBtn5_, resolumeSendIp5_, resolumeSendPort5_, resSendAdapterLbl5_, resolumeSendAdapterCombo5_, resolumeDelTargetBtn5_, resolumeSendExpanded5_, resolumeSendTargetCount_ >= 5);
    }

    auto ltcHeader = nextRow();
    pushRowRect (ltcHeader, true);
    auto ltcHeaderCopy = ltcHeader;
    setCompBounds (outLtcHeaderLabel_, ltcHeaderCopy);
    {
        auto btnHost = ltcHeader.removeFromLeft (36);
        const int d = 28;
        setCompBounds (outLtcExpandBtn_, { btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
                                           btnHost.getY() + (btnHost.getHeight() - d) / 2, d, d });
    }
    ltcHeader.removeFromLeft (110);
    setCompBounds (ltcOutSwitch_, ltcHeader.removeFromRight (54).reduced (0, 6));
    {
        auto dotHost = ltcHeader.removeFromRight (22);
        const int d = 18;
        setCompBounds (ltcThruDot_, { dotHost.getCentreX() - d / 2, dotHost.getCentreY() - d / 2, d, d });
    }
    setCompBounds (ltcThruLbl_, ltcHeader.removeFromRight (40));
    outLtcHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    outLtcExpandBtn_.setExpanded (outLtcExpanded_);

    if (outLtcExpanded_)
    {
        layoutParam (outDriverLbl_, ltcOutDriverCombo_);
        layoutParam (outDeviceLbl_, ltcOutDeviceCombo_);
        layoutParam (outChannelLbl_, ltcOutChannelCombo_);
        layoutParam (outRateLbl_, ltcOutSampleRateCombo_);
        layoutParam (outConvertLbl_, ltcConvertStrip_);
        layoutParam (outOffsetLbl_, ltcOffsetEditor_);
        layoutParam (outLevelLbl_, ltcOutLevelSlider_);
    }
    leftViewportContent_.setSize (juce::jmax (0, leftLayoutArea.getWidth()), juce::jmax (contentY, leftViewportRect_.getHeight()));

    auto hideAll = [this]
    {
        juce::Component* comps[] = {
            &ltcInDriverCombo_, &ltcInDeviceCombo_, &ltcInChannelCombo_, &ltcInSampleRateCombo_, &ltcInLevelBar_, &ltcInGainSlider_,
            &mtcInCombo_, &artnetInCombo_, &artnetListenIpEditor_, &oscAdapterCombo_, &oscIpEditor_, &oscPortEditor_, &oscFpsCombo_, &oscAddrStrEditor_, &oscAddrFloatEditor_, &oscFloatTypeCombo_, &oscFloatMaxEditor_,
            &ltcOutDriverCombo_, &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_, &ltcConvertStrip_, &ltcOffsetEditor_, &ltcOutLevelSlider_,
            &resolumeAdapterCombo_, &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_
        };
        for (auto* c : comps)
            if (c != nullptr)
                c->setVisible (false);
    };
    hideAll();
    inDriverLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInDriverCombo_.setVisible (sourceExpanded_ && src == 1);
    inDeviceLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInDeviceCombo_.setVisible (sourceExpanded_ && src == 1);
    inChannelLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInChannelCombo_.setVisible (sourceExpanded_ && src == 1);
    inRateLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInSampleRateCombo_.setVisible (sourceExpanded_ && src == 1);
    inLevelLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInLevelBar_.setVisible (sourceExpanded_ && src == 1);
    inGainLbl_.setVisible (sourceExpanded_ && src == 1);
    ltcInGainSlider_.setVisible (sourceExpanded_ && src == 1);
    mtcInLbl_.setVisible (sourceExpanded_ && src == 2);
    mtcInCombo_.setVisible (sourceExpanded_ && src == 2);
    artInLbl_.setVisible (sourceExpanded_ && src == 3);
    artnetInCombo_.setVisible (sourceExpanded_ && src == 3);
    artInListenIpLbl_.setVisible (sourceExpanded_ && src == 3);
    artnetListenIpEditor_.setVisible (sourceExpanded_ && src == 3);
    oscAdapterLbl_.setVisible (sourceExpanded_ && src == 4);
    oscAdapterCombo_.setVisible (sourceExpanded_ && src == 4);
    oscIpLbl_.setVisible (sourceExpanded_ && src == 4);
    oscIpEditor_.setVisible (sourceExpanded_ && src == 4);
    oscPortLbl_.setVisible (sourceExpanded_ && src == 4);
    oscPortEditor_.setVisible (sourceExpanded_ && src == 4);
    oscFpsLbl_.setVisible (sourceExpanded_ && src == 4);
    oscFpsCombo_.setVisible (sourceExpanded_ && src == 4);
    oscStrLbl_.setVisible (sourceExpanded_ && src == 4);
    oscAddrStrEditor_.setVisible (sourceExpanded_ && src == 4);
    oscFloatLbl_.setVisible (sourceExpanded_ && src == 4);
    oscAddrFloatEditor_.setVisible (sourceExpanded_ && src == 4);
    oscFloatTypeLbl_.setVisible (sourceExpanded_ && src == 4);
    oscFloatTypeCombo_.setVisible (sourceExpanded_ && src == 4);
    oscFloatMaxLbl_.setVisible (sourceExpanded_ && src == 4 && oscFloatTypeCombo_.getSelectedId() == 3);
    oscFloatMaxEditor_.setVisible (sourceExpanded_ && src == 4 && oscFloatTypeCombo_.getSelectedId() == 3);

    outDriverLbl_.setVisible (outLtcExpanded_);
    ltcOutDriverCombo_.setVisible (outLtcExpanded_);
    outDeviceLbl_.setVisible (outLtcExpanded_);
    ltcOutDeviceCombo_.setVisible (outLtcExpanded_);
    outChannelLbl_.setVisible (outLtcExpanded_);
    ltcOutChannelCombo_.setVisible (outLtcExpanded_);
    outRateLbl_.setVisible (outLtcExpanded_);
    ltcOutSampleRateCombo_.setVisible (outLtcExpanded_);
    outConvertLbl_.setVisible (outLtcExpanded_);
    ltcConvertStrip_.setVisible (outLtcExpanded_);
    outOffsetLbl_.setVisible (outLtcExpanded_);
    ltcOffsetEditor_.setVisible (outLtcExpanded_);
    outLevelLbl_.setVisible (outLtcExpanded_);
    ltcOutLevelSlider_.setVisible (outLtcExpanded_);

    resAdapterLbl_.setVisible (resolumeExpanded_);
    resolumeAdapterCombo_.setVisible (resolumeExpanded_);
    resListenIpLbl_.setVisible (resolumeExpanded_);
    resListenPortLbl_.setVisible (false);
    resMaxLayersLbl_.setVisible (resolumeExpanded_);
    resMaxClipsLbl_.setVisible (resolumeExpanded_);
    resGlobalOffsetLbl_.setVisible (resolumeExpanded_);
    resolumeListenIp_.setVisible (resolumeExpanded_);
    resolumeListenPort_.setVisible (resolumeExpanded_);
    resolumeMaxLayers_.setVisible (resolumeExpanded_);
    resolumeMaxClips_.setVisible (resolumeExpanded_);
    resolumeGlobalOffset_.setVisible (resolumeExpanded_);

    resSendIpLbl_.setVisible (triggerOutExpanded_);
    resSendIpLbl2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resSendIpLbl3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resSendIpLbl4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resSendIpLbl5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resSendPortLbl_.setVisible (false);
    resolumeSendIp_.setVisible (triggerOutExpanded_);
    resolumeSendPort_.setVisible (triggerOutExpanded_);
    resolumeSendIp2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeSendPort2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeSendIp3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeSendPort3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeSendIp4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeSendPort4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeSendIp5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resolumeSendPort5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resolumeSendExpandBtn1_.setVisible (triggerOutExpanded_);
    resolumeSendExpandBtn2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeSendExpandBtn3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeSendExpandBtn4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeSendExpandBtn5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resSendAdapterLbl1_.setVisible (triggerOutExpanded_ && resolumeSendExpanded1_);
    resSendAdapterLbl2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_);
    resSendAdapterLbl3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_);
    resSendAdapterLbl4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_);
    resSendAdapterLbl5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_);
    resolumeSendAdapterCombo1_.setVisible (triggerOutExpanded_ && resolumeSendExpanded1_);
    resolumeSendAdapterCombo2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_);
    resolumeSendAdapterCombo3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_);
    resolumeSendAdapterCombo4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_);
    resolumeSendAdapterCombo5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_);
    resolumeAddTargetBtn_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ < 5);
    resolumeDelTargetBtn2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeDelTargetBtn3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeDelTargetBtn4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeDelTargetBtn5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);

    getTriggersBtn_.setBounds (getTriggersRow);
    getTriggersBtn_.setVisible (true);
    createCustomBtn_.setBounds (createCustomRow);
    createCustomBtn_.setVisible (true);
    createCustomBtn_.setEnabled (! hasCustomGroup());

    auto buttons = actionRow;
    settingsButton_.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
    quitButton_.setBounds (buttons.reduced (1, 0));

    resolumeStatusLabel_.setBounds (statusLeftRect_.reduced (8, 0));
    statusLabel_.setBounds (statusRightRect_.reduced (8, 0));

    rightSectionRects_.add (right);
    triggerTable_.setBounds (right.reduced (3));
    updateTableColumnWidths();

    easyLabel_.toFront (false);
    triggerLabel_.toFront (false);
    versionLabel_.toFront (false);
    helpButton_.toFront (false);
    tcLabel_.toFront (false);
    if (fpsIndicatorStrip_ != nullptr)
        fpsIndicatorStrip_->toFront (false);
    getTriggersBtn_.toFront (false);
    createCustomBtn_.toFront (false);
    settingsButton_.toFront (false);
    quitButton_.toFront (false);
    resolumeStatusLabel_.toFront (false);
    statusLabel_.toFront (false);
    leftViewport_.toBack();
}

void TriggerContentComponent::mouseUp (const juce::MouseEvent& event)
{
    if (statusBarRect_.contains (event.getPosition()))
        openStatusMonitorWindow();
}

void TriggerContentComponent::updateTableColumnWidths()
{
    auto& h = triggerTable_.getHeader();
    constexpr int kTableScrollbarW = 8;
    constexpr int kCustomColsW = 184 + 42;   // cols 10, 11 fixed widths
    int available = triggerTable_.getWidth();
    available -= kTableScrollbarW;
    available -= 2;
    // If custom columns (10-12) are present they are fixed-width; subtract them
    // so cols 1-9 only fill the remaining portion and the custom cols stay visible.
    if (h.getIndexOfColumnId (10, false) >= 0)
        available -= kCustomColsW;
    available = juce::jmax (200, available);

    struct Col
    {
        int id;
        int base;
        int min;
    };

    // Adaptive minimum for End Action column:
    //   off  → button(62) + margins(2) + padding          = ~72px
    //   col  → button(62) + gap(4) + field(50) + margins  = ~120px
    //   lc   → button(62) + gap(4) + f1(42) + gap(4) + f2(42) + margins = ~160px
    int endActionMin = 72;
    for (const auto& clip : triggerRows_)
    {
        if (clip.isCustom) continue;
        if (clip.endActionMode == "lc")  { endActionMin = 160; break; }
        if (clip.endActionMode == "col")   endActionMin = 120;
    }

    std::array<Col, 9> cols {{
        { 1,  34,  30 },           // arrow
        { 2,  38,  34 },           // in
        { 3, 186, 150 },           // name
        { 4,  96,  92 },           // count    – "00:00:00:00" + padding
        { 5,  64,  58 },           // range
        { 6,  98,  92 },           // trigger  – timecode
        { 7,  98,  92 },           // duration – timecode
        { 8, 160, endActionMin },  // end action – adaptive per mode
        { 9,  48,  44 }            // test
    }};

    int baseSum = 0;
    for (const auto& c : cols)
        baseSum += c.base;
    const double scale = (double) available / (double) baseSum;

    std::array<int, 9> widths {};
    int used = 0;
    for (size_t i = 0; i < cols.size(); ++i)
    {
        int w = juce::jmax (cols[i].min, (int) std::round ((double) cols[i].base * scale));
        widths[i] = w;
        used += w;
    }

    // Keep Test column fixed-looking; absorb delta mostly into Name.
    const size_t nameIdx = 2; // column id 3
    const size_t endIdx = 7;  // column id 8
    int delta = available - used;
    widths[nameIdx] = juce::jmax (cols[nameIdx].min, widths[nameIdx] + delta);

    auto sumWidths = [&]() -> int
    {
        int s = 0;
        for (auto w : widths) s += w;
        return s;
    };

    int remain = available - sumWidths();
    if (remain != 0)
    {
        widths[endIdx] = juce::jmax (cols[endIdx].min, widths[endIdx] + remain);
        remain = available - sumWidths();
    }
    if (remain != 0)
    {
        widths[nameIdx] = juce::jmax (cols[nameIdx].min, widths[nameIdx] + remain);
    }

    for (size_t i = 0; i < cols.size(); ++i)
        h.setColumnWidth (cols[i].id, widths[i]);
}

void TriggerContentComponent::addCustomColumns()
{
    auto& h = triggerTable_.getHeader();
    if (h.getIndexOfColumnId (10, false) >= 0)
        return; // already present
    const int f = juce::TableHeaderComponent::notResizable;
    h.addColumn ("Custom Trigger", 10, 184, 184, 184, f);
    h.addColumn ("",               11,  42,  42,  42, f);

    constexpr int kExtraW = 184 + 42;
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setSize (juce::jmin (1800, w->getWidth() + kExtraW), w->getHeight());
}

void TriggerContentComponent::removeCustomColumns()
{
    constexpr int kExtraW = 184 + 42;
    auto& h = triggerTable_.getHeader();
    h.removeColumn (10);
    h.removeColumn (11);
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setSize (juce::jmax (1160, w->getWidth() - kExtraW), w->getHeight());
}

void TriggerContentComponent::tableColumnsResized (juce::TableHeaderComponent*)
{
    if (colWidthGuard_)
        return;
    colWidthGuard_ = true;

    auto& h = triggerTable_.getHeader();

    // Fixed minimums: timecode columns must show "00:00:00:00"
    auto enforceMin = [&] (int colId, int minW)
    {
        if (h.getColumnWidth (colId) < minW)
            h.setColumnWidth (colId, minW);
    };
    enforceMin (4, 92);   // count
    enforceMin (6, 92);   // trigger
    enforceMin (7, 92);   // duration

    // Adaptive minimum for End Action based on current clip modes
    int endMin = 72;
    for (const auto& clip : triggerRows_)
    {
        if (clip.endActionMode == "lc")  { endMin = 160; break; }
        if (clip.endActionMode == "col")   endMin = 120;
    }
    enforceMin (8, endMin);

    colWidthGuard_ = false;
}

void TriggerContentComponent::timerCallback()
{
    bridgeEngine_.setLtcInputGain (dbToLinearGain (ltcInGainSlider_.getValue()));
    bridgeEngine_.setLtcOutputGain (dbToLinearGain (ltcOutLevelSlider_.getValue()));
    bridgeEngine_.setOffsets (offsetFromEditor (ltcOffsetEditor_), 0, 0);
    const auto st = bridgeEngine_.tick();
    hasLiveInputTc_ = st.hasInputTc;
    if (st.hasInputTc)
    {
        liveInputTc_ = st.inputTc;
        liveInputFps_ = st.inputFps;
    }
    const float peak = bridgeEngine_.getLtcInputPeakLevel();
    ltcInLevelSmoothed_ = (peak > ltcInLevelSmoothed_) ? peak : (ltcInLevelSmoothed_ * 0.85f);
    ltcInLevelBar_.setLevel (ltcInLevelSmoothed_);
    updateClipCountdowns();
    if (st.hasInputTc)
        triggerTable_.repaint(); // refresh countdown column every tick when TC is live
    if (st.hasInputTc)
    {
        hasLatchedTc_ = true;
        latchedTc_ = st.inputTc;
        latchedFps_ = st.inputFps;
        tcLabel_.setText (st.inputTc.toDisplayString (st.inputFps).replaceCharacter ('.', ':'), juce::dontSendNotification);
        if (fpsIndicatorStrip_ != nullptr)
            fpsIndicatorStrip_->setActiveFps (st.inputFps);
    }
    else
    {
        if (hasLatchedTc_)
        {
            tcLabel_.setText (latchedTc_.toDisplayString (latchedFps_).replaceCharacter ('.', ':'), juce::dontSendNotification);
            if (fpsIndicatorStrip_ != nullptr)
                fpsIndicatorStrip_->setActiveFps (latchedFps_);
        }
        else
        {
            tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
            if (fpsIndicatorStrip_ != nullptr)
                fpsIndicatorStrip_->setActiveFps (std::nullopt);
        }
    }
    evaluateAndFireTriggers();
    processEndActions();

    if (queryPending_ && (juce::Time::currentTimeMillis() - queryStartMs_) > 3000)
    {
        queryPending_ = false;
        DarkDialog::show ("Easy Trigger",
                          "Could not receive clips from Resolume.\n"
                          "Please check IP address and port settings.",
                          getParentComponent());
    }

    // Build status counts directly from triggerRows_ so that Clear clip triggers
    // immediately reflects 0 clips without waiting for clipCollector_ to drain.
    int clipCount = 0, customCount = 0, maxLayer = 0;
    for (const auto& t : triggerRows_)
    {
        if (t.isCustom) { ++customCount; }
        else { ++clipCount; maxLayer = juce::jmax (maxLayer, t.layer); }
    }
    auto resolumeStatus = "Layers: " + juce::String (maxLayer)
                        + " | Clips: " + juce::String (clipCount);
    if (customCount > 0)
        resolumeStatus += " | Custom: " + juce::String (customCount);

    auto ltcStatusText = juce::String ("LTC ") + st.ltcOutStatus;
    if (! st.ltcOutStatus.equalsIgnoreCase ("OFF")
        && ! st.ltcOutStatus.equalsIgnoreCase ("THRU")
        && st.ltcOutFps.has_value())
    {
        ltcStatusText += " " + frameRateToString (*st.ltcOutFps);
    }

    setTimecodeStatusText ((st.hasInputTc ? "RUNNING" : "STOPPED - no timecode")
                           + juce::String (" | ") + ltcStatusText,
                           st.hasInputTc ? juce::Colour::fromRGB (0x51, 0xc8, 0x7b)
                                         : juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    setResolumeStatusText (resolumeStatus, juce::Colour::fromRGB (0xa0, 0xa4, 0xac));
}

int TriggerContentComponent::getNumRows()
{
    return (int) displayRows_.size();
}

void TriggerContentComponent::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) displayRows_.size()))
        return;
    const auto& dr = displayRows_[(size_t) row];
    if (dr.isGroup)
    {
        const bool enabled = layerEnabled_[dr.layer];
        g.setColour (enabled ? kSection : juce::Colour::fromRGB (0x3e, 0x3e, 0x42));
        g.fillRoundedRectangle (juce::Rectangle<float> (1.0f, 1.0f, (float) (width - 2), (float) (height - 2)), 6.0f);
        g.setColour (enabled ? juce::Colour::fromRGB (0x70, 0x70, 0x70) : juce::Colour::fromRGB (0x50, 0x50, 0x58));
        g.drawRoundedRectangle (juce::Rectangle<float> (1.0f, 1.0f, (float) (width - 2), (float) (height - 2)), 6.0f, 1.0f);
        return;
    }
    if (juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
    {
        const auto& clip = triggerRows_[(size_t) dr.clipIndex];
        const bool fired = (currentTriggerKeys_.find ({ clip.layer, clip.clip }) != currentTriggerKeys_.end());
        juce::Colour fill;
        if (! clip.include)
        {
            fill = clipIdleFillFor (clip.isCustom, clip.hasOffset, false);
        }
        else if (fired)
        {
            fill = juce::Colour::fromRGB (0xce, 0x9c, 0x00);
        }
        else if (clip.connected)
        {
            fill = clipConnectedFillFor (clip.isCustom, clip.hasOffset);
        }
        else
        {
            fill = clipIdleFillFor (clip.isCustom, clip.hasOffset, true);
        }

        auto rr = juce::Rectangle<float> (0.0f, 1.0f, (float) width, (float) juce::jmax (0, height - 2));
        g.setColour (fill);
        g.fillRoundedRectangle (rr, 5.0f);
        return;
    }
    else
    {
        g.setColour (input_);
    }
    g.fillRect (0, 0, width, height - 1);
}

void TriggerContentComponent::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool selected)
{
    juce::ignoreUnused (selected);
    if (! juce::isPositiveAndBelow (row, (int) displayRows_.size()))
        return;
    const auto& dr = displayRows_[(size_t) row];

    juce::String text;
    if (dr.isGroup)
    {
        const bool expanded = layerExpanded_[dr.layer];
        const bool enabled = layerEnabled_[dr.layer];
        if (columnId == 1)
        {
            auto arrowB = juce::Rectangle<float> (4.0f, 6.0f, 28.0f, (float) height - 12.0f).withSizeKeepingCentre (28.0f, 28.0f);
            g.setColour (juce::Colour::fromRGB (0x48, 0x48, 0x48));
            g.fillEllipse (arrowB);
            g.setColour (juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
            g.drawEllipse (arrowB, 1.0f);

            juce::Path p;
            const auto cx = arrowB.getCentreX();
            const auto cy = arrowB.getCentreY();
            const float s = 5.8f;
            if (expanded)
            {
                p.startNewSubPath (cx - s, cy - 2.0f);
                p.lineTo (cx + s, cy - 2.0f);
                p.lineTo (cx, cy + s);
            }
            else
            {
                p.startNewSubPath (cx - 2.0f, cy - s);
                p.lineTo (cx - 2.0f, cy + s);
                p.lineTo (cx + s, cy);
            }
            p.closeSubPath();
            g.setColour (expanded ? juce::Colour::fromRGB (0xf2, 0xf2, 0xf2) : juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
            g.fillPath (p);
            return;
        }
        else if (columnId == 2)
        {
            auto b = juce::Rectangle<float> (12.0f, 10.0f, 20.0f, (float) height - 20.0f).withSizeKeepingCentre (20.0f, 20.0f);
            g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
            g.drawEllipse (b, 1.4f);
            g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
            g.fillEllipse (b.reduced (1.6f));
            if (enabled)
            {
                g.setColour (juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
                g.fillEllipse (b.reduced (5.0f));
            }
            return;
        }
        else if (columnId == 3)
        {
            // Custom group header: name rendered by InlineTextCell component in refreshComponentForCell
            if (dr.layer == 0) return;
            // Filter out Resolume default names: "", "#", "Layer #", "Layer #N"
            auto isCustomName = [] (const juce::String& s) -> bool
            {
                const auto t = s.trim();
                return t.isNotEmpty() && !t.startsWith ("#") && !t.startsWith ("Layer #");
            };
            juce::String layerName;
            for (const auto& c : triggerRows_)
                if (c.layer == dr.layer && isCustomName (c.layerName)) { layerName = c.layerName.trim(); break; }
            text = layerName.isNotEmpty() ? layerName : ("Layer " + juce::String (dr.layer));
        }

        g.setColour (enabled ? juce::Colour::fromRGB (0xe0, 0xe0, 0xe0) : juce::Colour::fromRGB (0xa3, 0xa3, 0xa3));
        g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        g.drawText (text, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
        return;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return;
    const auto& it = triggerRows_[(size_t) dr.clipIndex];
    switch (columnId)
    {
        case 1:
        {
            if (it.isCustom)
                return;

            const bool hasTc = it.isCustom || it.hasOffset;
            const bool fired = (currentTriggerKeys_.find ({ it.layer, it.clip }) != currentTriggerKeys_.end());
            const auto badgeText = hasTc ? juce::String ("TC") : juce::String ("C");
            auto badge = juce::Rectangle<float> (6.0f, 0.0f, (float) width - 12.0f, (float) height)
                             .withSizeKeepingCentre (22.0f, 22.0f);
            g.setColour ((fired || it.connected) ? juce::Colour::fromRGB (0x18, 0x18, 0x18)
                                                 : juce::Colour::fromRGB (0x50, 0x50, 0x50));
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setColour ((fired || it.connected) ? juce::Colour::fromRGB (0x18, 0x18, 0x18)
                                                 : juce::Colour::fromRGB (0xc8, 0xc8, 0xc8));
            g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
            g.drawFittedText (badgeText, badge.getSmallestIntegerContainer(), juce::Justification::centred, 1);
            return;
        }
        case 2:
        {
            auto b = juce::Rectangle<float> (12.0f, 10.0f, 20.0f, (float) height - 20.0f).withSizeKeepingCentre (20.0f, 20.0f);
            g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
            g.drawEllipse (b, 1.4f);
            g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
            g.fillEllipse (b.reduced (1.6f));
            if (it.include)
            {
                g.setColour (juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
                g.fillEllipse (b.reduced (5.0f));
            }
            return;
        }
        case 3: text = ""; break;
        case 4: text = it.countdownTc; break;
        case 5: text = ""; break;
        case 6: text = ""; break;
        case 7: text = ""; break;
        case 8: text = ""; break;
        case 9: text = "";
            break;
        default: break;
    }
    juce::Colour textColour = juce::Colours::white.withAlpha (0.9f);
    if (columnId == 4)
    {
        const bool fired = (currentTriggerKeys_.find ({ it.layer, it.clip }) != currentTriggerKeys_.end());
        if (! it.include) textColour = juce::Colour::fromRGB (0x58, 0x58, 0x60);
        else if (fired)        textColour = juce::Colour::fromRGB (0x20, 0x14, 0x00);
        else if (it.connected) textColour = juce::Colour::fromRGB (0x18, 0x18, 0x18);
        else                   textColour = juce::Colour::fromRGB (0xe0, 0xe0, 0xe0);
    }
    g.setColour (textColour);
    g.setFont (clipRowFont());
    g.drawText (text, (columnId == 3 ? 12 : 6), 0, width - 8, height, juce::Justification::centredLeft, true);
}

juce::Component* TriggerContentComponent::refreshComponentForCell (int rowNumber, int columnId, bool, juce::Component* existing)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return nullptr;
    const auto& dr = displayRows_[(size_t) rowNumber];
    if (dr.isGroup)
    {
        if (dr.layer == 0)
        {
            // Custom group header: col 10 = [+] add, col 11 = [-] del group
            if (columnId == 10)
            {
                auto* btn = dynamic_cast<InlineAddButtonCell*> (existing);
                if (btn == nullptr) btn = new InlineAddButtonCell();
                btn->onPress = [this] { addCustomColTrigger(); };
                return btn;
            }
            if (columnId == 11)
            {
                auto* btn = dynamic_cast<InlineDeleteButtonCell*> (existing);
                if (btn == nullptr) btn = new InlineDeleteButtonCell();
                btn->onPress = [this] { deleteCustomGroup(); };
                return btn;
            }
            // Col 3: editable custom group name
            if (columnId == 3)
            {
                auto* ed = dynamic_cast<InlineTextCell*> (existing);
                if (ed == nullptr) ed = new InlineTextCell();
                const bool enabled = layerEnabled_[0];
                const juce::Colour textCol = enabled
                    ? juce::Colour::fromRGB (0xe0, 0xe0, 0xe0)
                    : juce::Colour::fromRGB (0x9a, 0x9a, 0xa5);
                ed->applyColourToAllText (textCol, true);
                ed->applyFontToAllText (juce::Font (juce::FontOptions (13.0f).withStyle ("bold")), true);
                ed->setText (customGroupName_, juce::dontSendNotification);
                ed->onCommit = [this] (const juce::String& v)
                {
                    customGroupName_ = v.trim().isNotEmpty() ? v.trim() : juce::String ("Custom Trigger");
                    triggerTable_.repaint();
                };
                return ed;
            }
            // Col 5 (Range) falls through; all other cols hide stale components
            if (columnId != 5)
            {
                if (existing != nullptr) existing->setVisible (false);
                return nullptr;
            }
        }

        if (columnId == 5)
        {
            // Editable Range field on the group header row — syncs to all clips in the layer
            auto* ed = dynamic_cast<InlineTextCell*> (existing);
            if (ed == nullptr)
                ed = new InlineTextCell();

            const bool enabled = layerEnabled_[dr.layer];
            const juce::Colour textCol = enabled
                ? juce::Colour::fromRGB (0xe0, 0xe0, 0xe0)
                : juce::Colour::fromRGB (0x9a, 0x9a, 0xa5);
            const juce::Font cellFont (juce::FontOptions (13.0f).withStyle ("Bold"));
            ed->applyColourToAllText (textCol, true);
            ed->applyFontToAllText (cellFont, true);

            double rangeVal = 0.0;
            for (const auto& row : triggerRows_)
                if (row.layer == dr.layer) { rangeVal = row.triggerRangeSec; break; }
            ed->setText (juce::String (rangeVal, 1), juce::dontSendNotification);

            ed->onCommit = [this, rowNumber] (const juce::String& v)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                const auto& dr2 = displayRows_[(size_t) rowNumber];
                if (! dr2.isGroup) return;
                const double newRange = juce::jmax (0.0, v.trim().replaceCharacter (',', '.').getDoubleValue());
                for (auto& c : triggerRows_)
                    if (c.layer == dr2.layer)
                        c.triggerRangeSec = newRange;
                triggerTable_.updateContent();
                triggerTable_.repaint();
            };
            return ed;
        }

        // For all other group-row columns, hide any stale clip-row component and remove it
        if (existing != nullptr)
            existing->setVisible (false);
        return nullptr;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return nullptr;

    auto& clip = triggerRows_[(size_t) dr.clipIndex];

    // ── Custom trigger clip row ──────────────────────────────────────────────
    if (clip.isCustom)
    {
        if (columnId == 9)
        {
            auto* btn = dynamic_cast<InlineTestButtonCell*> (existing);
            if (btn == nullptr) btn = new InlineTestButtonCell();
            btn->onPress = [this, rowNumber]
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
                auto& c = triggerRows_[(size_t) dr2.clipIndex];
                fireCustomTrigger (c);
                // Highlight row: clear connected for all custom clips, mark this one
                for (auto& t : triggerRows_)
                    if (t.layer == c.layer) t.connected = false;
                c.connected = true;
                triggerTable_.updateContent();
                triggerTable_.repaint();
            };
            return btn;
        }
        if (columnId == 10)
        {
            // Col / L-C selector widget (same pattern as End Action)
            auto* cell = dynamic_cast<InlineCustomTypeCell*> (existing);
            if (cell == nullptr) cell = new InlineCustomTypeCell();
            cell->setState (clip.customType, clip.customSourceCol,
                            clip.customSourceLayer, clip.customSourceClip);
            cell->onChanged = [this, rowNumber] (const juce::String& type,
                                                  const juce::String& col,
                                                  const juce::String& layer,
                                                  const juce::String& clipVal)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
                auto& c = triggerRows_[(size_t) dr2.clipIndex];
                c.customType        = type;
                c.customSourceCol   = col;
                c.customSourceLayer = layer;
                c.customSourceClip  = clipVal;
            };
            return cell;
        }
        if (columnId == 11)
        {
            auto* btn = dynamic_cast<InlineDeleteButtonCell*> (existing);
            if (btn == nullptr) btn = new InlineDeleteButtonCell();
            btn->onPress = [this, rowNumber]
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                deleteCustomTrigger (dr2.clipIndex);
            };
            return btn;
        }
        // Cols 3, 5, 6, 7, 8 fall through to standard handlers below
    }

    if (columnId == 3 || columnId == 5 || columnId == 6 || columnId == 7)
    {
        auto* ed = dynamic_cast<InlineTextCell*> (existing);
        if (ed == nullptr)
            ed = new InlineTextCell();

        const bool fired = (currentTriggerKeys_.find ({ clip.layer, clip.clip }) != currentTriggerKeys_.end());
        juce::Colour textCol = juce::Colour::fromRGB (0xe0, 0xe0, 0xe0);
        if (! clip.include)      textCol = juce::Colour::fromRGB (0x58, 0x58, 0x60);
        else if (fired)          textCol = juce::Colour::fromRGB (0x20, 0x14, 0x00);
        else if (clip.connected) textCol = juce::Colour::fromRGB (0x18, 0x18, 0x18);
        // applyColourToAllText/applyFontToAllText update existing sections immediately;
        // setColour(textColourId) alone only affects future inserts and is a no-op for existing text.
        const juce::Font cellFont (clipRowFont());
        ed->applyColourToAllText (textCol, true);
        ed->applyFontToAllText (cellFont, true);

        if (columnId == 3) ed->setText (clip.name, juce::dontSendNotification);
        if (columnId == 5) ed->setText (juce::String (clip.triggerRangeSec, 1), juce::dontSendNotification);
        if (columnId == 6) ed->setText (clip.triggerTc, juce::dontSendNotification);
        if (columnId == 7) ed->setText (clip.durationTc, juce::dontSendNotification);
        ed->onCommit = [this, rowNumber, columnId] (const juce::String& v)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            if (columnId == 3) c.name = v.trim();
            if (columnId == 5) c.triggerRangeSec = juce::jmax (0.0, v.trim().replaceCharacter (',', '.').getDoubleValue());
            if (columnId == 6) c.triggerTc = v.trim();
            if (columnId == 7) c.durationTc = v.trim();
            triggerTable_.repaint();
        };
        return ed;
    }

    if (columnId == 8)
    {
        auto* cell = dynamic_cast<InlineEndActionCell*> (existing);
        if (cell == nullptr)
            cell = new InlineEndActionCell();
        cell->setState (clip.endActionMode, clip.endActionCol, clip.endActionLayer, clip.endActionClip);
        cell->onChanged = [this, rowNumber] (const juce::String& mode,
                                             const juce::String& col,
                                             const juce::String& layer,
                                             const juce::String& clipValue)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            c.endActionMode = mode;
            c.endActionCol = col;
            c.endActionLayer = layer;
            c.endActionClip = clipValue;
            updateTableColumnWidths();
            triggerTable_.updateContent();
            triggerTable_.repaint();
        };
        return cell;
    }

    if (columnId == 9)
    {
        auto* btn = dynamic_cast<InlineTestButtonCell*> (existing);
        if (btn == nullptr)
            btn = new InlineTestButtonCell();
        btn->onPress = [this, rowNumber]
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            sendTestTrigger (c.layer, c.clip);
            for (auto& t : triggerRows_) if (t.layer == c.layer) t.connected = false;
            c.connected = true;

            // Schedule end action (same logic as evaluateAndFireTriggers)
            const juce::String eMode = c.endActionMode.trim().toLowerCase();
            if (eMode == "col" || eMode == "lc")
            {
                int durFrames = 0;
                if (parseTcToFrames (c.durationTc, 25, durFrames) && durFrames > 0)
                {
                    const double durSec = (double) durFrames / 25.0;
                    PendingEndAction ea;
                    ea.executeTs = juce::Time::getMillisecondCounterHiRes() * 0.001 + durSec;
                    ea.mode  = eMode;
                    ea.col   = c.endActionCol;
                    ea.layer = c.endActionLayer;
                    ea.clip  = c.endActionClip;
                    pendingEndActions_[{ c.layer, c.clip }] = ea;
                }
            }
            else
            {
                pendingEndActions_.erase ({ c.layer, c.clip });
            }

            triggerTable_.updateContent();
            triggerTable_.repaint();
        };
        return btn;
    }
    return nullptr;
}

void TriggerContentComponent::cellClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return;

    const auto& dr = displayRows_[(size_t) rowNumber];
    if (dr.isGroup)
    {
        if (columnId == 1)
        {
            layerExpanded_[dr.layer] = ! layerExpanded_[dr.layer];
        }
        else if (columnId == 2)
        {
            const bool newEnabled = ! layerEnabled_[dr.layer];
            layerEnabled_[dr.layer] = newEnabled;
            for (auto& t : triggerRows_)
                if (t.layer == dr.layer)
                {
                    t.include = newEnabled;
                    if (! newEnabled)
                        currentTriggerKeys_.erase ({ t.layer, t.clip });
                }
        }
        rebuildDisplayRows();
        triggerTable_.updateContent();
        triggerTable_.repaint();
        return;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return;
    if (columnId == 2)
    {
        auto& clip = triggerRows_[(size_t) dr.clipIndex];
        clip.include = ! clip.include;
        if (! clip.include)
            currentTriggerKeys_.erase ({ clip.layer, clip.clip });
        bool anyIncluded = false;
        for (const auto& t : triggerRows_)
            if (t.layer == clip.layer && t.include)
                anyIncluded = true;
        layerEnabled_[clip.layer] = anyIncluded;
        triggerTable_.repaint();
        return;
    }
    juce::ignoreUnused (columnId);
}

void TriggerContentComponent::cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    juce::ignoreUnused (rowNumber, columnId);
}

void TriggerContentComponent::loadFonts()
{
    auto loadFont = [] (const juce::File& f) -> juce::Font
    {
        if (! f.existsAsFile()) return {};
        juce::MemoryBlock data;
        f.loadFileAsData (data);
        auto tf = juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
        return tf != nullptr ? juce::Font (juce::FontOptions (tf)) : juce::Font();
    };

    auto base = findUiBaseDirFromExe();
    juce::File fontsDir;
    if (base.exists())
        fontsDir = base.getChildFile ("Fonts");
    if (! fontsDir.exists())
    {
        auto cwd = juce::File::getCurrentWorkingDirectory();
        auto project = cwd.getChildFile ("EASYTRIGGER-JYCE").getChildFile ("Fonts");
        auto local = cwd.getChildFile ("EasyTrigger").getChildFile ("Fonts");
        if (project.exists()) fontsDir = project;
        else if (local.exists()) fontsDir = local;
    }

    headerBold_ = loadFont (fontsDir.getChildFile ("Thunder-SemiBoldLC.ttf"));
    headerLight_ = loadFont (fontsDir.getChildFile ("Thunder-LightLC.ttf"));
    mono_ = loadFont (fontsDir.getChildFile ("JetBrainsMonoNL-Bold.ttf"));
    if (headerBold_.getHeight() <= 0.0f) headerBold_ = juce::FontOptions (30.0f);
    if (headerLight_.getHeight() <= 0.0f) headerLight_ = juce::FontOptions (30.0f);
    if (mono_.getHeight() <= 0.0f) mono_ = juce::FontOptions (40.0f);
}

void TriggerContentComponent::applyTheme()
{
    lookAndFeel_ = std::make_unique<TriggerLookAndFeel>();
    lookAndFeel_->setColour (juce::ComboBox::backgroundColourId, input_);
    lookAndFeel_->setColour (juce::ComboBox::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
    lookAndFeel_->setColour (juce::ComboBox::outlineColourId, row_);
    lookAndFeel_->setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (0x9a, 0xa1, 0xac));

    // Dropdown list style (same as Bridge).
    lookAndFeel_->setColour (juce::PopupMenu::backgroundColourId, input_);
    lookAndFeel_->setColour (juce::PopupMenu::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
    lookAndFeel_->setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    lookAndFeel_->setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    lookAndFeel_->setColour (juce::PopupMenu::headerTextColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));

    // Scrollbar inside popup menu.
    lookAndFeel_->setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));
    lookAndFeel_->setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    lookAndFeel_->setColour (juce::ScrollBar::trackColourId, juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));
    leftViewport_.getVerticalScrollBar().setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
    leftViewport_.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
    leftViewport_.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));

    lookAndFeel_->setColour (juce::TextEditor::backgroundColourId, input_);
    lookAndFeel_->setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
    lookAndFeel_->setColour (juce::TextEditor::outlineColourId, row_);
    juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel_.get());
    setLookAndFeel (lookAndFeel_.get());

    auto styleEditor = [this] (juce::TextEditor& e)
    {
        e.setColour (juce::TextEditor::backgroundColourId, input_);
        e.setColour (juce::TextEditor::outlineColourId, row_);
        e.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (0x6a, 0x6a, 0x6a));
        e.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        e.setJustification (juce::Justification::centredLeft);
        e.setIndents (8, 0);
    };
    for (auto* e : { &resolumeSendIp_, &resolumeSendPort_,
                     &resolumeSendIp2_, &resolumeSendPort2_,
                     &resolumeSendIp3_, &resolumeSendPort3_,
                     &resolumeSendIp4_, &resolumeSendPort4_,
                     &resolumeSendIp5_, &resolumeSendPort5_,
                     &resolumeListenIp_, &resolumeListenPort_,
                     &resolumeMaxLayers_, &resolumeMaxClips_, &resolumeGlobalOffset_ })
    {
        styleEditor (*e);
    }
    for (auto* l : { &resolumeHeader_,
                     &triggerOutHeaderLabel_,
                     &resAdapterLbl_,
                     &resSendIpLbl_, &resSendIpLbl2_, &resSendIpLbl3_, &resSendIpLbl4_, &resSendIpLbl5_,
                     &resSendAdapterLbl1_, &resSendAdapterLbl2_, &resSendAdapterLbl3_, &resSendAdapterLbl4_, &resSendAdapterLbl5_,
                     &resSendPortLbl_, &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_, &resGlobalOffsetLbl_ })
    {
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        l->setJustificationType (juce::Justification::centredLeft);
    }
    for (auto* h : { &resolumeHeader_, &triggerOutHeaderLabel_ })
    {
        h->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
        h->setFont (juce::FontOptions (14.0f));
        h->setJustificationType (juce::Justification::centredLeft);
    }
    resolumeStatusLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    resolumeStatusLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    statusLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    resolumeStatusLabel_.setJustificationType (juce::Justification::centredLeft);
    statusLabel_.setJustificationType (juce::Justification::centredRight);
    // Pass mouse events through so the status bar click (openStatusMonitorWindow) reaches the parent
    resolumeStatusLabel_.setInterceptsMouseClicks (false, false);
    statusLabel_.setInterceptsMouseClicks (false, false);
    getTriggersBtn_.setColour (juce::TextButton::buttonColourId, row_);
    getTriggersBtn_.setColour (juce::TextButton::buttonOnColourId, row_);
    getTriggersBtn_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    getTriggersBtn_.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    createCustomBtn_.setColour (juce::TextButton::buttonColourId,   row_);
    createCustomBtn_.setColour (juce::TextButton::buttonOnColourId, row_);
    createCustomBtn_.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
    createCustomBtn_.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);

    for (auto* b : { &resolumeAddTargetBtn_, &resolumeDelTargetBtn2_, &resolumeDelTargetBtn3_, &resolumeDelTargetBtn4_, &resolumeDelTargetBtn5_ })
    {
        b->setColour (juce::TextButton::buttonColourId, input_);
        b->setColour (juce::TextButton::buttonOnColourId, input_);
        b->setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        b->setColour (juce::TextButton::textColourOnId, kBg);
    }
}

void TriggerContentComponent::openHelpPage()
{
    auto base = findUiBaseDirFromExe();
    if (! base.exists())
        return;

    auto help = base.getChildFile ("Help/easy_trigger_help.html");
    if (help.existsAsFile())
        juce::URL (help.getFullPathName()).launchInDefaultBrowser();
}


void TriggerContentComponent::refreshTriggerRows()
{
    // Preserve custom (user-created) triggers across Resolume refreshes
    std::vector<TriggerClip> savedCustom;
    for (const auto& c : triggerRows_)
        if (c.isCustom) savedCustom.push_back (c);

    std::map<juce::String, TriggerClip> prevByKey;
    for (const auto& p : triggerRows_)
        if (! p.isCustom)
            prevByKey[juce::String (p.layer) + ":" + juce::String (p.clip)] = p;

    triggerRows_.clear();
    auto clips = clipCollector_.snapshot();
    std::sort (clips.begin(), clips.end(), [] (const auto& a, const auto& b)
    {
        if (a.layer != b.layer) return a.layer < b.layer;
        return a.clip < b.clip;
    });

    for (const auto& c : clips)
    {
        if (c.durationSeconds <= 0.0)
            continue;

        const auto key = juce::String (c.layer) + ":" + juce::String (c.clip);
        const bool includeThisClip = c.hasOffset ? includeClipsWithOffset_
                                                 : includeClipsWithoutOffset_;
        const bool wasTracked = prevByKey.find (key) != prevByKey.end();
        if (! includeThisClip && ! wasTracked)
            continue;

        TriggerClip row;
        row.layer = c.layer;
        row.clip = c.clip;
        row.include = true;
        row.hasOffset = c.hasOffset;
        row.name = c.clipName.isNotEmpty() ? c.clipName : ("Layer " + juce::String (c.layer) + " Clip " + juce::String (c.clip));
        row.layerName = c.layerName;
        row.countdownTc = "00:00:00:00";
        row.triggerRangeSec = 5.0;
        row.durationTc = secondsToTc (c.durationSeconds, FrameRate::FPS_25);
        row.triggerTc = c.hasOffset ? secondsToTc (c.offsetSeconds, FrameRate::FPS_25)
                                    : "00:00:00:00";
        row.endActionMode = "off";
        row.connected = c.connected;
        row.timecodeHit = false;

        if (auto it = prevByKey.find (key); it != prevByKey.end())
        {
            const auto& old = it->second;
            // Keep local trigger configuration, but always refresh live clip data from Resolume.
            row.include = old.include;
            row.triggerRangeSec = old.triggerRangeSec;
            row.endActionMode = old.endActionMode;
            row.endActionCol = old.endActionCol;
            row.endActionLayer = old.endActionLayer;
            row.endActionClip = old.endActionClip;
        }
        triggerRows_.push_back (row);
    }

    // Restore custom triggers at the end (they sort to layer=0, always first)
    for (const auto& c : savedCustom)
        triggerRows_.push_back (c);

    {
        std::set<std::pair<int, int>> valid;
        for (const auto& t : triggerRows_)
            valid.insert ({ t.layer, t.clip });
        for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        {
            if (valid.find (*it) == valid.end()) it = currentTriggerKeys_.erase (it);
            else ++it;
        }
    }

    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::updateClipCountdowns()
{
    if (! hasLiveInputTc_)
    {
        for (auto& t : triggerRows_)
        {
            t.countdownTc = "00:00:00:00";
            t.timecodeHit = false;
        }
        hasLastInputFrames_ = false;
        triggerRangeActive_.clear();
        return;
    }

    const int fps = juce::jmax (1, frameRateToInt (liveInputFps_));
    const auto currentTc = liveInputTc_.toDisplayString (liveInputFps_).replaceCharacter ('.', ':');
    int currentFrames = 0;
    if (! parseTcToFrames (currentTc, fps, currentFrames))
        return;
    currentFrames -= globalOffsetFramesFromEditor (resolumeGlobalOffset_, fps);

    for (auto& t : triggerRows_)
    {
        int triggerFrames = 0;
        if (! parseTcToFrames (t.triggerTc, fps, triggerFrames))
        {
            t.countdownTc = "00:00:00:00";
            t.timecodeHit = false;
            continue;
        }
        const int remain = triggerFrames - currentFrames;
        t.timecodeHit = std::abs (remain) <= juce::jmax (1, (int) std::round (t.triggerRangeSec * fps));
        if (remain <= 0)
            t.countdownTc = "00:00:00:00";
        else
            t.countdownTc = secondsToTc ((double) remain / (double) fps, liveInputFps_);
    }
}

void TriggerContentComponent::evaluateAndFireTriggers()
{
    if (! hasLiveInputTc_)
        return;
    if (triggerRows_.empty())
        return;

    const int fps = juce::jmax (1, frameRateToInt (liveInputFps_));
    const auto currentTc = liveInputTc_.toDisplayString (liveInputFps_).replaceCharacter ('.', ':');
    int currentFrames = 0;
    if (! parseTcToFrames (currentTc, fps, currentFrames))
        return;
    currentFrames -= globalOffsetFramesFromEditor (resolumeGlobalOffset_, fps);

    if (! hasLastInputFrames_)
    {
        lastInputFrames_ = currentFrames;
        hasLastInputFrames_ = true;
        return;
    }

    const int prevFrames = lastInputFrames_;
    lastInputFrames_ = currentFrames;

    if (currentFrames < prevFrames)
    {
        // Treat any backward jump/rewind as a re-arm event: do not fire triggers
        // while moving backwards, and clear one-shot/range state so they can fire
        // again when time moves forward through the markers.
        triggerRangeActive_.clear();
        currentTriggerKeys_.clear();
        pendingEndActions_.clear();
        for (auto& t : triggerRows_)
            t.connected = false;
        triggerTable_.updateContent();
        triggerTable_.repaint();
        return;
    }

    struct Candidate
    {
        int index { -1 };
        int score { (std::numeric_limits<int>::max)() };
        bool isCrossOnly { false };
    };

    std::map<int, Candidate> bestByLayer;
    std::map<std::pair<int, int>, bool> newRangeState;

    for (int i = 0; i < (int) triggerRows_.size(); ++i)
    {
        auto& t = triggerRows_[(size_t) i];
        const auto key = std::make_pair (t.layer, t.clip);

        const bool layerEnabled = (layerEnabled_.find (t.layer) == layerEnabled_.end()) ? true : layerEnabled_[t.layer];
        if (! t.include || ! layerEnabled)
        {
            newRangeState[key] = false;
            continue;
        }

        int trig = 0;
        if (! parseTcToFrames (t.triggerTc, fps, trig))
        {
            newRangeState[key] = false;
            continue;
        }

        const int range = juce::jmax (1, (int) std::round (t.triggerRangeSec * fps));
        const int winStart = trig - range;
        const int winEnd = trig + range;
        const bool inNow = currentFrames >= winStart && currentFrames <= winEnd;
        const bool wasIn = triggerRangeActive_.count (key) > 0 ? triggerRangeActive_[key] : false;
        newRangeState[key] = inNow;

        bool crossed = false;
        if (prevFrames != currentFrames)
        {
            const int lo = juce::jmin (prevFrames, currentFrames);
            const int hi = juce::jmax (prevFrames, currentFrames);
            crossed = !(hi < winStart || lo > winEnd);
        }

        if (! inNow && ! crossed)
            continue;
        if (wasIn)
            continue; // already fired for this window pass — skip both "still inside" and exit-side crossing

        auto& best = bestByLayer[t.layer];
        if (best.index < 0)
        {
            best.index = i;
            best.score = std::abs (currentFrames - trig);
            best.isCrossOnly = ! inNow;
            continue;
        }

        const int score = std::abs (currentFrames - trig);
        const bool crossOnly = ! inNow;
        if (best.isCrossOnly && ! crossOnly)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = false;
        }
        else if (best.isCrossOnly == crossOnly && score < best.score)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = crossOnly;
        }
    }

    triggerRangeActive_ = std::move (newRangeState);
    if (bestByLayer.empty())
        return;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastTriggerFireTs_ < 0.01)
        return;
    lastTriggerFireTs_ = now;

    for (const auto& [layer, c] : bestByLayer)
    {
        juce::ignoreUnused (layer);
        if (! juce::isPositiveAndBelow (c.index, (int) triggerRows_.size()))
            continue;
        auto& t = triggerRows_[(size_t) c.index];
        if (t.isCustom)
            fireCustomTrigger (t);
        else
            sendTestTrigger (t.layer, t.clip);
        for (auto& x : triggerRows_)
            if (x.layer == t.layer)
                x.connected = false;
        t.connected = true;

        // Schedule end action (Col / L-C mode), mirroring Python _schedule_end_action_for_trigger
        {
            const juce::String eMode = t.endActionMode.trim().toLowerCase();
            if (eMode == "col" || eMode == "lc")
            {
                int durFrames = 0;
                // durationTc is always stored in FPS_25 format — parse with fixed 25 to avoid
                // live-fps mismatch causing parseTcToFrames to reject valid frame values
                if (parseTcToFrames (t.durationTc, 25, durFrames) && durFrames > 0)
                {
                    const double durSec = (double) durFrames / 25.0;
                    // Align to triggerTc, not to actual fire time.
                    // The trigger may have fired early (within the range window), so offset
                    // forward so the end action fires at wall-clock equivalent of triggerTc + dur.
                    int trigFrames = 0;
                    const double tcAlignOffset = parseTcToFrames (t.triggerTc, fps, trigFrames)
                        ? juce::jmax (0.0, (double) (trigFrames - currentFrames) / (double) fps)
                        : 0.0;
                    PendingEndAction ea;
                    ea.executeTs = now + tcAlignOffset + durSec;
                    ea.mode      = eMode;
                    ea.col       = t.endActionCol;
                    ea.layer     = t.endActionLayer;
                    ea.clip      = t.endActionClip;
                    pendingEndActions_[{ t.layer, t.clip }] = ea;
                }
            }
            else
            {
                // mode is "off" — remove any stale pending action for this clip
                pendingEndActions_.erase ({ t.layer, t.clip });
            }
        }

        // Python parity: keep last fired clip highlighted (orange) per layer until next fire on that layer.
        for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        {
            if (it->first == t.layer) it = currentTriggerKeys_.erase (it);
            else ++it;
        }
        currentTriggerKeys_.insert ({ t.layer, t.clip });
    }
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::processEndActions()
{
    if (pendingEndActions_.empty())
        return;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    const auto targets = collectResolumeSendTargets();

    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
    {
        const auto& ea = it->second;
        if (ea.executeTs > now)
        {
            ++it;
            continue;
        }

        // Build OSC address depending on mode
        juce::String addr;
        if (ea.mode == "col")
        {
            const int col = ea.col.getIntValue();
            if (col > 0)
                addr = "/composition/columns/" + juce::String (col) + "/connect";
        }
        else if (ea.mode == "lc")
        {
            const int lay = ea.layer.getIntValue();
            const int clp = ea.clip.getIntValue();
            if (lay > 0 && clp > 0)
                addr = "/composition/layers/" + juce::String (lay) + "/clips/" + juce::String (clp) + "/connect";
        }

        if (addr.isNotEmpty())
        {
            for (const auto& target : targets)
                sendOscPulse (target, addr);
        }

        it = pendingEndActions_.erase (it);
    }
}

void TriggerContentComponent::restartSelectedSource()
{
    const int src = sourceCombo_.getSelectedId();
    if (src == 1)
        bridgeEngine_.setInputSource (bridge::engine::InputSource::LTC);
    else if (src == 2)
        bridgeEngine_.setInputSource (bridge::engine::InputSource::MTC);
    else if (src == 3)
        bridgeEngine_.setInputSource (bridge::engine::InputSource::ArtNet);
    else
        bridgeEngine_.setInputSource (bridge::engine::InputSource::OSC);
}

void TriggerContentComponent::queueLtcOutputApply()
{
    const bool enabled = ltcOutSwitch_.getState();

    const int selectedId = ltcOutDeviceCombo_.getSelectedId();
    const int idx = selectedId > 0 ? selectedId - 1 : -1;
    if (! juce::isPositiveAndBelow (idx, filteredOutputIndices_.size()))
    {
        if (! enabled)
            bridgeEngine_.setLtcOutputEnabled (false);
        return;
    }

    {
        const std::lock_guard<std::mutex> lock (ltcOutputApplyMutex_);
        pendingLtcOutputChoice_ = outputChoices_[filteredOutputIndices_[idx]];
        pendingLtcOutputChannel_ = comboChannelIndex (ltcOutChannelCombo_);
        pendingLtcOutputSampleRate_ = comboSampleRate (ltcOutSampleRateCombo_);
        pendingLtcOutputBufferSize_ = 256;
        pendingLtcOutputEnabled_ = enabled;
        pendingLtcThruMode_ = ltcThruDot_.getState();
        ltcOutputApplyPending_ = true;
    }

    ltcOutputApplyCv_.notify_one();
}

void TriggerContentComponent::ltcOutputApplyLoop()
{
    auto safeThis = juce::Component::SafePointer<TriggerContentComponent> (this);

    for (;;)
    {
        bridge::engine::AudioChoice choice;
        int channel = 0;
        double sampleRate = 0.0;
        int bufferSize = 0;
        bool enabled = false;
        bool thruMode = false;

        {
            std::unique_lock<std::mutex> lock (ltcOutputApplyMutex_);
            ltcOutputApplyCv_.wait (lock, [this] { return ltcOutputApplyExit_ || ltcOutputApplyPending_; });
            if (ltcOutputApplyExit_)
                break;

            choice = pendingLtcOutputChoice_;
            channel = pendingLtcOutputChannel_;
            sampleRate = pendingLtcOutputSampleRate_;
            bufferSize = pendingLtcOutputBufferSize_;
            enabled = pendingLtcOutputEnabled_;
            thruMode = pendingLtcThruMode_;
            ltcOutputApplyPending_ = false;
        }

        juce::String err;
        if (thruMode && enabled)
        {
            // Thru ON + switch ON → stop normal output, run passthrough
            bridgeEngine_.stopLtcOutput();
            bridgeEngine_.startLtcThru (choice, channel, sampleRate, bufferSize, err);
            if (err.isNotEmpty())
                juce::MessageManager::callAsync ([safeThis, err]
                {
                    if (safeThis != nullptr)
                    {
                        safeThis->setTimecodeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
                        safeThis->ltcThruDot_.setState (false);
                    }
                });
        }
        else if (thruMode && !enabled)
        {
            // Thru ON + switch OFF → stop both, nothing runs
            bridgeEngine_.stopLtcThru();
            bridgeEngine_.stopLtcOutput();
        }
        else
        {
            // Normal mode (Thru OFF) → stop thru, run normal output
            bridgeEngine_.stopLtcThru();
            bridgeEngine_.startLtcOutput (choice, channel, sampleRate, bufferSize, err);
            bridgeEngine_.setLtcOutputEnabled (enabled);
            if (err.isNotEmpty())
                juce::MessageManager::callAsync ([safeThis, err]
                {
                    if (safeThis != nullptr)
                        safeThis->setTimecodeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
                });
        }
    }
}

void TriggerContentComponent::onOutputToggleChanged()
{
    bridgeEngine_.setLtcOutputEnabled (ltcOutSwitch_.getState());
    queueLtcOutputApply();
}

void TriggerContentComponent::onOutputSettingsChanged()
{
    bridgeEngine_.setLtcOutputConvertFps (ltcConvertStrip_.getSelectedFps());
    queueLtcOutputApply();
}

void TriggerContentComponent::onInputSettingsChanged()
{
    juce::String err;

    const int ltcSelectedId = ltcInDeviceCombo_.getSelectedId();
    const int ltcIdx = ltcSelectedId > 0 ? ltcSelectedId - 1 : -1;
    if (juce::isPositiveAndBelow (ltcIdx, filteredInputIndices_.size()))
        bridgeEngine_.startLtcInput (inputChoices_[filteredInputIndices_[ltcIdx]],
                                     comboChannelIndex (ltcInChannelCombo_),
                                     comboSampleRate (ltcInSampleRateCombo_),
                                     0,
                                     err);

    if (mtcInCombo_.getNumItems() > 0)
        bridgeEngine_.startMtcInput (mtcInCombo_.getSelectedItemIndex(), err);

    if (artnetInCombo_.getNumItems() > 0)
    {
        const auto artnetListenIp = artnetListenIpEditor_.getText().trim();
        bridgeEngine_.startArtnetInput (artnetInCombo_.getSelectedItemIndex(),
                                        (artnetListenIp == "0.0.0.0" ? juce::String() : artnetListenIp),
                                        err);
    }

    FrameRate fps = FrameRate::FPS_25;
    if (oscFpsCombo_.getSelectedId() == 1) fps = FrameRate::FPS_24;
    if (oscFpsCombo_.getSelectedId() == 3) fps = FrameRate::FPS_2997;
    if (oscFpsCombo_.getSelectedId() == 4) fps = FrameRate::FPS_30;
    const auto bindIp = (oscIpEditor_.getText().trim().isNotEmpty() ? oscIpEditor_.getText().trim()
                                                                     : parseBindIpFromAdapterLabel (oscAdapterCombo_.getText()));
    const auto floatVt  = static_cast<bridge::engine::OscValueType> (oscFloatTypeCombo_.getSelectedId() - 1);
    // Accept both "7.15" and "7,15" by normalising comma to dot before parsing.
    auto floatMaxRaw = oscFloatMaxEditor_.getText().trim();
    juce::String floatMaxText;
    for (int i = 0; i < floatMaxRaw.length(); ++i)
    {
        auto ch = floatMaxRaw[i];
        if (ch == ',')
            ch = '.';
        floatMaxText += ch;
    }
    const double floatMax = juce::jmax (1.0, floatMaxText.getDoubleValue());
    bridgeEngine_.startOscInput (juce::jlimit (1, 65535, oscPortEditor_.getText().getIntValue()),
                                 bindIp,
                                 fps,
                                 oscAddrStrEditor_.getText(),
                                 oscAddrFloatEditor_.getText(),
                                 floatVt,
                                 floatMax,
                                 err);

    restartSelectedSource();

    if (err.isNotEmpty())
        setTimecodeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
}

void TriggerContentComponent::startAudioDeviceScan()
{
    if (scanThread_ != nullptr && scanThread_->isThreadRunning())
    {
        if (! scanThread_->stopThread (2000))
            return;
    }

    scanThread_ = std::make_unique<AudioScanThread> (this);
    scanThread_->startThread();
}

void TriggerContentComponent::onAudioScanComplete (const juce::Array<bridge::engine::AudioChoice>& inputs,
                                                   const juce::Array<bridge::engine::AudioChoice>& outputs)
{
    const auto prevInDriver = ltcInDriverCombo_.getText();
    const auto prevOutDriver = ltcOutDriverCombo_.getText();
    inputChoices_ = inputs;
    outputChoices_ = outputs;
    fillDriverCombo (ltcInDriverCombo_, inputChoices_, prevInDriver);
    fillDriverCombo (ltcOutDriverCombo_, outputChoices_, prevOutDriver);
    refreshLtcDeviceListsByDriver();
    refreshLtcChannelCombos();
    refreshNetworkMidiLists();
    if (pendingAutoLoad_)
        maybeAutoLoadConfig();
    else
    {
        onInputSettingsChanged();
        onOutputSettingsChanged();
    }
}

void TriggerContentComponent::refreshNetworkMidiLists()
{
    mtcInCombo_.clear();
    auto ins = bridgeEngine_.midiInputs();
    for (int i = 0; i < ins.size(); ++i)
        mtcInCombo_.addItem (ins[i], i + 1);
    if (mtcInCombo_.getNumItems() > 0)
        mtcInCombo_.setSelectedItemIndex (0, juce::dontSendNotification);

    auto ifaces = bridgeEngine_.artnetInterfaces();
    artnetInCombo_.clear();
    oscAdapterCombo_.clear();
    resolumeAdapterCombo_.clear();
    resolumeSendAdapterCombo1_.clear();
    resolumeSendAdapterCombo2_.clear();
    resolumeSendAdapterCombo3_.clear();
    resolumeSendAdapterCombo4_.clear();
    resolumeSendAdapterCombo5_.clear();
    oscAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo1_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo2_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo3_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo4_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo5_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    juce::StringArray oscAdapterItems;
    juce::StringArray networkAdapterItems;
    for (int i = 0; i < ifaces.size(); ++i)
    {
        artnetInCombo_.addItem (ifaces[i], i + 1);
        if (! ifaces[i].startsWithIgnoreCase ("ALL INTERFACES"))
        {
            if (! oscAdapterItems.contains (ifaces[i]))
                oscAdapterItems.add (ifaces[i]);
            if (! networkAdapterItems.contains (ifaces[i]))
                networkAdapterItems.add (ifaces[i]);
        }
    }
    for (int i = 0; i < oscAdapterItems.size(); ++i)
        oscAdapterCombo_.addItem (oscAdapterItems[i], i + 2);
    for (int i = 0; i < networkAdapterItems.size(); ++i)
    {
        const auto id = i + 2;
        resolumeAdapterCombo_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo1_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo2_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo3_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo4_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo5_.addItem (networkAdapterItems[i], id);
    }
    if (artnetInCombo_.getNumItems() > 0)
        artnetInCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (oscAdapterCombo_.getNumItems() > 0)
        oscAdapterCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (resolumeAdapterCombo_.getNumItems() > 0)
        resolumeAdapterCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    for (auto* c : { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_ })
        if (c->getNumItems() > 0)
            c->setSelectedItemIndex (0, juce::dontSendNotification);
    syncOscIpWithAdapter();
    syncResolumeListenIpWithAdapter();
}

void TriggerContentComponent::refreshLtcDeviceListsByDriver()
{
    juce::String prevInType, prevInDev;
    juce::String prevOutType, prevOutDev;

    const int prevInId = ltcInDeviceCombo_.getSelectedId();
    const int prevInIdx = prevInId > 0 ? prevInId - 1 : -1;
    if (juce::isPositiveAndBelow (prevInIdx, filteredInputIndices_.size()))
    {
        const int realIdx = filteredInputIndices_[prevInIdx];
        if (juce::isPositiveAndBelow (realIdx, inputChoices_.size()))
        {
            prevInType = inputChoices_[realIdx].typeName;
            prevInDev = inputChoices_[realIdx].deviceName;
        }
    }

    const int prevOutId = ltcOutDeviceCombo_.getSelectedId();
    const int prevOutIdx = prevOutId > 0 ? prevOutId - 1 : -1;
    if (juce::isPositiveAndBelow (prevOutIdx, filteredOutputIndices_.size()))
    {
        const int realIdx = filteredOutputIndices_[prevOutIdx];
        if (juce::isPositiveAndBelow (realIdx, outputChoices_.size()))
        {
            prevOutType = outputChoices_[realIdx].typeName;
            prevOutDev = outputChoices_[realIdx].deviceName;
        }
    }

    filteredInputChoices_.clear();
    filteredOutputChoices_.clear();
    filteredInputIndices_.clear();
    filteredOutputIndices_.clear();
    for (int i = 0; i < inputChoices_.size(); ++i)
    {
        const auto& c = inputChoices_[i];
        if (matchesDriverFilter (ltcInDriverCombo_.getText(), c.typeName))
        {
            filteredInputChoices_.add (c);
            filteredInputIndices_.add (i);
        }
    }

    for (int i = 0; i < outputChoices_.size(); ++i)
    {
        const auto& c = outputChoices_[i];
        if (matchesDriverFilter (ltcOutDriverCombo_.getText(), c.typeName))
        {
            filteredOutputChoices_.add (c);
            filteredOutputIndices_.add (i);
        }
    }

    fillAudioCombo (ltcInDeviceCombo_, filteredInputChoices_);
    fillAudioCombo (ltcOutDeviceCombo_, filteredOutputChoices_);

    if (prevInDev.isNotEmpty())
    {
        for (int i = 0; i < filteredInputIndices_.size(); ++i)
        {
            const int realIdx = filteredInputIndices_[i];
            if (juce::isPositiveAndBelow (realIdx, inputChoices_.size())
                && inputChoices_[realIdx].typeName == prevInType
                && inputChoices_[realIdx].deviceName == prevInDev)
            {
                ltcInDeviceCombo_.setSelectedId (i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    if (prevOutDev.isNotEmpty())
    {
        for (int i = 0; i < filteredOutputIndices_.size(); ++i)
        {
            const int realIdx = filteredOutputIndices_[i];
            if (juce::isPositiveAndBelow (realIdx, outputChoices_.size())
                && outputChoices_[realIdx].typeName == prevOutType
                && outputChoices_[realIdx].deviceName == prevOutDev)
            {
                ltcOutDeviceCombo_.setSelectedId (i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    refreshLtcChannelCombos();
}

void TriggerContentComponent::refreshLtcChannelCombos()
{
    const auto refill = [] (juce::ComboBox& combo, int channelCount)
    {
        const auto previousText = combo.getText();
        fillChannelCombo (combo, channelCount);
        if (previousText.isNotEmpty())
            selectComboItemByText (combo, previousText);
    };

    int inputChannelCount = 0;
    const int inSelectedId = ltcInDeviceCombo_.getSelectedId();
    const int inIdx = inSelectedId > 0 ? inSelectedId - 1 : -1;
    if (juce::isPositiveAndBelow (inIdx, filteredInputIndices_.size()))
    {
        const int realIdx = filteredInputIndices_[inIdx];
        if (juce::isPositiveAndBelow (realIdx, inputChoices_.size()))
            inputChannelCount = inputChoices_[realIdx].channelCount;
    }
    refill (ltcInChannelCombo_, inputChannelCount);

    int outputChannelCount = 0;
    const int outSelectedId = ltcOutDeviceCombo_.getSelectedId();
    const int outIdx = outSelectedId > 0 ? outSelectedId - 1 : -1;
    if (juce::isPositiveAndBelow (outIdx, filteredOutputIndices_.size()))
    {
        const int realIdx = filteredOutputIndices_[outIdx];
        if (juce::isPositiveAndBelow (realIdx, outputChoices_.size()))
            outputChannelCount = outputChoices_[realIdx].channelCount;
    }
    refill (ltcOutChannelCombo_, outputChannelCount);
}

void TriggerContentComponent::fillAudioCombo (juce::ComboBox& combo, const juce::Array<bridge::engine::AudioChoice>& choices)
{
    combo.clear();
    if (choices.isEmpty())
    {
        combo.addItem ("(No audio devices)", kPlaceholderItemId);
        combo.setSelectedId (kPlaceholderItemId, juce::dontSendNotification);
        return;
    }

    for (int i = 0; i < choices.size(); ++i)
        combo.addItem (choices[i].displayName, i + 1);
    combo.setSelectedId (1, juce::dontSendNotification);
}

double TriggerContentComponent::comboSampleRate (const juce::ComboBox& combo)
{
    const auto text = combo.getText().trim();
    if (text.startsWithIgnoreCase ("default"))
        return 0.0;
    return juce::jmax (0.0, text.getDoubleValue());
}

int TriggerContentComponent::comboChannelIndex (const juce::ComboBox& combo)
{
    const int selectedId = combo.getSelectedId();
    if (selectedId >= 1000)
        return -2 - (selectedId - 1000);
    if (selectedId == kPlaceholderItemId)
        return 0;
    return juce::jmax (0, combo.getSelectedItemIndex());
}

void TriggerContentComponent::syncOscIpWithAdapter()
{
    const auto ip = parseBindIpFromAdapterLabel (oscAdapterCombo_.getText());
    const auto lockIp = (ip != "0.0.0.0");
    if (lockIp)
        oscIpEditor_.setText (ip, juce::dontSendNotification);
    else if (oscIpEditor_.getText().trim().isEmpty() || oscIpEditor_.getText().trim() == "127.0.0.1")
        oscIpEditor_.setText ("0.0.0.0", juce::dontSendNotification);
    oscIpEditor_.setReadOnly (lockIp);
}

void TriggerContentComponent::syncResolumeListenIpWithAdapter()
{
    const auto ip = parseBindIpFromAdapterLabel (resolumeAdapterCombo_.getText());
    const auto lockIp = (ip != "0.0.0.0");
    if (lockIp)
        resolumeListenIp_.setText (ip, juce::dontSendNotification);
    else if (resolumeListenIp_.getText().trim().isEmpty() || resolumeListenIp_.getText().trim() == "127.0.0.1")
        resolumeListenIp_.setText ("0.0.0.0", juce::dontSendNotification);
    resolumeListenIp_.setReadOnly (lockIp);
}

void TriggerContentComponent::toggleSendAdapterExpanded (int index)
{
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_ };
    ExpandCircleButton* buttons[] = { &resolumeSendExpandBtn1_, &resolumeSendExpandBtn2_, &resolumeSendExpandBtn3_, &resolumeSendExpandBtn4_, &resolumeSendExpandBtn5_ };

    if (! juce::isPositiveAndBelow (index, 5))
        return;

    *expandedStates[(size_t) index] = ! *expandedStates[(size_t) index];
    buttons[(size_t) index]->setExpanded (*expandedStates[(size_t) index]);
    updateWindowHeight();
    resized();
    repaint();
}

void TriggerContentComponent::addResolumeSendTarget()
{
    if (resolumeSendTargetCount_ >= 5)
        return;

    auto adapters = std::array<juce::ComboBox*, 5> { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_ };
    auto ips = std::array<juce::TextEditor*, 5> { &resolumeSendIp_, &resolumeSendIp2_, &resolumeSendIp3_, &resolumeSendIp4_, &resolumeSendIp5_ };
    auto ports = std::array<juce::TextEditor*, 5> { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_ };
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_ };
    const int src = juce::jmax (0, resolumeSendTargetCount_ - 1);
    adapters[(size_t) resolumeSendTargetCount_]->setText (adapters[(size_t) src]->getText(), juce::dontSendNotification);
    ips[(size_t) resolumeSendTargetCount_]->setText (ips[(size_t) src]->getText(), juce::dontSendNotification);
    ports[(size_t) resolumeSendTargetCount_]->setText (ports[(size_t) src]->getText(), juce::dontSendNotification);
    *expandedStates[(size_t) resolumeSendTargetCount_] = false;
    ++resolumeSendTargetCount_;
    resized();
    repaint();
}

void TriggerContentComponent::removeResolumeSendTarget (int targetIndex)
{
    if (targetIndex <= 0 || targetIndex >= resolumeSendTargetCount_ || targetIndex > 4)
        return;

    auto adapters = std::array<juce::ComboBox*, 5> { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_ };
    auto ips = std::array<juce::TextEditor*, 5> { &resolumeSendIp_, &resolumeSendIp2_, &resolumeSendIp3_, &resolumeSendIp4_, &resolumeSendIp5_ };
    auto ports = std::array<juce::TextEditor*, 5> { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_ };
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_ };
    for (int i = targetIndex; i < resolumeSendTargetCount_ - 1; ++i)
    {
        adapters[(size_t) i]->setText (adapters[(size_t) (i + 1)]->getText(), juce::dontSendNotification);
        ips[(size_t) i]->setText (ips[(size_t) (i + 1)]->getText(), juce::dontSendNotification);
        ports[(size_t) i]->setText (ports[(size_t) (i + 1)]->getText(), juce::dontSendNotification);
        *expandedStates[(size_t) i] = *expandedStates[(size_t) (i + 1)];
    }

    adapters[(size_t) (resolumeSendTargetCount_ - 1)]->setSelectedId (1, juce::dontSendNotification);
    ips[(size_t) (resolumeSendTargetCount_ - 1)]->setText ("127.0.0.1", juce::dontSendNotification);
    ports[(size_t) (resolumeSendTargetCount_ - 1)]->setText ("7000", juce::dontSendNotification);
    *expandedStates[(size_t) (resolumeSendTargetCount_ - 1)] = false;
    --resolumeSendTargetCount_;
    resized();
    repaint();
}

std::vector<TriggerContentComponent::ResolumeSendTarget> TriggerContentComponent::collectResolumeSendTargets() const
{
    std::vector<ResolumeSendTarget> targets;
    targets.reserve ((size_t) juce::jlimit (1, 5, resolumeSendTargetCount_));

    auto addTarget = [&targets] (const juce::ComboBox& adapterCombo, const juce::TextEditor& ipEd, const juce::TextEditor& portEd)
    {
        ResolumeSendTarget target;
        target.localBindIp = parseBindIpFromAdapterLabel (adapterCombo.getText());
        target.ip = ipEd.getText().trim();
        if (target.ip.isEmpty())
            target.ip = "127.0.0.1";
        target.port = juce::jlimit (1, 65535, portEd.getText().trim().getIntValue());
        for (const auto& existing : targets)
        {
            if (existing.localBindIp == target.localBindIp
                && existing.ip == target.ip
                && existing.port == target.port)
                return;
        }
        targets.emplace_back (target);
    };

    addTarget (resolumeSendAdapterCombo1_, resolumeSendIp_, resolumeSendPort_);
    if (resolumeSendTargetCount_ >= 2) addTarget (resolumeSendAdapterCombo2_, resolumeSendIp2_, resolumeSendPort2_);
    if (resolumeSendTargetCount_ >= 3) addTarget (resolumeSendAdapterCombo3_, resolumeSendIp3_, resolumeSendPort3_);
    if (resolumeSendTargetCount_ >= 4) addTarget (resolumeSendAdapterCombo4_, resolumeSendIp4_, resolumeSendPort4_);
    if (resolumeSendTargetCount_ >= 5) addTarget (resolumeSendAdapterCombo5_, resolumeSendIp5_, resolumeSendPort5_);
    return targets;
}

bool TriggerContentComponent::sendOscPulse (const ResolumeSendTarget& target, const juce::String& addr) const
{
    juce::DatagramSocket socket (true);
    const auto bindIp = target.localBindIp.trim().isNotEmpty() ? target.localBindIp.trim() : juce::String ("0.0.0.0");
    bool bound = false;
    if (bindIp != "0.0.0.0")
        bound = socket.bindToPort (0, bindIp);
    else
        bound = socket.bindToPort (0);
    if (! bound)
        return false;

    juce::OSCSender sender;
    if (! sender.connectToSocket (socket, target.ip, target.port))
        return false;

    juce::OSCMessage on (addr);  on.addInt32 (1);
    juce::OSCMessage off (addr); off.addInt32 (0);
    return sender.send (on) && sender.send (off);
}

bool TriggerContentComponent::parseTcToFrames (const juce::String& tc, int fps, int& outFrames)
{
    juce::StringArray p;
    p.addTokens (tc, ":", "");
    p.removeEmptyStrings();
    if (p.size() != 4)
        return false;
    const int hh = p[0].getIntValue();
    const int mm = p[1].getIntValue();
    const int ss = p[2].getIntValue();
    const int ff = p[3].getIntValue();
    if (mm < 0 || mm > 59 || ss < 0 || ss > 59 || ff < 0 || ff >= fps)
        return false;
    outFrames = (((hh * 60) + mm) * 60 + ss) * fps + ff;
    return true;
}

void TriggerContentComponent::setTimecodeStatusText (const juce::String& text, juce::Colour colour)
{
    statusLabel_.setText (text, juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::textColourId, colour);
}

void TriggerContentComponent::setResolumeStatusText (const juce::String& text, juce::Colour colour)
{
    resolumeStatusLabel_.setText (text, juce::dontSendNotification);
    resolumeStatusLabel_.setColour (juce::Label::textColourId, colour);
}

void TriggerContentComponent::openStatusMonitorWindow()
{
    // If already open, bring to front
    if (statusMonitor_ != nullptr)
    {
        statusMonitor_->toFront (true);
        return;
    }

    // Build a live getter lambda that reads current values every timer tick
    auto getter = [this] (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals)
    {
        keys.clearQuick();
        vals.clearQuick();

        keys.add ("Source:");        vals.add (sourceCombo_.getText());
        keys.add ("Input TC:");      vals.add (tcLabel_.getText()
                                               + "  (" + formatDisplayedInputFps (hasLatchedTc_, latchedFps_, fpsIndicatorStrip_.get()) + ")");
        keys.add ("TC Status:");     vals.add (statusLabel_.getText());
        keys.add ("Resolume:");      vals.add (resolumeStatusLabel_.getText());
        keys.add ("LTC Out:");       vals.add ((ltcOutSwitch_.getState() ? "ON" : "OFF")
                                               + juce::String ("  |  ") + ltcOutDeviceCombo_.getText());
        keys.add ("LTC Ch / Rate:"); vals.add (ltcOutChannelCombo_.getText()
                                               + "  |  " + ltcOutSampleRateCombo_.getText());
        keys.add ("MTC In:");        vals.add (mtcInCombo_.getText());
        keys.add ("ArtNet In:");     vals.add (artnetInCombo_.getText()
                                               + "  |  " + artnetListenIpEditor_.getText());
        keys.add ("OSC Listen:");    vals.add (oscIpEditor_.getText() + ":" + oscPortEditor_.getText());

        // Last fired clips — one per layer, from currentTriggerKeys_
        juce::String fired;
        for (const auto& key : currentTriggerKeys_)
        {
            for (const auto& t : triggerRows_)
            {
                if (t.layer == key.first && t.clip == key.second)
                {
                    if (fired.isNotEmpty()) fired += ",  ";
                    fired += "L" + juce::String (t.layer)
                           + " C" + juce::String (t.clip)
                           + " " + t.name
                           + "  ->  " + t.triggerTc;
                    break;
                }
            }
        }
        keys.add ("Last Fired:");    vals.add (fired.isEmpty() ? "-" : fired);
    };

    auto* win = new StatusMonitorWindow (std::move (getter), getParentComponent());
    statusMonitor_ = win;
}

void TriggerContentComponent::sendTestTrigger (int layer, int clip)
{
    if (layer < 1 || clip < 1)
        return;
    const auto addr = "/composition/layers/" + juce::String (layer) + "/clips/" + juce::String (clip) + "/connect";
    for (const auto& target : collectResolumeSendTargets())
        sendOscPulse (target, addr);
}

void TriggerContentComponent::rebuildDisplayRows()
{
    displayRows_.clear();
    std::map<int, std::vector<int>> byLayer;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
        byLayer[triggerRows_[(size_t) i].layer].push_back (i);

    for (auto& [layer, indices] : byLayer)
    {
        std::sort (indices.begin(), indices.end(), [this] (int a, int b)
        {
            return triggerRows_[(size_t) a].clip < triggerRows_[(size_t) b].clip;
        });

        if (layerExpanded_.find (layer) == layerExpanded_.end())
            layerExpanded_[layer] = true;
        bool anyIncluded = false;
        for (int idx : indices)
            anyIncluded = anyIncluded || triggerRows_[(size_t) idx].include;
        layerEnabled_[layer] = anyIncluded;

        displayRows_.push_back ({ true, layer, -1 });
        if (layerExpanded_[layer])
            for (int idx : indices)
                displayRows_.push_back ({ false, layer, idx });
    }
}

bool TriggerContentComponent::hasCustomGroup() const
{
    for (const auto& c : triggerRows_)
        if (c.isCustom) return true;
    return false;
}

void TriggerContentComponent::addCustomColTrigger()
{
    addCustomColumns();
    TriggerClip row;
    row.layer = 0;
    int nextClip = 1;
    for (const auto& c : triggerRows_)
        if (c.isCustom) nextClip = juce::jmax (nextClip, c.clip + 1);
    row.clip = nextClip;
    row.include = true;
    row.hasOffset = true;
    row.name = "Custom " + juce::String (nextClip);
    row.layerName = "Custom Trigger";
    row.countdownTc = "00:00:00:00";
    row.triggerRangeSec = 5.0;
    row.durationTc = "00:00:00:00";
    row.triggerTc = "00:00:00:00";
    row.endActionMode = "off";
    row.isCustom = true;
    row.customType = "col";
    row.customSourceCol = "1";
    triggerRows_.push_back (row);
    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::addCustomLcTrigger()
{
    addCustomColumns();
    TriggerClip row;
    row.layer = 0;
    int nextClip = 1;
    for (const auto& c : triggerRows_)
        if (c.isCustom) nextClip = juce::jmax (nextClip, c.clip + 1);
    row.clip = nextClip;
    row.include = true;
    row.hasOffset = true;
    row.name = "Layer/Clip";
    row.layerName = "Custom";
    row.countdownTc = "00:00:00:00";
    row.triggerRangeSec = 5.0;
    row.durationTc = "00:00:00:00";
    row.triggerTc = "00:00:00:00";
    row.endActionMode = "off";
    row.isCustom = true;
    row.customType = "lc";
    row.customSourceLayer = "1";
    row.customSourceClip = "1";
    triggerRows_.push_back (row);
    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::deleteCustomTrigger (int clipIndex)
{
    if (! juce::isPositiveAndBelow (clipIndex, (int) triggerRows_.size()))
        return;
    if (! triggerRows_[(size_t) clipIndex].isCustom)
        return;
    const auto& c = triggerRows_[(size_t) clipIndex];
    currentTriggerKeys_.erase ({ c.layer, c.clip });
    pendingEndActions_.erase ({ c.layer, c.clip });
    triggerRangeActive_.erase ({ c.layer, c.clip });
    triggerRows_.erase (triggerRows_.begin() + clipIndex);
    rebuildDisplayRows();
    // updateContent WHILE custom cols still in header so JUCE clears stale components
    triggerTable_.updateContent();
    // If the last custom trigger was removed, clean up the custom group completely
    if (! hasCustomGroup())
    {
        layerExpanded_.erase (0);
        layerEnabled_.erase (0);
        removeCustomColumns(); // resizes window and removes cols 10/11 from header
    }
    triggerTable_.repaint();
    createCustomBtn_.setEnabled (! hasCustomGroup());
}

void TriggerContentComponent::deleteCustomGroup()
{
    for (auto it = triggerRows_.begin(); it != triggerRows_.end();)
        it = it->isCustom ? triggerRows_.erase (it) : ++it;
    for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        it = (it->first == 0) ? currentTriggerKeys_.erase (it) : ++it;
    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
        it = (it->first.first == 0) ? pendingEndActions_.erase (it) : ++it;
    for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
        it = (it->first.first == 0) ? triggerRangeActive_.erase (it) : ++it;
    layerExpanded_.erase (0);
    layerEnabled_.erase (0);

    // Rebuild display first so refreshComponentForCell sees no custom rows.
    rebuildDisplayRows();

    // Call updateContent WHILE cols 10-12 still exist: JUCE calls
    // refreshComponentForCell for those cols on every (now-regular) row,
    // gets nullptr back, and removes the stale custom-row components.
    triggerTable_.updateContent();

    // Now remove the columns and resize the window.
    removeCustomColumns();

    triggerTable_.repaint();
    createCustomBtn_.setEnabled (true);
}

void TriggerContentComponent::fireCustomTrigger (const TriggerClip& clip)
{
    const auto targets = collectResolumeSendTargets();

    juce::String addr;
    if (clip.customType == "col")
    {
        const int col = clip.customSourceCol.getIntValue();
        if (col > 0)
            addr = "/composition/columns/" + juce::String (col) + "/connect";
    }
    else if (clip.customType == "lc")
    {
        const int lay = clip.customSourceLayer.getIntValue();
        const int clp = clip.customSourceClip.getIntValue();
        if (lay > 0 && clp > 0)
            addr = "/composition/layers/" + juce::String (lay)
                   + "/clips/" + juce::String (clp) + "/connect";
    }

    if (addr.isEmpty())
        return;

    for (const auto& target : targets)
        sendOscPulse (target, addr);
}

void TriggerContentComponent::openGetClipsOptions()
{
    new GetClipsOptionsWindow (includeClipsWithOffset_,
                               includeClipsWithoutOffset_,
                               [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (bool includeWithOffset, bool includeWithoutOffset)
                               {
                                   if (safe != nullptr)
                                       safe->queryResolume (includeWithOffset, includeWithoutOffset);
                               },
                               getParentComponent());
}

void TriggerContentComponent::queryResolume (bool includeClipsWithOffset, bool includeClipsWithoutOffset)
{
    juce::String err;
    includeClipsWithOffset_ = includeClipsWithOffset;
    includeClipsWithoutOffset_ = includeClipsWithoutOffset;
    clipReceiveEnabled_ = true;
    clipCollector_.clear();
    const auto listenIp = resolumeListenIp_.getText().trim().isNotEmpty() ? resolumeListenIp_.getText().trim() : "127.0.0.1";
    if (! clipCollector_.startListening (listenIp, juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()), err))
    {
        setResolumeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        return;
    }
    auto targets = collectResolumeSendTargets();
    if (targets.empty())
        targets.push_back ({ "0.0.0.0", "127.0.0.1", 7000 });
    if (! clipCollector_.configureSender (targets.front().localBindIp, targets.front().ip, targets.front().port, err))
    {
        setResolumeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        return;
    }
    const int maxLayers = juce::jlimit (1, 64, resolumeMaxLayers_.getText().getIntValue());
    const int maxClips = juce::jlimit (1, 256, resolumeMaxClips_.getText().getIntValue());
    clipCollector_.queryClips (maxLayers, maxClips);
    // macOS may occasionally drop part of a large UDP query burst.
    // Send two delayed retries to make clip collection deterministic.
    for (int i = 1; i <= 2; ++i)
    {
        juce::Timer::callAfterDelay (160 * i,
                                     [safe = juce::Component::SafePointer<TriggerContentComponent> (this), maxLayers, maxClips]
                                     {
                                         if (safe == nullptr || ! safe->clipReceiveEnabled_)
                                             return;
                                         safe->clipCollector_.queryClips (maxLayers, maxClips);
                                     });
    }
    queryPending_ = true;
    queryStartMs_ = juce::Time::currentTimeMillis();
    juce::String modeText;
    if (includeClipsWithOffset_ && includeClipsWithoutOffset_)
        modeText = "all clips";
    else if (includeClipsWithOffset_)
        modeText = "clips with offset";
    else if (includeClipsWithoutOffset_)
        modeText = "clips without offset";
    else
        modeText = "no clips";

    setResolumeStatusText ("Resolume query sent: " + modeText,
                           juce::Colour::fromRGB (0x51, 0xc8, 0x7b));
}

void TriggerContentComponent::resetSettings()
{
    // Stop active engines before resetting
    bridgeEngine_.stopLtcThru();
    bridgeEngine_.stopLtcOutput();
    bridgeEngine_.stopLtcInput();

    // Source
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceExpanded_    = true;
    triggerOutExpanded_ = false;
    outLtcExpanded_    = false;
    resolumeExpanded_  = false;
    sourceExpandBtn_.setExpanded   (sourceExpanded_);
    triggerOutExpandBtn_.setExpanded (triggerOutExpanded_);
    outLtcExpandBtn_.setExpanded   (outLtcExpanded_);
    resolumeExpandBtn_.setExpanded (resolumeExpanded_);

    // LTC In
    ltcInDriverCombo_.setSelectedId  (1, juce::dontSendNotification);
    ltcOutDriverCombo_.setSelectedId (1, juce::dontSendNotification);
    refreshLtcDeviceListsByDriver();
    ltcInDeviceCombo_.setSelectedId  (0, juce::dontSendNotification);
    ltcInChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcInSampleRateCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcInGainSlider_.setValue (0.0, juce::dontSendNotification);

    // LTC Out
    ltcOutDeviceCombo_.setSelectedId (0, juce::dontSendNotification);
    ltcOutChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutSampleRateCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOffsetEditor_.setText  ("00:00:00:00", juce::dontSendNotification);
    ltcOutLevelSlider_.setValue (0.0, juce::dontSendNotification);
    ltcOutSwitch_.setState (false);
    ltcThruDot_.setState   (false);

    // MTC / ArtNet / OSC
    mtcInCombo_.setSelectedId      (0, juce::dontSendNotification);
    artnetInCombo_.setSelectedId   (0, juce::dontSendNotification);
    artnetListenIpEditor_.setText  ("0.0.0.0",  juce::dontSendNotification);
    oscIpEditor_.setText           ("0.0.0.0",  juce::dontSendNotification);
    oscPortEditor_.setText         ("9000",      juce::dontSendNotification);
    // OSC addresses: restore the same defaults as at startup
    oscAddrStrEditor_.setText      ("/frames/str", juce::dontSendNotification);
    oscAddrFloatEditor_.setText    ("/time",       juce::dontSendNotification);
    oscFloatTypeCombo_.setSelectedId (1,         juce::dontSendNotification);
    oscFloatMaxEditor_.setText     ("3600",      juce::dontSendNotification);

    // Resolume
    resolumeAdapterCombo_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo1_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo2_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo3_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo4_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo5_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendIp_.setText        ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort_.setText      ("7000",       juce::dontSendNotification);
    resolumeSendTargetCount_ = 1;
    resolumeSendExpanded1_ = true;
    resolumeSendExpanded2_ = false;
    resolumeSendExpanded3_ = false;
    resolumeSendExpanded4_ = false;
    resolumeSendExpanded5_ = false;
    resolumeSendExpandBtn1_.setExpanded (resolumeSendExpanded1_);
    resolumeSendExpandBtn2_.setExpanded (resolumeSendExpanded2_);
    resolumeSendExpandBtn3_.setExpanded (resolumeSendExpanded3_);
    resolumeSendExpandBtn4_.setExpanded (resolumeSendExpanded4_);
    resolumeSendExpandBtn5_.setExpanded (resolumeSendExpanded5_);
    resolumeSendIp2_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort2_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp3_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort3_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp4_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort4_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp5_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort5_.setText     ("7000",       juce::dontSendNotification);
    resolumeListenIp_.setText      ("127.0.0.1", juce::dontSendNotification);
    resolumeListenPort_.setText    ("7001",       juce::dontSendNotification);
    resolumeMaxLayers_.setText     ("4",          juce::dontSendNotification);
    resolumeMaxClips_.setText      ("32",         juce::dontSendNotification);
    resolumeGlobalOffset_.setText  ("00:00:00:00", juce::dontSendNotification);
    syncResolumeListenIpWithAdapter();

    updateWindowHeight();
    resized();
    repaint();
    juce::Timer::callAfterDelay (50, [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
    {
        if (auto* self = safe.getComponent())
        {
            self->onInputSettingsChanged();
            self->onOutputSettingsChanged();
        }
    });
    setTimecodeStatusText ("Settings reset to defaults", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
}

void TriggerContentComponent::openSettingsMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save settings...");
    m.addItem (2, "Save clips...");
    m.addItem (3, "Save all...");
    m.addSeparator();
    m.addItem (4, "Load settings...");
    m.addItem (5, "Load clips...");
    m.addItem (6, "Load all...");
    m.addSeparator();
    m.addItem (7, "Rescan audio devices");
    m.addSeparator();
    m.addItem (10, "Clear custom triggers", hasCustomGroup());
    m.addItem (11, "Clear clip triggers", ! triggerRows_.empty());
    m.addItem (12, "Reset settings");
    m.addSeparator();
    m.addItem (8, "Load on startup", true, autoLoadOnStartup_);
    m.addItem (9, "Close to tray", true, closeToTray_);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton_),
                     [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (int result)
                     {
                         if (safe == nullptr)
                             return;

                         switch (result)
                         {
                             case 1: safe->saveConfigAs (kConfigModeSettings); break;
                             case 2: safe->saveConfigAs (kConfigModeClips); break;
                             case 3: safe->saveConfigAs (kConfigModeAll); break;
                             case 4: safe->loadConfigFrom (kConfigModeSettings); break;
                             case 5: safe->loadConfigFrom (kConfigModeClips); break;
                             case 6: safe->loadConfigFrom (kConfigModeAll); break;
                             case 7:
                                 safe->startAudioDeviceScan();
                                 safe->setTimecodeStatusText ("Audio devices rescanned", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                 break;
                             case 10:
                                 if (safe->hasCustomGroup())
                                 {
                                     safe->deleteCustomGroup();
                                     safe->setTimecodeStatusText ("Custom triggers cleared", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                 }
                                 break;
                             case 11:
                             {
                                 // Stop receiving Resolume OSC updates so custom triggers
                                 // cannot cause refreshTriggerRows() to recreate clips.
                                 safe->clipReceiveEnabled_ = false;
                                 safe->clipCollector_.clear();
                                 // Remove all non-custom clips and their state
                                 for (auto it = safe->triggerRows_.begin(); it != safe->triggerRows_.end();)
                                     it = (! it->isCustom) ? safe->triggerRows_.erase (it) : ++it;
                                 for (auto it = safe->currentTriggerKeys_.begin(); it != safe->currentTriggerKeys_.end();)
                                     it = (it->first != 0) ? safe->currentTriggerKeys_.erase (it) : ++it;
                                 for (auto it = safe->pendingEndActions_.begin(); it != safe->pendingEndActions_.end();)
                                     it = (it->first.first != 0) ? safe->pendingEndActions_.erase (it) : ++it;
                                 for (auto it = safe->triggerRangeActive_.begin(); it != safe->triggerRangeActive_.end();)
                                     it = (it->first.first != 0) ? safe->triggerRangeActive_.erase (it) : ++it;
                                 safe->rebuildDisplayRows();
                                 safe->triggerTable_.updateContent();
                                 safe->triggerTable_.repaint();
                                 safe->setTimecodeStatusText ("Clip triggers cleared", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                 break;
                             }
                             case 12:
                                 safe->resetSettings();
                                 break;
                             case 8:
                                 safe->autoLoadOnStartup_ = ! safe->autoLoadOnStartup_;
                                 safe->saveRuntimePrefs();
                                 safe->setTimecodeStatusText (safe->autoLoadOnStartup_ ? "Load on startup ON" : "Load on startup OFF",
                                                              juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                 break;
                             case 9:
                                 safe->closeToTray_ = ! safe->closeToTray_;
                                 safe->saveRuntimePrefs();
                                 safe->setTimecodeStatusText (safe->closeToTray_ ? "Close to tray ON" : "Close to tray OFF",
                                                              juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                 break;
                             default:
                                 break;
                         }
                     });
}

void TriggerContentComponent::loadRuntimePrefs()
{
    if (auto prefs = bridge::core::ConfigStore::loadJsonFile (getRuntimePrefsFile()))
    {
        if (auto* obj = prefs->getDynamicObject())
        {
            if (obj->hasProperty ("close_to_tray"))
                closeToTray_ = (bool) obj->getProperty ("close_to_tray");
            if (obj->hasProperty ("auto_load_on_startup"))
                autoLoadOnStartup_ = (bool) obj->getProperty ("auto_load_on_startup");
            if (obj->hasProperty ("last_config_path"))
            {
                const auto path = obj->getProperty ("last_config_path").toString();
                if (path.isNotEmpty())
                    lastConfigFile_ = juce::File (path);
            }
        }
    }

    pendingAutoLoad_ = autoLoadOnStartup_ && lastConfigFile_.existsAsFile();
}

void TriggerContentComponent::saveRuntimePrefs() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("close_to_tray", closeToTray_);
    obj->setProperty ("auto_load_on_startup", autoLoadOnStartup_);
    obj->setProperty ("last_config_path", lastConfigFile_.getFullPathName());
    bridge::core::ConfigStore::saveJsonFile (getRuntimePrefsFile(), juce::var (obj));
}

void TriggerContentComponent::maybeAutoLoadConfig()
{
    if (pendingAutoLoad_ && lastConfigFile_.existsAsFile())
    {
        pendingAutoLoad_ = false;
        loadConfigFromFile (lastConfigFile_, kConfigModeAll);
    }
}

void TriggerContentComponent::saveConfigAs (int modeId)
{
    const juce::String label = modeId == kConfigModeSettings ? "settings"
                             : modeId == kConfigModeClips ? "clips"
                                                          : "all";

    saveChooser_ = std::make_unique<juce::FileChooser> (
        "Save Easy Trigger " + label,
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("easy_trigger_" + label + ".etr"),
        "*.etr");

    saveChooser_->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [safe = juce::Component::SafePointer<TriggerContentComponent> (this), modeId] (const juce::FileChooser& chooser)
                               {
                                   if (safe == nullptr)
                                       return;
                                   auto file = chooser.getResult();
                                   if (file == juce::File{})
                                       return;
                                   if (! file.hasFileExtension (".etr"))
                                       file = file.withFileExtension (".etr");
                                   safe->saveConfigToFile (file, modeId);
                                   safe->saveChooser_.reset();
                               });
}

void TriggerContentComponent::loadConfigFrom (int modeId)
{
    loadChooser_ = std::make_unique<juce::FileChooser> (
        "Load Easy Trigger config",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.etr");

    loadChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safe = juce::Component::SafePointer<TriggerContentComponent> (this), modeId] (const juce::FileChooser& chooser)
                               {
                                   if (safe == nullptr)
                                       return;
                                   auto file = chooser.getResult();
                                   if (file != juce::File{})
                                       safe->loadConfigFromFile (file, modeId);
                                   safe->loadChooser_.reset();
                               });
}

void TriggerContentComponent::saveConfigToFile (const juce::File& file, int modeId)
{
    auto* rootObj = new juce::DynamicObject();
    rootObj->setProperty ("app", "Easy Trigger");
    rootObj->setProperty ("format", "etr");
    rootObj->setProperty ("mode", configModeKey (modeId));
    rootObj->setProperty ("version", bridge::version::kAppVersion);

    if (modeId == kConfigModeSettings || modeId == kConfigModeAll)
    {
        auto* leftObj = new juce::DynamicObject();
        leftObj->setProperty ("source", sourceCombo_.getSelectedId());
        leftObj->setProperty ("source_expanded", sourceExpanded_);
        leftObj->setProperty ("trigger_out_expanded", triggerOutExpanded_);
        leftObj->setProperty ("out_ltc_expanded", outLtcExpanded_);
        leftObj->setProperty ("resolume_expanded", resolumeExpanded_);
        leftObj->setProperty ("ltc_in_driver", ltcInDriverCombo_.getText());
        leftObj->setProperty ("ltc_in_device", ltcInDeviceCombo_.getText());
        leftObj->setProperty ("ltc_in_channel", ltcInChannelCombo_.getText());
        leftObj->setProperty ("ltc_in_rate", ltcInSampleRateCombo_.getText());
        leftObj->setProperty ("ltc_in_gain", ltcInGainSlider_.getValue());
        leftObj->setProperty ("mtc_in", mtcInCombo_.getText());
        leftObj->setProperty ("artnet_in", artnetInCombo_.getText());
        leftObj->setProperty ("artnet_listen_ip", artnetListenIpEditor_.getText());
        leftObj->setProperty ("osc_adapter", oscAdapterCombo_.getText());
        leftObj->setProperty ("osc_ip", oscIpEditor_.getText());
        leftObj->setProperty ("osc_port", oscPortEditor_.getText());
        leftObj->setProperty ("osc_fps", oscFpsCombo_.getText());
        leftObj->setProperty ("osc_str", oscAddrStrEditor_.getText());
        leftObj->setProperty ("osc_float", oscAddrFloatEditor_.getText());
        leftObj->setProperty ("osc_float_type", oscFloatTypeCombo_.getSelectedId());
        leftObj->setProperty ("osc_float_max", oscFloatMaxEditor_.getText());
        leftObj->setProperty ("ltc_out_driver", ltcOutDriverCombo_.getText());
        leftObj->setProperty ("ltc_out_device", ltcOutDeviceCombo_.getText());
        leftObj->setProperty ("ltc_out_channel", ltcOutChannelCombo_.getText());
        leftObj->setProperty ("ltc_out_rate", ltcOutSampleRateCombo_.getText());
        leftObj->setProperty ("ltc_out_convert_fps", ltcConvertStrip_.getSelectedFps().has_value() ? frameRateToString (*ltcConvertStrip_.getSelectedFps()) : juce::String());
        leftObj->setProperty ("ltc_out_offset", ltcOffsetEditor_.getText());
        leftObj->setProperty ("ltc_out_level", ltcOutLevelSlider_.getValue());
        leftObj->setProperty ("ltc_out_enabled", ltcOutSwitch_.getState());
        leftObj->setProperty ("ltc_out_thru", ltcThruDot_.getState());
        leftObj->setProperty ("res_adapter", resolumeAdapterCombo_.getText());
        leftObj->setProperty ("res_send_adapter_1", resolumeSendAdapterCombo1_.getText());
        leftObj->setProperty ("res_send_ip", resolumeSendIp_.getText());
        leftObj->setProperty ("res_send_port", resolumeSendPort_.getText());
        leftObj->setProperty ("res_send_target_count", resolumeSendTargetCount_);
        leftObj->setProperty ("res_send_expanded_1", resolumeSendExpanded1_);
        leftObj->setProperty ("res_send_adapter_2", resolumeSendAdapterCombo2_.getText());
        leftObj->setProperty ("res_send_ip_2", resolumeSendIp2_.getText());
        leftObj->setProperty ("res_send_port_2", resolumeSendPort2_.getText());
        leftObj->setProperty ("res_send_expanded_2", resolumeSendExpanded2_);
        leftObj->setProperty ("res_send_adapter_3", resolumeSendAdapterCombo3_.getText());
        leftObj->setProperty ("res_send_ip_3", resolumeSendIp3_.getText());
        leftObj->setProperty ("res_send_port_3", resolumeSendPort3_.getText());
        leftObj->setProperty ("res_send_expanded_3", resolumeSendExpanded3_);
        leftObj->setProperty ("res_send_adapter_4", resolumeSendAdapterCombo4_.getText());
        leftObj->setProperty ("res_send_ip_4", resolumeSendIp4_.getText());
        leftObj->setProperty ("res_send_port_4", resolumeSendPort4_.getText());
        leftObj->setProperty ("res_send_expanded_4", resolumeSendExpanded4_);
        leftObj->setProperty ("res_send_adapter_5", resolumeSendAdapterCombo5_.getText());
        leftObj->setProperty ("res_send_ip_5", resolumeSendIp5_.getText());
        leftObj->setProperty ("res_send_port_5", resolumeSendPort5_.getText());
        leftObj->setProperty ("res_send_expanded_5", resolumeSendExpanded5_);
        leftObj->setProperty ("res_listen_ip", resolumeListenIp_.getText());
        leftObj->setProperty ("res_listen_port", resolumeListenPort_.getText());
        leftObj->setProperty ("res_max_layers", resolumeMaxLayers_.getText());
        leftObj->setProperty ("res_max_clips", resolumeMaxClips_.getText());
        leftObj->setProperty ("res_global_offset", resolumeGlobalOffset_.getText());
        rootObj->setProperty ("left", juce::var (leftObj));
    }

    if (modeId == kConfigModeClips || modeId == kConfigModeAll)
    {
        juce::Array<juce::var> rows;
        for (const auto& clip : triggerRows_)
        {
            auto* rowObj = new juce::DynamicObject();
            rowObj->setProperty ("layer", clip.layer);
            rowObj->setProperty ("clip", clip.clip);
            rowObj->setProperty ("include", clip.include);
            rowObj->setProperty ("has_offset", clip.hasOffset);
            rowObj->setProperty ("name", clip.name);
            rowObj->setProperty ("layer_name", clip.layerName);
            rowObj->setProperty ("trigger_range_sec", clip.triggerRangeSec);
            rowObj->setProperty ("duration_tc", clip.durationTc);
            rowObj->setProperty ("trigger_tc", clip.triggerTc);
            rowObj->setProperty ("end_action_mode", clip.endActionMode);
            rowObj->setProperty ("end_action_col", clip.endActionCol);
            rowObj->setProperty ("end_action_layer", clip.endActionLayer);
            rowObj->setProperty ("end_action_clip", clip.endActionClip);
            rowObj->setProperty ("is_custom", clip.isCustom);
            if (clip.isCustom)
            {
                rowObj->setProperty ("custom_type", clip.customType);
                rowObj->setProperty ("custom_source_col", clip.customSourceCol);
                rowObj->setProperty ("custom_source_layer", clip.customSourceLayer);
                rowObj->setProperty ("custom_source_clip", clip.customSourceClip);
            }
            rows.add (juce::var (rowObj));
        }
        rootObj->setProperty ("clips", juce::var (rows));
        if (hasCustomGroup())
            rootObj->setProperty ("custom_group_name", customGroupName_);
    }

    if (bridge::core::ConfigStore::saveJsonFile (file, juce::var (rootObj)))
    {
        lastConfigFile_ = file;
        saveRuntimePrefs();
        setTimecodeStatusText ("Config saved: " + file.getFileName(), juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    }
    else
    {
        setTimecodeStatusText ("Failed to save config", juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
    }
}

void TriggerContentComponent::loadConfigFromFile (const juce::File& file, int modeId)
{
    auto parsed = bridge::core::ConfigStore::loadJsonFile (file);
    if (! parsed.has_value() || ! parsed->isObject())
    {
        setTimecodeStatusText ("Invalid config", juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        return;
    }

    auto* rootObj = parsed->getDynamicObject();
    if (rootObj == nullptr)
        return;

    auto setComboText = [] (juce::ComboBox& combo, const juce::String& text)
    {
        for (int i = 0; i < combo.getNumItems(); ++i)
        {
            if (combo.getItemText (i) == text)
            {
                combo.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
        }
    };

    if ((modeId == kConfigModeSettings || modeId == kConfigModeAll) && rootObj->hasProperty ("left"))
    {
        if (auto* leftObj = rootObj->getProperty ("left").getDynamicObject())
        {
            sourceCombo_.setSelectedId ((int) leftObj->getProperty ("source"), juce::dontSendNotification);
            sourceExpanded_ = (bool) leftObj->getProperty ("source_expanded");
            triggerOutExpanded_ = leftObj->hasProperty ("trigger_out_expanded") ? (bool) leftObj->getProperty ("trigger_out_expanded") : false;
            outLtcExpanded_ = (bool) leftObj->getProperty ("out_ltc_expanded");
            resolumeExpanded_ = (bool) leftObj->getProperty ("resolume_expanded");

            setComboText (ltcInDriverCombo_, leftObj->getProperty ("ltc_in_driver").toString());
            setComboText (ltcOutDriverCombo_, leftObj->getProperty ("ltc_out_driver").toString());
            refreshLtcDeviceListsByDriver();
            setComboText (ltcInDeviceCombo_, leftObj->getProperty ("ltc_in_device").toString());
            refreshLtcChannelCombos();
            setComboText (ltcInChannelCombo_, leftObj->getProperty ("ltc_in_channel").toString());
            setComboText (ltcInSampleRateCombo_, leftObj->getProperty ("ltc_in_rate").toString());
            ltcInGainSlider_.setValue ((double) leftObj->getProperty ("ltc_in_gain"), juce::dontSendNotification);
            setComboText (mtcInCombo_, leftObj->getProperty ("mtc_in").toString());
            setComboText (artnetInCombo_, leftObj->getProperty ("artnet_in").toString());
            artnetListenIpEditor_.setText (leftObj->getProperty ("artnet_listen_ip").toString(), juce::dontSendNotification);
            setComboText (oscAdapterCombo_, leftObj->getProperty ("osc_adapter").toString());
            oscIpEditor_.setText (leftObj->getProperty ("osc_ip").toString(), juce::dontSendNotification);
            oscPortEditor_.setText (leftObj->getProperty ("osc_port").toString(), juce::dontSendNotification);
            setComboText (oscFpsCombo_, leftObj->getProperty ("osc_fps").toString());
            oscAddrStrEditor_.setText (leftObj->getProperty ("osc_str").toString(), juce::dontSendNotification);
            oscAddrFloatEditor_.setText (leftObj->getProperty ("osc_float").toString(), juce::dontSendNotification);
            if (leftObj->hasProperty ("osc_float_type"))
                oscFloatTypeCombo_.setSelectedId ((int) leftObj->getProperty ("osc_float_type"), juce::dontSendNotification);
            if (leftObj->hasProperty ("osc_float_max"))
                oscFloatMaxEditor_.setText (leftObj->getProperty ("osc_float_max").toString(), juce::dontSendNotification);

            setComboText (ltcOutDeviceCombo_, leftObj->getProperty ("ltc_out_device").toString());
            refreshLtcChannelCombos();
            setComboText (ltcOutChannelCombo_, leftObj->getProperty ("ltc_out_channel").toString());
            setComboText (ltcOutSampleRateCombo_, leftObj->getProperty ("ltc_out_rate").toString());
            if (leftObj->hasProperty ("ltc_out_convert_fps"))
            {
                auto fps = fpsFromString (leftObj->getProperty ("ltc_out_convert_fps").toString());
                ltcConvertStrip_.setSelectedFps (fps);
                bridgeEngine_.setLtcOutputConvertFps (fps);
            }
            ltcOffsetEditor_.setText (leftObj->getProperty ("ltc_out_offset").toString(), juce::dontSendNotification);
            ltcOutLevelSlider_.setValue ((double) leftObj->getProperty ("ltc_out_level"), juce::dontSendNotification);
            ltcOutSwitch_.setState ((bool) leftObj->getProperty ("ltc_out_enabled"));
            ltcThruDot_.setState ((bool) leftObj->getProperty ("ltc_out_thru"));

            setComboText (resolumeAdapterCombo_, leftObj->hasProperty ("res_adapter") ? leftObj->getProperty ("res_adapter").toString()
                                                                                      : juce::String ("ALL INTERFACES (0.0.0.0)"));
            setComboText (resolumeSendAdapterCombo1_, leftObj->hasProperty ("res_send_adapter_1") ? leftObj->getProperty ("res_send_adapter_1").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendIp_.setText (leftObj->getProperty ("res_send_ip").toString(), juce::dontSendNotification);
            resolumeSendPort_.setText (leftObj->getProperty ("res_send_port").toString(), juce::dontSendNotification);
            resolumeSendTargetCount_ = juce::jlimit (1, 5, (int) leftObj->getProperty ("res_send_target_count"));
            if (! leftObj->hasProperty ("res_send_target_count"))
                resolumeSendTargetCount_ = 1;
            resolumeSendExpanded1_ = leftObj->hasProperty ("res_send_expanded_1") ? (bool) leftObj->getProperty ("res_send_expanded_1") : true;
            resolumeSendIp2_.setText (leftObj->hasProperty ("res_send_ip_2") ? leftObj->getProperty ("res_send_ip_2").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort2_.setText (leftObj->hasProperty ("res_send_port_2") ? leftObj->getProperty ("res_send_port_2").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo2_, leftObj->hasProperty ("res_send_adapter_2") ? leftObj->getProperty ("res_send_adapter_2").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded2_ = leftObj->hasProperty ("res_send_expanded_2") ? (bool) leftObj->getProperty ("res_send_expanded_2") : false;
            resolumeSendIp3_.setText (leftObj->hasProperty ("res_send_ip_3") ? leftObj->getProperty ("res_send_ip_3").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort3_.setText (leftObj->hasProperty ("res_send_port_3") ? leftObj->getProperty ("res_send_port_3").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo3_, leftObj->hasProperty ("res_send_adapter_3") ? leftObj->getProperty ("res_send_adapter_3").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded3_ = leftObj->hasProperty ("res_send_expanded_3") ? (bool) leftObj->getProperty ("res_send_expanded_3") : false;
            resolumeSendIp4_.setText (leftObj->hasProperty ("res_send_ip_4") ? leftObj->getProperty ("res_send_ip_4").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort4_.setText (leftObj->hasProperty ("res_send_port_4") ? leftObj->getProperty ("res_send_port_4").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo4_, leftObj->hasProperty ("res_send_adapter_4") ? leftObj->getProperty ("res_send_adapter_4").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded4_ = leftObj->hasProperty ("res_send_expanded_4") ? (bool) leftObj->getProperty ("res_send_expanded_4") : false;
            resolumeSendIp5_.setText (leftObj->hasProperty ("res_send_ip_5") ? leftObj->getProperty ("res_send_ip_5").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort5_.setText (leftObj->hasProperty ("res_send_port_5") ? leftObj->getProperty ("res_send_port_5").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo5_, leftObj->hasProperty ("res_send_adapter_5") ? leftObj->getProperty ("res_send_adapter_5").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded5_ = leftObj->hasProperty ("res_send_expanded_5") ? (bool) leftObj->getProperty ("res_send_expanded_5") : false;
            resolumeListenIp_.setText (leftObj->getProperty ("res_listen_ip").toString(), juce::dontSendNotification);
            resolumeListenPort_.setText (leftObj->getProperty ("res_listen_port").toString(), juce::dontSendNotification);
            resolumeMaxLayers_.setText (leftObj->getProperty ("res_max_layers").toString(), juce::dontSendNotification);
            resolumeMaxClips_.setText (leftObj->getProperty ("res_max_clips").toString(), juce::dontSendNotification);
            resolumeGlobalOffset_.setText (leftObj->hasProperty ("res_global_offset")
                                               ? leftObj->getProperty ("res_global_offset").toString()
                                               : juce::String ("00:00:00:00"),
                                           juce::dontSendNotification);

            sourceExpandBtn_.setExpanded (sourceExpanded_);
            triggerOutExpandBtn_.setExpanded (triggerOutExpanded_);
            outLtcExpandBtn_.setExpanded (outLtcExpanded_);
            resolumeExpandBtn_.setExpanded (resolumeExpanded_);
            resolumeSendExpandBtn1_.setExpanded (resolumeSendExpanded1_);
            resolumeSendExpandBtn2_.setExpanded (resolumeSendExpanded2_);
            resolumeSendExpandBtn3_.setExpanded (resolumeSendExpanded3_);
            resolumeSendExpandBtn4_.setExpanded (resolumeSendExpanded4_);
            resolumeSendExpandBtn5_.setExpanded (resolumeSendExpanded5_);
            syncResolumeListenIpWithAdapter();
            // Update layout and paint immediately so the UI reflects loaded state
            // before audio device init blocks the message thread.
            updateWindowHeight();
            resized();
            repaint();
            juce::Timer::callAfterDelay (50, [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
            {
                if (auto* self = safe.getComponent())
                {
                    self->onInputSettingsChanged();
                    self->onOutputSettingsChanged();
                }
            });
        }
    }

    if ((modeId == kConfigModeClips || modeId == kConfigModeAll) && rootObj->hasProperty ("clips"))
    {
        triggerRows_.clear();
        if (auto* arr = rootObj->getProperty ("clips").getArray())
        {
            for (const auto& entry : *arr)
            {
                if (auto* rowObj = entry.getDynamicObject())
                {
                    TriggerClip clip;
                    clip.layer = (int) rowObj->getProperty ("layer");
                    clip.clip = (int) rowObj->getProperty ("clip");
                    clip.include = (bool) rowObj->getProperty ("include");
                    clip.name = rowObj->getProperty ("name").toString();
                    clip.layerName = rowObj->getProperty ("layer_name").toString();
                    clip.triggerRangeSec = (double) rowObj->getProperty ("trigger_range_sec");
                    clip.durationTc = rowObj->getProperty ("duration_tc").toString();
                    clip.triggerTc = rowObj->getProperty ("trigger_tc").toString();
                    clip.endActionMode = rowObj->getProperty ("end_action_mode").toString();
                    clip.endActionCol = rowObj->getProperty ("end_action_col").toString();
                    clip.endActionLayer = rowObj->getProperty ("end_action_layer").toString();
                    clip.endActionClip = rowObj->getProperty ("end_action_clip").toString();
                    clip.isCustom = rowObj->hasProperty ("is_custom") && (bool) rowObj->getProperty ("is_custom");
                    clip.hasOffset = rowObj->hasProperty ("has_offset")
                        ? (bool) rowObj->getProperty ("has_offset")
                        : clip.isCustom;
                    if (clip.isCustom)
                        clip.hasOffset = true;
                    if (clip.isCustom)
                    {
                        clip.customType = rowObj->getProperty ("custom_type").toString();
                        if (clip.customType.isEmpty()) clip.customType = "col";
                        clip.customSourceCol   = rowObj->getProperty ("custom_source_col").toString();
                        clip.customSourceLayer = rowObj->getProperty ("custom_source_layer").toString();
                        clip.customSourceClip  = rowObj->getProperty ("custom_source_clip").toString();
                    }
                    triggerRows_.push_back (clip);
                }
            }
        }
        if (hasCustomGroup())
        {
            addCustomColumns();
            customGroupName_ = rootObj->hasProperty ("custom_group_name")
                                   ? rootObj->getProperty ("custom_group_name").toString()
                                   : juce::String ("Custom Trigger");
        }
        rebuildDisplayRows();
        triggerTable_.updateContent();
        triggerTable_.repaint();
        createCustomBtn_.setEnabled (! hasCustomGroup());
    }

    updateWindowHeight();
    resized();
    repaint();
    lastConfigFile_ = file;
    saveRuntimePrefs();
    setTimecodeStatusText ("Config loaded: " + file.getFileName(), juce::Colour::fromRGB (0xec, 0x48, 0x3c));
}

juce::String TriggerContentComponent::secondsToTc (double sec, FrameRate fps)
{
    const int nominal = juce::jmax (1, frameRateToInt (fps));
    int totalFrames = (int) juce::roundToInt (juce::jmax (0.0, sec) * (double) nominal);
    const int ff = totalFrames % nominal;
    totalFrames /= nominal;
    const int ss = totalFrames % 60;
    totalFrames /= 60;
    const int mm = totalFrames % 60;
    totalFrames /= 60;
    const int hh = totalFrames % 24;
    return juce::String::formatted ("%02d:%02d:%02d:%02d", hh, mm, ss, ff);
}

MainWindow::MainWindow()
    : juce::DocumentWindow ("Easy Trigger",
                            juce::Colours::black,
                            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton | juce::DocumentWindow::closeButton)
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGB (0x11, 0x12, 0x16));
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    // Min width: left panel (390) + margins (34) + table columns min sum (712) + scrollbar (8) + padding (16)
    setResizeLimits (1160, 428, 10000, 10000);
    setContentOwned (new TriggerContentComponent(), true);
    const auto icon = loadTriggerAppIcon();
    if (icon.isValid())
        setIcon (icon);
    createTrayIcon();
    if (trayIcon_ != nullptr && icon.isValid())
        trayIcon_->setIconImage (icon, icon);
    centreWithSize (1240, 828);
    setVisible (true);
#if JUCE_WINDOWS
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainWindow> (this)]
    {
        if (safe != nullptr)
            applyNativeDarkTitleBar (*safe);
    });
#endif
}

MainWindow::~MainWindow()
{
    trayIcon_.reset();
}

void MainWindow::closeButtonPressed()
{
    if (quittingFromMenu_)
    {
        setVisible (false); // hide immediately to prevent white flash on fullscreen close
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return;
    }

    bool closeToTray = false;
    if (auto* content = dynamic_cast<TriggerContentComponent*> (getContentComponent()))
        closeToTray = content->closeToTrayEnabled();

    if (closeToTray)
    {
        setVisible (false);
        if (trayIcon_ != nullptr)
            trayIcon_->showInfoBubble ("Easy Trigger", "Running in system tray");
        return;
    }

    setVisible (false); // hide immediately to prevent white flash on fullscreen close
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void MainWindow::prepareForShutdown()
{
    juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
    setVisible (false);
    if (trayIcon_ != nullptr)
        trayIcon_->setVisible (false);
}

void MainWindow::createTrayIcon()
{
    trayIcon_ = std::make_unique<TriggerTrayIcon> (*this);
    trayIcon_->setIconTooltip ("Easy Trigger");
}

void MainWindow::showFromTray()
{
    setVisible (true);
    setMinimised (false);
    toFront (true);
}

void MainWindow::quitFromTray()
{
    quittingFromMenu_ = true;
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
} // namespace trigger
