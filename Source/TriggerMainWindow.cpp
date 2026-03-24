#include "TriggerMainWindow.h"
using trigger::styleCombo;
using trigger::styleEditor;
using trigger::styleFlatMenuButton;
using trigger::styleRowButton;
using trigger::styleSectionLabel;
using trigger::styleValueLabel;
using trigger::styleSlider;
#include "Version.h"
#include "core/ConfigStore.h"
#include "ui/windows/StatusMonitorWindow.h"
#include "ui/windows/UpdatePromptWindow.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif
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
    return kClipIdle;
}

juce::Colour clipConnectedFillFor (bool isCustom, bool hasOffset)
{
    if (isCustom)
        return kClipConnectedCustom;

    if (hasOffset)
        return kClipConnectedTc;

    return kClipConnectedPlain;
}

juce::FontOptions clipRowFont()
{
    return juce::FontOptions (kTableRowFontSize).withStyle ("Bold");
}

juce::String normaliseTriggerRangeMode (juce::String mode)
{
    mode = mode.trim().toLowerCase();
    if (mode == "pre" || mode == "post")
        return mode;
    return "mid";
}

std::pair<int, int> getTriggerWindowBounds (int triggerFrame, int rangeFrames, const juce::String& mode)
{
    const auto normalised = normaliseTriggerRangeMode (mode);
    if (normalised == "pre")
        return { triggerFrame - rangeFrames, triggerFrame };
    if (normalised == "post")
        return { triggerFrame, triggerFrame + rangeFrames };
    return { triggerFrame - rangeFrames, triggerFrame + rangeFrames };
}

bool isFrameInsideTriggerWindow (int currentFrame, int triggerFrame, int rangeFrames, const juce::String& mode)
{
    const auto [startFrame, endFrame] = getTriggerWindowBounds (triggerFrame, rangeFrames, mode);
    return currentFrame >= startFrame && currentFrame <= endFrame;
}

bool parseTcToFramesLocal (const juce::String& tc, int fps, int& outFrames)
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

std::optional<std::pair<int, int>> getClipDurationWindowBounds (int triggerFrame, const juce::String& durationTc, int liveFps)
{
    int durationFrames25 = 0;
    if (! parseTcToFramesLocal (durationTc, 25, durationFrames25) || durationFrames25 <= 0)
        return std::nullopt;

    const double durationSec = (double) durationFrames25 / 25.0;
    const int durationFramesLive = juce::jmax (1, (int) std::round (durationSec * (double) juce::jmax (1, liveFps)));
    return std::make_pair (triggerFrame, triggerFrame + durationFramesLive);
}

bool isFrameInsideClipDurationWindow (int currentFrame, int triggerFrame, const juce::String& durationTc, int liveFps)
{
    if (const auto bounds = getClipDurationWindowBounds (triggerFrame, durationTc, liveFps))
        return currentFrame >= bounds->first && currentFrame <= bounds->second;
    return false;
}

void tagPopupComponentTree (juce::Component& component, juce::MouseListener& listener, int rowNumber)
{
    if (dynamic_cast<juce::TextEditor*> (&component) != nullptr
        || dynamic_cast<juce::ComboBox*> (&component) != nullptr)
    {
        component.getProperties().remove ("triggerRowNumber");
        component.removeMouseListener (&listener);
        return;
    }

    component.getProperties().set ("triggerRowNumber", rowNumber);
    component.removeMouseListener (&listener);
    component.addMouseListener (&listener, true);

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        tagPopupComponentTree (*component.getChildComponent (i), listener, rowNumber);
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
    const auto activeBg = kSection;
    const auto activeText = juce::Colour::fromRGB (0xea, 0xea, 0xea);
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

#include "ui/widgets/TrayIcon.h"

#include "ui/widgets/TableCells.h"

#include "ui/windows/AboutWindow.h"


#include "ui/windows/DarkDialog.h"

#include "ui/windows/ColourPickerWindow.h"
// ─────────────────────────────────────────────────────────────────────────────

#include "ui/windows/PreferencesWindow.h"
// ─────────────────────────────────────────────────────────────────────────────
#include "ui/windows/GetClipsOptionsWindow.h"
}

#include "ui/workers/AudioScanThread.h"

TriggerContentComponent::TriggerContentComponent()
{
    ltcOutputApplyThread_ = std::thread ([this] { ltcOutputApplyLoop(); });
    setOpaque (true);
    loadFonts();
    applyTheme();
    addAndMakeVisible (settingsPanel_);

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
    settingsPanel_.content().addAndMakeVisible (sourceHeaderLabel_);
    sourceExpandBtn_.setExpanded (true);
    sourceExpandBtn_.onClick = [this]
    {
        sourceExpanded_ = ! sourceExpanded_;
        sourceExpandBtn_.setExpanded (sourceExpanded_);
        updateWindowHeight();
        resized();
        repaint();
    };
    settingsPanel_.content().addAndMakeVisible (sourceExpandBtn_);
    settingsPanel_.content().addAndMakeVisible (sourceCombo_);
    sourceHeaderLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
    sourceHeaderLabel_.setFont (juce::FontOptions (14.0f));
    sourceHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    sourceHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 6, 0, 0));
    juce::Label* sourceRowLabels[] = {
        &inDriverLbl_, &inDeviceLbl_, &inChannelLbl_, &inRateLbl_, &inLevelLbl_, &inGainLbl_,
        &mtcInLbl_, &artInLbl_, &artInListenIpLbl_, &oscAdapterLbl_, &oscIpLbl_, &oscPortLbl_, &oscFpsLbl_, &oscStrLbl_, &oscFloatLbl_, &oscFloatTypeLbl_, &oscFloatMaxLbl_,
        &outDriverLbl_, &outDeviceLbl_, &outChannelLbl_, &outRateLbl_, &outConvertLbl_, &outOffsetLbl_, &outLevelLbl_,
        &resAdapterLbl_, &resSendIpLbl_, &resSendIpLbl2_, &resSendIpLbl3_, &resSendIpLbl4_, &resSendIpLbl5_, &resSendIpLbl6_, &resSendIpLbl7_, &resSendIpLbl8_,
        &resSendAdapterLbl1_, &resSendAdapterLbl2_, &resSendAdapterLbl3_, &resSendAdapterLbl4_, &resSendAdapterLbl5_, &resSendAdapterLbl6_, &resSendAdapterLbl7_, &resSendAdapterLbl8_,
        &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_, &resGlobalOffsetLbl_
    };
    for (auto* l : sourceRowLabels)
    {
        l->setColour (juce::Label::backgroundColourId, row_);
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        l->setJustificationType (juce::Justification::centredLeft);
        settingsPanel_.content().addAndMakeVisible (*l);
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
    styleCombo (resolumeSendAdapterCombo6_);
    styleCombo (resolumeSendAdapterCombo7_);
    styleCombo (resolumeSendAdapterCombo8_);
    styleCombo (oscFpsCombo_);
    styleCombo (ltcOutDeviceCombo_);
    styleCombo (ltcOutChannelCombo_);
    styleCombo (ltcOutSampleRateCombo_);
    settingsPanel_.content().addAndMakeVisible (ltcConvertStrip_);
    settingsPanel_.content().addAndMakeVisible (ltcInDriverCombo_);
    settingsPanel_.content().addAndMakeVisible (ltcOutDriverCombo_);

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
                     &resolumeAdapterCombo_, &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_, &resolumeSendAdapterCombo6_, &resolumeSendAdapterCombo7_, &resolumeSendAdapterCombo8_ })
    {
        settingsPanel_.content().addAndMakeVisible (*c);
        c->onChange = [this] { onInputSettingsChanged(); };
    }
    for (auto* c : { &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_ })
    {
        settingsPanel_.content().addAndMakeVisible (*c);
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
        settingsPanel_.content().repaint();
        settingsPanel_.viewport().repaint();
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
        onInputSettingsChanged();   // starts/stops inputs for the newly selected source
        updateWindowHeight();
        resized();
        repaint();
    };
    ltcOutSwitch_.onToggle = [this] (bool) { onOutputToggleChanged(); };
    ltcThruDot_.onToggle = [this] (bool) { queueLtcOutputApply(); };

    settingsPanel_.content().addAndMakeVisible (ltcInGainSlider_);
    settingsPanel_.content().addAndMakeVisible (ltcInLevelBar_);
    settingsPanel_.content().addAndMakeVisible (ltcOutLevelSlider_);
    settingsPanel_.content().addAndMakeVisible (ltcOffsetEditor_);
    settingsPanel_.content().addAndMakeVisible (oscIpEditor_);
    settingsPanel_.content().addAndMakeVisible (oscPortEditor_);
    settingsPanel_.content().addAndMakeVisible (oscAddrStrEditor_);
    settingsPanel_.content().addAndMakeVisible (oscAddrFloatEditor_);
    settingsPanel_.content().addAndMakeVisible (oscFloatTypeCombo_);
    settingsPanel_.content().addAndMakeVisible (oscFloatMaxEditor_);
    settingsPanel_.content().addAndMakeVisible (artnetListenIpEditor_);

    resolumeSendIp_.setText ("127.0.0.1");
    resolumeSendPort_.setText ("7000");
    resAdapterLbl_.setText ("Adapter:", juce::dontSendNotification);
    resSendIpLbl_.setText ("Send 1:", juce::dontSendNotification);
    resSendIpLbl2_.setText ("Send 2:", juce::dontSendNotification);
    resSendIpLbl3_.setText ("Send 3:", juce::dontSendNotification);
    resSendIpLbl4_.setText ("Send 4:", juce::dontSendNotification);
    resSendIpLbl5_.setText ("Send 5:", juce::dontSendNotification);
    resSendIpLbl6_.setText ("Send 6:", juce::dontSendNotification);
    resSendIpLbl7_.setText ("Send 7:", juce::dontSendNotification);
    resSendIpLbl8_.setText ("Send 8:", juce::dontSendNotification);
    for (auto* b : { &resolumeSendExpandBtn1_, &resolumeSendExpandBtn2_, &resolumeSendExpandBtn3_, &resolumeSendExpandBtn4_, &resolumeSendExpandBtn5_, &resolumeSendExpandBtn6_, &resolumeSendExpandBtn7_, &resolumeSendExpandBtn8_ })
        b->setColours (input_, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a), juce::Colour::fromRGB (0xf2, 0xf2, 0xf2));
    resolumeSendIp2_.setText ("127.0.0.1");
    resolumeSendPort2_.setText ("7000");
    resolumeSendIp3_.setText ("127.0.0.1");
    resolumeSendPort3_.setText ("7000");
    resolumeSendIp4_.setText ("127.0.0.1");
    resolumeSendPort4_.setText ("7000");
    resolumeSendIp5_.setText ("127.0.0.1");
    resolumeSendPort5_.setText ("7000");
    resolumeSendIp6_.setText ("127.0.0.1");
    resolumeSendPort6_.setText ("7000");
    resolumeSendIp7_.setText ("127.0.0.1");
    resolumeSendPort7_.setText ("7000");
    resolumeSendIp8_.setText ("127.0.0.1");
    resolumeSendPort8_.setText ("7000");
    resListenIpLbl_.setText ("Listen:", juce::dontSendNotification);
    for (auto* e : { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_, &resolumeSendPort6_, &resolumeSendPort7_, &resolumeSendPort8_ })
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
    resolumeDelTargetBtn6_.onClick = [this] { removeResolumeSendTarget (5); };
    resolumeDelTargetBtn7_.onClick = [this] { removeResolumeSendTarget (6); };
    resolumeDelTargetBtn8_.onClick = [this] { removeResolumeSendTarget (7); };
    resolumeAdapterCombo_.onChange = [this] { syncResolumeListenIpWithAdapter(); };
    resolumeSendExpandBtn1_.onClick = [this] { toggleSendAdapterExpanded (0); };
    resolumeSendExpandBtn2_.onClick = [this] { toggleSendAdapterExpanded (1); };
    resolumeSendExpandBtn3_.onClick = [this] { toggleSendAdapterExpanded (2); };
    resolumeSendExpandBtn4_.onClick = [this] { toggleSendAdapterExpanded (3); };
    resolumeSendExpandBtn5_.onClick = [this] { toggleSendAdapterExpanded (4); };
    resolumeSendExpandBtn6_.onClick = [this] { toggleSendAdapterExpanded (5); };
    resolumeSendExpandBtn7_.onClick = [this] { toggleSendAdapterExpanded (6); };
    resolumeSendExpandBtn8_.onClick = [this] { toggleSendAdapterExpanded (7); };
    resolumeSendExpandBtn1_.setExpanded (resolumeSendExpanded1_);
    resolumeSendExpandBtn2_.setExpanded (resolumeSendExpanded2_);
    resolumeSendExpandBtn3_.setExpanded (resolumeSendExpanded3_);
    resolumeSendExpandBtn4_.setExpanded (resolumeSendExpanded4_);
    resolumeSendExpandBtn5_.setExpanded (resolumeSendExpanded5_);
    resolumeSendExpandBtn6_.setExpanded (resolumeSendExpanded6_);
    resolumeSendExpandBtn7_.setExpanded (resolumeSendExpanded7_);
    resolumeSendExpandBtn8_.setExpanded (resolumeSendExpanded8_);
    getTriggersBtn_.onClick = [this] { openGetClipsOptions(); };
    createCustomBtn_.onClick = [this]
    {
        addCustomColTrigger();
        createCustomBtn_.setEnabled (! hasCustomGroupsAtLimit());
    };
    styleFlatMenuButton (fileMenuBtn_);
    manageMenuBtn_.setButtonText ("Manage Triggers");
    styleFlatMenuButton (manageMenuBtn_);
    styleFlatMenuButton (viewMenuBtn_);
    styleFlatMenuButton (helpMenuBtn_);
    fileMenuBtn_.onClick = [this] { openFileMenu(); };
    manageMenuBtn_.onClick = [this] { openManageMenu(); };
    viewMenuBtn_.onClick = [this] { openViewMenu(); };
    helpMenuBtn_.onClick = [this] { openHelpMenu(); };
    resolumeExpandBtn_.setExpanded (false);
    helpButton_.onClick = [this] { openHelpPage(); };
    resolumeExpandBtn_.onClick = [this] { resolumeExpanded_ = ! resolumeExpanded_; resolumeExpandBtn_.setExpanded (resolumeExpanded_); updateWindowHeight(); resized(); repaint(); };
    styleSectionLabel (triggerOutHeaderLabel_);
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
    styleSectionLabel (outLtcHeaderLabel_);
    outLtcHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    outLtcHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 42, 0, 0));
    settingsPanel_.content().addAndMakeVisible (outLtcHeaderLabel_);
    settingsPanel_.content().addAndMakeVisible (outLtcExpandBtn_);
    outLtcExpandBtn_.setExpanded (false);
    outLtcExpandBtn_.onClick = [this]
    {
        outLtcExpanded_ = ! outLtcExpanded_;
        outLtcExpandBtn_.setExpanded (outLtcExpanded_);
        updateWindowHeight();
        resized();
        repaint();
    };
    settingsPanel_.content().addAndMakeVisible (ltcOutSwitch_);
    settingsPanel_.content().addAndMakeVisible (ltcThruDot_);
    ltcThruLbl_.setColour (juce::Label::textColourId, kTextSecondary);
    ltcThruLbl_.setJustificationType (juce::Justification::centredLeft);
    settingsPanel_.content().addAndMakeVisible (ltcThruLbl_);
    styleRowButton (settingsButton_, kRow);
    settingsButton_.onClick = [this] { openSettingsMenu(); };
    styleRowButton (quitButton_, juce::Colour::fromRGB (0xb6, 0x45, 0x40));
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
    h.addColumn ("",           1,  30,  30);
    h.addColumn ("In",         2,  34,  34);
    h.addColumn ("Name",       3, 150, 150);
    h.addColumn ("Count",      4,  92,  92);
    h.addColumn ("Range",      5,  58,  58);
    h.addColumn ("Trigger",    6,  92,  92);
    h.addColumn ("Duration",   7,  92,  92);
    h.addColumn ("End Action", 8, 110, 110);  // dynamic min enforced in tableColumnsResized
    h.addColumn ("Send",       9,  84,  84);
    h.addColumn ("Test",      10,  56,  56, 56, juce::TableHeaderComponent::notResizable);
    h.addListener (this);
    h.setStretchToFitActive (false);
    h.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    h.setColour (juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));

    clipCollector_.onChanged = [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
    {
        if (safe == nullptr)
            return;
        safe->queryPending_ = false;
        const auto nowMs = juce::Time::currentTimeMillis();

        if (safe->clipReceiveEnabled_)
        {
            safe->clipRefreshQueued_ = true;
            safe->clipRefreshDueMs_ = nowMs + 120;
            safe->clipImportQuietUntilMs_ = nowMs + 350;
        }
        else
        {
            safe->clipFeedbackDirty_ = true;
            safe->clipFeedbackDueMs_ = nowMs + 16;
        }
    };

    clipCollector_.onRawMessage = [this] (const juce::OSCMessage& msg)
    {
        lastOscInMs_ = juce::Time::currentTimeMillis();
        lastOscInPort_ = resolumeListenPort_.getText().getIntValue();
        oscListenOk_ = true;
        oscListenState_ = "ok";
        oscListenDetail_ = "listening " + resolumeListenIp_.getText().trim()
            + ":" + juce::String (juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()));
        updateNetworkStatusIndicator();

        if (clipReceiveEnabled_ && rawOscDiagnosticLogsRemaining_ > 0)
        {
            --rawOscDiagnosticLogsRemaining_;
            logGetClipsDiagnostic ("OSC IN " + msg.getAddressPattern().toString());
        }
        else if (clipReceiveEnabled_ && ! rawOscDiagnosticSuppressedLogged_)
        {
            rawOscDiagnosticSuppressedLogged_ = true;
            logGetClipsDiagnostic ("OSC IN logging suppressed to keep app responsive");
        }
        OscLogEntry e;
        e.dir         = OscLogEntry::Dir::input;
        e.timestampMs = juce::Time::currentTimeMillis();
        e.ip          = resolumeListenIp_.getText().trim();
        e.port        = resolumeListenPort_.getText().getIntValue();
        e.address     = msg.getAddressPattern().toString();
        if (msg.size() > 0)
        {
            if      (msg[0].isInt32())   { e.type = "i32"; e.value = juce::String (msg[0].getInt32()); }
            else if (msg[0].isFloat32()) { e.type = "f32"; e.value = juce::String (msg[0].getFloat32(), 4); }
            else if (msg[0].isString())  { e.type = "str"; e.value = msg[0].getString(); }
            else                         { e.type = "..."; }
        }
        else
        {
            e.type = "bang";
        }
        oscLog_.push (std::move (e));
    };

    addAndMakeVisible (easyLabel_);
    addAndMakeVisible (triggerLabel_);
    addAndMakeVisible (versionLabel_);
    addAndMakeVisible (tcLabel_);
    addAndMakeVisible (*fpsIndicatorStrip_);
    statusBar_.onClick = [this] { openStatusMonitorWindow(); };
    addAndMakeVisible (statusBar_);
    settingsPanel_.content().addAndMakeVisible (resolumeHeader_);
    settingsPanel_.content().addAndMakeVisible (resolumeExpandBtn_);
    settingsPanel_.content().addAndMakeVisible (triggerOutHeaderLabel_);
    settingsPanel_.content().addAndMakeVisible (triggerOutExpandBtn_);
    settingsPanel_.content().addAndMakeVisible (resAdapterLbl_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl2_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl3_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl4_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl5_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl6_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl7_);
    settingsPanel_.content().addAndMakeVisible (resSendIpLbl8_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl1_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl2_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl3_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl4_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl5_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl6_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl7_);
    settingsPanel_.content().addAndMakeVisible (resSendAdapterLbl8_);
    settingsPanel_.content().addAndMakeVisible (resSendPortLbl_);
    settingsPanel_.content().addAndMakeVisible (resListenIpLbl_);
    settingsPanel_.content().addAndMakeVisible (resListenPortLbl_);
    settingsPanel_.content().addAndMakeVisible (resMaxLayersLbl_);
    settingsPanel_.content().addAndMakeVisible (resMaxClipsLbl_);
    settingsPanel_.content().addAndMakeVisible (resGlobalOffsetLbl_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp2_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort2_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp3_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort3_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp4_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort4_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp5_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort5_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp6_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort6_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp7_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort7_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendIp8_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendPort8_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn1_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn2_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn3_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn4_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn5_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn6_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn7_);
    settingsPanel_.content().addAndMakeVisible (resolumeSendExpandBtn8_);
    settingsPanel_.content().addAndMakeVisible (resolumeAddTargetBtn_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn2_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn3_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn4_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn5_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn6_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn7_);
    settingsPanel_.content().addAndMakeVisible (resolumeDelTargetBtn8_);
    settingsPanel_.content().addAndMakeVisible (resolumeListenIp_);
    settingsPanel_.content().addAndMakeVisible (resolumeListenPort_);
    settingsPanel_.content().addAndMakeVisible (resolumeMaxLayers_);
    settingsPanel_.content().addAndMakeVisible (resolumeMaxClips_);
    settingsPanel_.content().addAndMakeVisible (resolumeGlobalOffset_);
    addAndMakeVisible (fileMenuBtn_);
    addAndMakeVisible (manageMenuBtn_);
    addAndMakeVisible (viewMenuBtn_);
    addAndMakeVisible (helpMenuBtn_);
    addAndMakeVisible (triggerTable_);
    if (auto* vp = triggerTable_.getViewport())
        vp->setScrollBarsShown (true, false);

    setSize (1240, 820);
    resized();
    setResolumeStatusText ("Resolume idle", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    setNetworkStatusText ("OSC idle", juce::Colour::fromRGB (0xa0, 0xa4, 0xac));
    setTimecodeStatusText ("SAFE START", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
    loadRuntimePrefs();
    startAudioDeviceScan();
    startTimerHz (60);
}

TriggerContentComponent::~TriggerContentComponent()
{
    // Close status monitor first so its OscLog* pointer is never dangling
    if (auto* mon = statusMonitor_.getComponent())
        delete mon;

    if (auto* prefs = preferencesWindow_.getComponent())
        delete prefs;

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

    const int triggerOutRows = juce::jlimit (1, 8, resolumeSendTargetCount_)
                             + (resolumeSendExpanded1_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 6 && resolumeSendExpanded6_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 7 && resolumeSendExpanded7_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 8 && resolumeSendExpanded8_ ? 1 : 0);
    const int resolumeRows = 5;

    int h = 16;
    h += 40 + 4;
    h += 90;
    h += 4 + kCompactBarHeight + 4;
    h += 24 + 4;
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
    h += 88;
    h += 24;
    h += kStartupHeightExtra;
    return juce::jlimit (420, 1400, h);
}

void TriggerContentComponent::updateWindowHeight (bool forceGrow)
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

        constexpr int kBaseMinW  = 1284;  // left(390)+overhead(34)+cols_min(793)+test(56)+scroll+pad(10)
        constexpr int kCustomExtraW = 184 + 42;
        const int minW = kBaseMinW + (hasCustomGroup() ? kCustomExtraW : 0);
        window->setResizeLimits (minW, minTotal, 1800, 1400);

        if (forceGrow && window->getHeight() < minTotal)
            window->setSize (window->getWidth(), minTotal);
    }
}

void TriggerContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg_);
    if (! headerRect_.isEmpty())
    {
        g.setColour (kHeader);
        g.fillRoundedRectangle (headerRect_.toFloat(), 5.0f);
        g.setColour (kWindowBorder);
        g.drawRoundedRectangle (headerRect_.toFloat(), 5.0f, 1.0f);
    }
    if (! timerRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));
        g.fillRoundedRectangle (timerRect_.toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x33, 0x33, 0x33));
        g.drawRoundedRectangle (timerRect_.toFloat(), 5.0f, 1.0f);
    }
    juce::ignoreUnused (menuBarRect_);
    {
        juce::Graphics::ScopedSaveState savedState (g);
        auto clip = leftViewportRect_;
        g.reduceClipRegion (clip);
        const auto viewPos = settingsPanel_.getViewPosition();
        g.setColour (kSection);
        for (auto r : sectionRowRects_)
            g.fillRoundedRectangle (r.translated (clip.getX() - viewPos.x, clip.getY() - viewPos.y).toFloat(), 5.0f);
        g.setColour (kRow);
        for (auto r : leftRowRects_)
            g.fillRoundedRectangle (r.translated (clip.getX() - viewPos.x, clip.getY() - viewPos.y).toFloat(), 5.0f);
    }
    g.setColour (bg_);
    for (auto r : rightSectionRects_)
        g.fillRoundedRectangle (r.toFloat(), 6.0f);

}

void TriggerContentComponent::paintOverChildren (juce::Graphics& g)
{
    if (! dragActive_ || dragHoverRow_ < 0)
        return;

    auto rowBounds = triggerTable_.getRowPosition (dragHoverRow_, true);
    if (rowBounds.isEmpty())
        return;

    const int y = rowBounds.getY();
    g.setColour (kStatusOk);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) rowBounds.getX() + 8.0f,
                                                    (float) y + 1.0f,
                                                    (float) rowBounds.getWidth() - 16.0f,
                                                    3.0f),
                            1.5f);
}

void TriggerContentComponent::resized()
{
    leftRowRects_.clear();
    sectionRowRects_.clear();
    rightSectionRects_.clear();
    headerRect_ = {};
    menuBarRect_ = {};
    timerRect_ = {};
    auto bounds = getLocalBounds();
    statusBar_.setBounds (bounds.removeFromBottom (24));
    auto content = bounds.reduced (8);
    menuBarRect_ = content.removeFromTop (24);
    content.removeFromTop (4);
    const int totalW = content.getWidth();
    int leftW = juce::jlimit (330, 390, (int) std::round ((double) totalW * 0.40));
    if (totalW - leftW < 420)
        leftW = juce::jmax (300, totalW - 420);
    // Never let the left panel consume the entire width – keep at least 300 px for the table
    leftW = juce::jmin (leftW, juce::jmax (0, totalW - 300));
    auto left = content.removeFromLeft (leftW);
    auto right = content.reduced (6, 0);

    juce::Rectangle<int> getTriggersRow;
    juce::Rectangle<int> createCustomRow;
    layoutSettingsPanelChrome (left, getTriggersRow, createCustomRow);
    const auto src = sourceCombo_.getSelectedId();
    auto rowsForSource = [this, src]() -> int
    {
        if (src == 1) return 6;
        if (src == 2) return 1;
        if (src == 3) return 2;
        return oscFloatTypeCombo_.getSelectedId() == 3 ? 8 : 7;
    };
    const int triggerOutRows = juce::jlimit (1, 8, resolumeSendTargetCount_)
                             + (resolumeSendExpanded1_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 6 && resolumeSendExpanded6_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 7 && resolumeSendExpanded7_ ? 1 : 0)
                             + (resolumeSendTargetCount_ >= 8 && resolumeSendExpanded8_ ? 1 : 0);
    const int resolumeRows = 5;
    const int contentRows = 1 + (sourceExpanded_ ? rowsForSource() : 0)
                          + 1 + (resolumeExpanded_ ? resolumeRows : 0)
                          + 1 + (triggerOutExpanded_ ? triggerOutRows : 0)
                          + 1 + (outLtcExpanded_ ? 7 : 0);
    const int viewportScrollWidth = settingsPanel_.getScrollBarThickness();
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
    auto layoutSourceSection = [&]()
    {
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

        if (! sourceExpanded_)
            return;

        if (src == 1)
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
            return;
        }

        if (src == 2)
        {
            layoutParam (mtcInLbl_, mtcInCombo_);
            return;
        }

        if (src == 3)
        {
            layoutParam (artInLbl_, artnetInCombo_);
            layoutParam (artInListenIpLbl_, artnetListenIpEditor_);
            return;
        }

        layoutParam (oscAdapterLbl_, oscAdapterCombo_);
        layoutParam (oscIpLbl_, oscIpEditor_);
        layoutParam (oscPortLbl_, oscPortEditor_);
        layoutParam (oscFpsLbl_, oscFpsCombo_);
        layoutParam (oscStrLbl_, oscAddrStrEditor_);
        layoutParam (oscFloatLbl_, oscAddrFloatEditor_);
        layoutParam (oscFloatTypeLbl_, oscFloatTypeCombo_);
        const bool showMax = oscFloatTypeCombo_.getSelectedId() == 3;
        layoutParam (oscFloatMaxLbl_, oscFloatMaxEditor_, showMax);
    };

    auto layoutResolumeSection = [&]()
    {
        headerRow (resolumeHeader_, resolumeExpandBtn_);
        resolumeHeader_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        resolumeExpandBtn_.setExpanded (resolumeExpanded_);
        if (! resolumeExpanded_)
            return;

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
    };

    auto layoutTriggerOutSection = [&]()
    {
        headerRow (triggerOutHeaderLabel_, triggerOutExpandBtn_);
        triggerOutHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        triggerOutExpandBtn_.setExpanded (triggerOutExpanded_);
        if (! triggerOutExpanded_)
            return;

        layoutTriggerTargetRow (resSendIpLbl_,  resolumeSendExpandBtn1_, resolumeSendIp_,  resolumeSendPort_,  resSendAdapterLbl1_, resolumeSendAdapterCombo1_, resolumeAddTargetBtn_,  resolumeSendExpanded1_, true);
        layoutTriggerTargetRow (resSendIpLbl2_, resolumeSendExpandBtn2_, resolumeSendIp2_, resolumeSendPort2_, resSendAdapterLbl2_, resolumeSendAdapterCombo2_, resolumeDelTargetBtn2_, resolumeSendExpanded2_, resolumeSendTargetCount_ >= 2);
        layoutTriggerTargetRow (resSendIpLbl3_, resolumeSendExpandBtn3_, resolumeSendIp3_, resolumeSendPort3_, resSendAdapterLbl3_, resolumeSendAdapterCombo3_, resolumeDelTargetBtn3_, resolumeSendExpanded3_, resolumeSendTargetCount_ >= 3);
        layoutTriggerTargetRow (resSendIpLbl4_, resolumeSendExpandBtn4_, resolumeSendIp4_, resolumeSendPort4_, resSendAdapterLbl4_, resolumeSendAdapterCombo4_, resolumeDelTargetBtn4_, resolumeSendExpanded4_, resolumeSendTargetCount_ >= 4);
        layoutTriggerTargetRow (resSendIpLbl5_, resolumeSendExpandBtn5_, resolumeSendIp5_, resolumeSendPort5_, resSendAdapterLbl5_, resolumeSendAdapterCombo5_, resolumeDelTargetBtn5_, resolumeSendExpanded5_, resolumeSendTargetCount_ >= 5);
        layoutTriggerTargetRow (resSendIpLbl6_, resolumeSendExpandBtn6_, resolumeSendIp6_, resolumeSendPort6_, resSendAdapterLbl6_, resolumeSendAdapterCombo6_, resolumeDelTargetBtn6_, resolumeSendExpanded6_, resolumeSendTargetCount_ >= 6);
        layoutTriggerTargetRow (resSendIpLbl7_, resolumeSendExpandBtn7_, resolumeSendIp7_, resolumeSendPort7_, resSendAdapterLbl7_, resolumeSendAdapterCombo7_, resolumeDelTargetBtn7_, resolumeSendExpanded7_, resolumeSendTargetCount_ >= 7);
        layoutTriggerTargetRow (resSendIpLbl8_, resolumeSendExpandBtn8_, resolumeSendIp8_, resolumeSendPort8_, resSendAdapterLbl8_, resolumeSendAdapterCombo8_, resolumeDelTargetBtn8_, resolumeSendExpanded8_, resolumeSendTargetCount_ >= 8);
    };

    auto layoutOutLtcSection = [&]()
    {
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

        if (! outLtcExpanded_)
            return;

        layoutParam (outDriverLbl_, ltcOutDriverCombo_);
        layoutParam (outDeviceLbl_, ltcOutDeviceCombo_);
        layoutParam (outChannelLbl_, ltcOutChannelCombo_);
        layoutParam (outRateLbl_, ltcOutSampleRateCombo_);
        layoutParam (outConvertLbl_, ltcConvertStrip_);
        layoutParam (outOffsetLbl_, ltcOffsetEditor_);
        layoutParam (outLevelLbl_, ltcOutLevelSlider_);
    };

    layoutSourceSection();
    layoutResolumeSection();
    layoutTriggerOutSection();
    layoutOutLtcSection();
    settingsPanel_.setContentSize (juce::jmax (0, leftLayoutArea.getWidth()), juce::jmax (contentY, leftViewportRect_.getHeight()));
    updateLeftPanelVisibility (src);

    getTriggersBtn_.setVisible (true);
    createCustomBtn_.setVisible (true);
    createCustomBtn_.setEnabled (! hasCustomGroupsAtLimit());

    layoutMenuButtons();

    rightSectionRects_.add (right);
    triggerTable_.setBounds (right.reduced (3));
    updateTableColumnWidths();

    bringChromeToFront();
}

void TriggerContentComponent::layoutSettingsPanelChrome (juce::Rectangle<int>& left,
                                                         juce::Rectangle<int>& getTriggersRow,
                                                         juce::Rectangle<int>& createCustomRow)
{
    headerRect_ = left.removeFromTop (40);
    auto top = headerRect_.reduced (6, 0);
    const int versionW = juce::jmax (58, versionLabel_.getFont().getStringWidth (versionLabel_.getText()) + 4);
    auto versionArea = top.removeFromRight (versionW);
    const int easyW = juce::jmax (46, easyLabel_.getFont().getStringWidth ("EASY ") + 6);
    const int trigW = juce::jmax (90, triggerLabel_.getFont().getStringWidth ("TRIGGER") + 6);
    const int startX = top.getX() + 2;
    const int yOff = 6;
    const int titleH = juce::jmax (1, top.getHeight() - yOff);
    easyLabel_.setBounds (startX, top.getY() + yOff, easyW, titleH);
    triggerLabel_.setBounds (startX + easyW, top.getY() + yOff, trigW, titleH);
    versionLabel_.setBounds (versionArea.getX(), top.getY() + yOff, versionW, titleH);
    left.removeFromTop (4);
    timerRect_ = left.removeFromTop (90);
    tcLabel_.setBounds (timerRect_);
    left.removeFromTop (4);
    auto fpsRect = left.removeFromTop (kCompactBarHeight);
    if (fpsIndicatorStrip_ != nullptr)
        fpsIndicatorStrip_->setBounds (fpsRect);
    left.removeFromTop (4);

    juce::ignoreUnused (getTriggersRow, createCustomRow);
    settingsPanel_.setBounds (left);
    settingsPanel_.layoutPanel (settingsPanel_.getLocalBounds());
    leftViewportRect_ = settingsPanel_.getViewportBoundsInParent();
}

void TriggerContentComponent::layoutMenuButtons()
{
    auto menuButtons = menuBarRect_;
    menuButtons.reduce (0, 0);
    const int menuGap = 2;
    const auto menuButtonWidth = [] (const juce::TextButton& button)
    {
        return juce::jmax (42, juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")).getStringWidth (button.getButtonText()) + 18);
    };
    fileMenuBtn_.setBounds (menuButtons.removeFromLeft (menuButtonWidth (fileMenuBtn_)));
    menuButtons.removeFromLeft (menuGap);
    manageMenuBtn_.setBounds (menuButtons.removeFromLeft (menuButtonWidth (manageMenuBtn_)));
    menuButtons.removeFromLeft (menuGap);
    viewMenuBtn_.setBounds (menuButtons.removeFromLeft (menuButtonWidth (viewMenuBtn_)));
    menuButtons.removeFromLeft (menuGap);
    helpMenuBtn_.setBounds (menuButtons.removeFromLeft (menuButtonWidth (helpMenuBtn_)));
}

void TriggerContentComponent::bringChromeToFront()
{
    easyLabel_.toFront (false);
    triggerLabel_.toFront (false);
    versionLabel_.toFront (false);
    tcLabel_.toFront (false);
    if (fpsIndicatorStrip_ != nullptr)
        fpsIndicatorStrip_->toFront (false);
    getTriggersBtn_.toFront (false);
    createCustomBtn_.toFront (false);
    fileMenuBtn_.toFront (false);
    manageMenuBtn_.toFront (false);
    viewMenuBtn_.toFront (false);
    helpMenuBtn_.toFront (false);
    statusBar_.toFront (false);
    settingsPanel_.bringButtonsToFront();
}

void TriggerContentComponent::updateLeftPanelVisibility (int src)
{
    juce::Component* comps[] = {
        &ltcInDriverCombo_, &ltcInDeviceCombo_, &ltcInChannelCombo_, &ltcInSampleRateCombo_, &ltcInLevelBar_, &ltcInGainSlider_,
        &mtcInCombo_, &artnetInCombo_, &artnetListenIpEditor_, &oscAdapterCombo_, &oscIpEditor_, &oscPortEditor_, &oscFpsCombo_, &oscAddrStrEditor_, &oscAddrFloatEditor_, &oscFloatTypeCombo_, &oscFloatMaxEditor_,
        &ltcOutDriverCombo_, &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_, &ltcConvertStrip_, &ltcOffsetEditor_, &ltcOutLevelSlider_,
        &resolumeAdapterCombo_, &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_, &resolumeSendAdapterCombo6_, &resolumeSendAdapterCombo7_, &resolumeSendAdapterCombo8_
    };
    for (auto* c : comps)
        if (c != nullptr)
            c->setVisible (false);

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
    resSendIpLbl6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6);
    resSendIpLbl7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7);
    resSendIpLbl8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8);
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
    resolumeSendIp6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6);
    resolumeSendPort6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6);
    resolumeSendIp7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7);
    resolumeSendPort7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7);
    resolumeSendIp8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8);
    resolumeSendPort8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8);
    resolumeSendExpandBtn1_.setVisible (triggerOutExpanded_);
    resolumeSendExpandBtn2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeSendExpandBtn3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeSendExpandBtn4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeSendExpandBtn5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resolumeSendExpandBtn6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6);
    resolumeSendExpandBtn7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7);
    resolumeSendExpandBtn8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8);
    resSendAdapterLbl1_.setVisible (triggerOutExpanded_ && resolumeSendExpanded1_);
    resSendAdapterLbl2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_);
    resSendAdapterLbl3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_);
    resSendAdapterLbl4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_);
    resSendAdapterLbl5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_);
    resSendAdapterLbl6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6 && resolumeSendExpanded6_);
    resSendAdapterLbl7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7 && resolumeSendExpanded7_);
    resSendAdapterLbl8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8 && resolumeSendExpanded8_);
    resolumeSendAdapterCombo1_.setVisible (triggerOutExpanded_ && resolumeSendExpanded1_);
    resolumeSendAdapterCombo2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2 && resolumeSendExpanded2_);
    resolumeSendAdapterCombo3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3 && resolumeSendExpanded3_);
    resolumeSendAdapterCombo4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4 && resolumeSendExpanded4_);
    resolumeSendAdapterCombo5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5 && resolumeSendExpanded5_);
    resolumeSendAdapterCombo6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6 && resolumeSendExpanded6_);
    resolumeSendAdapterCombo7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7 && resolumeSendExpanded7_);
    resolumeSendAdapterCombo8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8 && resolumeSendExpanded8_);
    resolumeAddTargetBtn_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ < 8);
    resolumeDelTargetBtn2_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 2);
    resolumeDelTargetBtn3_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 3);
    resolumeDelTargetBtn4_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 4);
    resolumeDelTargetBtn5_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 5);
    resolumeDelTargetBtn6_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 6);
    resolumeDelTargetBtn7_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 7);
    resolumeDelTargetBtn8_.setVisible (triggerOutExpanded_ && resolumeSendTargetCount_ >= 8);
}

void TriggerContentComponent::mouseUp (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        if (auto* source = event.eventComponent)
        {
            auto* tagged = source;
            while (tagged != nullptr)
            {
                if (tagged->getProperties().contains ("triggerRowNumber"))
                {
                    showRowContextMenu ((int) tagged->getProperties()["triggerRowNumber"]);
                    return;
                }
                tagged = tagged->getParentComponent();
            }
        }
    }

}

void TriggerContentComponent::updateTableColumnWidths()
{
    auto& h = triggerTable_.getHeader();
    constexpr int kTableScrollbarW = 8;
    constexpr int kCustomColsW = 42;         // only the delete col (11) is fixed; Custom Trigger scales
    constexpr int kTestColW    = 56;         // Test col is always fixed
    const bool hasCustom = h.getIndexOfColumnId (11, false) >= 0;
    const int  testColId = hasCustom ? 12 : 10;
    int available = triggerTable_.getWidth();
    available -= kTableScrollbarW;
    available -= 2;
    available -= kTestColW;  // Test col excluded from scaling
    if (hasCustom)
        available -= kCustomColsW;
    available = juce::jmax (200, available);

    struct Col
    {
        int id;
        int base;
        int min;
    };

    // End Action: Set button (36) + gap (4) + info label (~70)
    constexpr int endActionMin = 110;

    std::vector<Col> cols {
        { 1,  34,  30 },           // arrow
        { 2,  38,  34 },           // in
        { 3, 186, 150 },           // name
        { 4,  96,  92 },           // count    – "00:00:00:00" + padding
        { 5, 112, 100 },           // range mode + value
        { 6,  98,  92 },           // trigger  – timecode
        { 7,  98,  92 },           // duration – timecode
        { 8, 165, endActionMin },  // end action – Set + info
        { 9,  88,  80 }            // send
    };
    if (hasCustom)
        cols.push_back ({ 10, 180, 110 });  // Custom Trigger – Set + summary
    // Test col set separately – not included in scaling

    int baseSum = 0;
    for (const auto& c : cols)
        baseSum += c.base;
    const double scale = (double) available / (double) baseSum;

    std::vector<int> widths (cols.size(), 0);
    int used = 0;
    for (size_t i = 0; i < cols.size(); ++i)
    {
        int w = juce::jmax (cols[i].min, (int) std::round ((double) cols[i].base * scale));
        widths[i] = w;
        used += w;
    }

    // Keep Send/Test compact-looking; absorb delta mostly into Name.
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

    h.setColumnWidth (testColId, kTestColW);
}

void TriggerContentComponent::refreshTriggerTableContent()
{
    triggerTable_.updateContent();
    repaintTriggerTable();
}

void TriggerContentComponent::repaintTriggerTable()
{
    triggerTable_.repaint();
}

void TriggerContentComponent::addCustomColumns()
{
    auto& h = triggerTable_.getHeader();
    if (h.getIndexOfColumnId (10, false) >= 0)
    {
        if (h.getIndexOfColumnId (12, false) >= 0)
            return; // already present
        h.removeColumn (10);
    }
    const int f = juce::TableHeaderComponent::notResizable;
    h.addColumn ("Custom Trigger", 10, 180, 110, 600);  // resizable, min = Set+summary
    h.addColumn ("",               11,  42,  42,  42, f);
    h.addColumn ("Test",           12,  56,  56,  56, juce::TableHeaderComponent::notResizable);

    // Move Custom Trigger and its delete button to right after Name (index 2 → positions 3,4)
    h.moveColumn (10, 3);
    h.moveColumn (11, 4);

    constexpr int kExtraW  = 184 + 42;
    constexpr int kMinW    = 1440;  // left(390)+overhead(34)+cols_min(738)+test(56)+custom(152)+scroll(10)+buffer(60)
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
    {
        const int newW = juce::jmax (juce::jmin (1800, w->getWidth() + kExtraW), kMinW);
        w->setSize (newW, w->getHeight());
    }
}

void TriggerContentComponent::removeCustomColumns()
{
    auto& h = triggerTable_.getHeader();
    if (h.getIndexOfColumnId (12, false) < 0)
        return;

    constexpr int kExtraW = 184 + 42;
    h.removeColumn (10);
    h.removeColumn (11);
    h.removeColumn (12);
    h.addColumn ("Test", 10, 56, 56, 56, juce::TableHeaderComponent::notResizable);
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->setSize (juce::jmax (1284, w->getWidth() - kExtraW), w->getHeight());
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

    // End Action: Set + info label
    enforceMin (8, 110);
    // Custom Trigger: Set + summary (when present)
    if (h.getIndexOfColumnId (10, false) >= 0 && h.getIndexOfColumnId (11, false) >= 0)
        enforceMin (10, 110);

    colWidthGuard_ = false;
}

void TriggerContentComponent::timerCallback()
{
    const auto nowMs = juce::Time::currentTimeMillis();

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

    if (clipRefreshQueued_ && nowMs >= clipRefreshDueMs_)
    {
        clipRefreshQueued_ = false;
        refreshTriggerRows();
    }

    if (clipFeedbackDirty_ && nowMs >= clipFeedbackDueMs_)
    {
        clipFeedbackDirty_ = false;
        syncTriggerRowFeedbackFromCollector();
    }

    if (clipReceiveEnabled_ && ! queryPending_ && clipImportQuietUntilMs_ > 0 && nowMs >= clipImportQuietUntilMs_)
    {
        clipReceiveEnabled_ = false;
        clipImportQuietUntilMs_ = 0;
        logGetClipsDiagnostic ("Get Clips import settled; live OSC feedback will no longer rebuild rows");
    }

    if (queryPending_ && (nowMs - queryStartMs_) > 3000)
    {
        queryPending_ = false;
        clipReceiveEnabled_ = false;
        clipRefreshQueued_ = false;
        clipImportQuietUntilMs_ = 0;
        oscListenOk_ = false;
        oscListenState_ = "error";
        oscListenDetail_ = "no reply on port " + juce::String (juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()))
            + " (possible firewall / Resolume output mismatch)";
        updateNetworkStatusIndicator();
        logGetClipsDiagnostic ("Get Clips timeout: no OSC reply within 3000 ms");
        DarkDialog::show ("Easy Trigger",
                          "Could not receive clips from Resolume.\n"
                          "Please check IP address and port settings.",
                          getParentComponent());
    }

    if (updateCheckDelay_ > 0)
    {
        if (--updateCheckDelay_ == 0)
            requestUpdateCheck (false);
    }

    pollUpdateChecker();

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
    updateNetworkStatusIndicator();
}

int TriggerContentComponent::getNumRows()
{
    return (int) displayRows_.size();
}

void TriggerContentComponent::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    juce::ignoreUnused (selected);
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
        if (fired)
            fill = kClipFired;
        else
            fill = clipIdleFillFor (clip.isCustom, clip.hasOffset, clip.include);

        auto rr = juce::Rectangle<float> (0.0f, 1.0f, (float) width, (float) juce::jmax (0, height - 2));
        g.setColour (fill);
        g.fillRoundedRectangle (rr, 5.0f);
        if (! fired && (clip.connected || clip.testHighlight))
        {
            const auto borderColour = clipConnectedFillFor (clip.isCustom, clip.hasOffset).brighter (0.7f);
            g.setColour (borderColour);
            g.drawRoundedRectangle (rr.reduced (1.0f), 5.0f, 2.0f);
        }
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
            g.setColour (kArrowCollapsed);
            g.drawEllipse (b, 1.4f);
            g.setColour (kInput);
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
            if (trigger::model::isCustomLayer (dr.layer)) return;
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

        g.setColour (enabled ? kTextPrimary : juce::Colour::fromRGB (0xa3, 0xa3, 0xa3));
        g.setFont (juce::FontOptions (kGroupHeaderFontSize).withStyle ("bold"));
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
            g.setColour (fired ? kTextDarkActive : kBadgeBorder);
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setColour (fired ? kTextDarkActive : juce::Colour::fromRGB (0xc8, 0xc8, 0xc8));
            g.setFont (juce::FontOptions (kBadgeFontSize).withStyle ("Bold"));
            g.drawFittedText (badgeText, badge.getSmallestIntegerContainer(), juce::Justification::centred, 1);
            return;
        }
        case 2:
        {
            auto b = juce::Rectangle<float> (12.0f, 10.0f, 20.0f, (float) height - 20.0f).withSizeKeepingCentre (20.0f, 20.0f);
            g.setColour (kArrowCollapsed);
            g.drawEllipse (b, 1.4f);
            g.setColour (kInput);
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
        case 10: text = "";
            break;
        case 11: text = "";
            break;
        case 12: text = "";
            break;
        default: break;
    }
    juce::Colour textColour = juce::Colours::white.withAlpha (0.9f);
    if (columnId == 4)
    {
        const bool fired = (currentTriggerKeys_.find ({ it.layer, it.clip }) != currentTriggerKeys_.end());
        if (! it.include) textColour = kTextDisabled;
        else if (fired)    textColour = kTextAmberDark;
        else               textColour = kTextPrimary;
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
    auto withPopup = [this, rowNumber] (juce::Component* component) -> juce::Component*
    {
        if (component != nullptr)
            tagPopupComponentTree (*component, static_cast<juce::Component&> (*this), rowNumber);
        return component;
    };
    if (dr.isGroup)
    {
        if (trigger::model::isCustomLayer (dr.layer))
        {
            const int groupId = trigger::model::groupIdFromLayer (dr.layer);
            const auto* group = findCustomGroupById (groupId);
            if (group == nullptr)
                return nullptr;

            if (columnId == 10)
            {
                auto* btn = dynamic_cast<InlineAddButtonCell*> (existing);
                if (btn == nullptr) { if (existing != nullptr) existing->setVisible (false); btn = new InlineAddButtonCell(); }
                btn->onPress = [this, groupId] { addCustomColTriggerToGroup (groupId); };
                return withPopup (btn);
            }
            if (columnId == 11)
            {
                auto* btn = dynamic_cast<InlineDeleteButtonCell*> (existing);
                if (btn == nullptr) { if (existing != nullptr) existing->setVisible (false); btn = new InlineDeleteButtonCell(); }
                btn->onPress = [this, groupId] { deleteCustomGroup (groupId); };
                return withPopup (btn);
            }
            if (columnId == 3)
            {
                auto* ed = dynamic_cast<InlineTextCell*> (existing);
                if (ed == nullptr) { if (existing != nullptr) existing->setVisible (false); ed = new InlineTextCell(); }
                const bool enabled = layerEnabled_[dr.layer];
                const juce::Colour textCol = enabled ? kTextPrimary
                                                     : juce::Colour::fromRGB (0x9a, 0x9a, 0xa5);
                ed->applyColourToAllText (textCol, true);
                ed->applyFontToAllText (juce::Font (juce::FontOptions (kGroupHeaderFontSize).withStyle ("bold")), true);
                ed->setText (group->name, juce::dontSendNotification);
                ed->onCommit = [this, groupId] (const juce::String& v)
                {
                    if (auto* target = findCustomGroupById (groupId))
                    {
                        target->name = v.trim().isNotEmpty() ? v.trim()
                                                             : juce::String ("Custom Group " + juce::String (groupId));
                        for (auto& clip : triggerRows_)
                            if (clip.isCustom && clip.customGroupId == groupId)
                                clip.layerName = target->name;
                    }
                    triggerTable_.repaint();
                };
                return withPopup (ed);
            }
            if (columnId == 9)
            {
                auto* combo = dynamic_cast<InlineSendTargetCell*> (existing);
                if (combo == nullptr) { if (existing != nullptr) existing->setVisible (false); combo = new InlineSendTargetCell(); }
                bool hasAny = false;
                int sharedIndex = 0;
                for (const auto& clip : triggerRows_)
                {
                    if (clip.layer != dr.layer)
                        continue;
                    if (! hasAny)
                    {
                        sharedIndex = clip.sendTargetIndex;
                        hasAny = true;
                    }
                    else if (clip.sendTargetIndex != sharedIndex)
                    {
                        sharedIndex = 0;
                        break;
                    }
                }
                combo->setChoices (juce::jlimit (1, 8, resolumeSendTargetCount_), clampSendTargetIndex (sharedIndex));
                combo->onSelectionChanged = [this, rowNumber] (int selectedId)
                {
                    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                    const auto& dr2 = displayRows_[(size_t) rowNumber];
                    if (! dr2.isGroup) return;
                    const int sendIndex = clampSendTargetIndex (selectedId - 1);
                    for (auto& c : triggerRows_)
                        if (c.layer == dr2.layer)
                            c.sendTargetIndex = sendIndex;
                    refreshTriggerTableContent();
                };
                return withPopup (combo);
            }
            // Col 5 (Range) and col 8 (End Action) fall through; all other cols hide stale components
            if (columnId != 5 && columnId != 8)
            {
                if (existing != nullptr) existing->setVisible (false);
                return nullptr;
            }
        }

        if (columnId == 5)
        {
            // Editable Range field on the group header row — syncs to all clips in the layer
            auto* cell = dynamic_cast<InlineRangeCell*> (existing);
            if (cell == nullptr)
            {
                if (existing != nullptr) existing->setVisible (false);
                cell = new InlineRangeCell();
            }

            const bool enabled = layerEnabled_[dr.layer];
            const juce::Colour textCol = enabled ? kTextPrimary
                                                 : juce::Colour::fromRGB (0x9a, 0x9a, 0xa5);
            const juce::Font cellFont (juce::FontOptions (kGroupHeaderFontSize).withStyle ("Bold"));
            cell->setTextAppearance (textCol, juce::Font (cellFont));

            double rangeVal = 0.0;
            juce::String rangeMode = "mid";
            for (const auto& row : triggerRows_)
                if (row.layer == dr.layer) { rangeVal = row.triggerRangeSec; rangeMode = row.triggerRangeMode; break; }
            cell->setState (rangeVal, rangeMode);

            cell->onRangeCommit = [this, rowNumber] (const juce::String& v)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                const auto& dr2 = displayRows_[(size_t) rowNumber];
                if (! dr2.isGroup) return;
                const double newRange = juce::jmax (0.0, v.trim().replaceCharacter (',', '.').getDoubleValue());
                for (auto& c : triggerRows_)
                    if (c.layer == dr2.layer)
                        c.triggerRangeSec = newRange;
                refreshTriggerTableContent();
            };
            cell->onModeChanged = [this, rowNumber] (const juce::String& mode)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                const auto& dr2 = displayRows_[(size_t) rowNumber];
                if (! dr2.isGroup) return;
                const auto normalised = normaliseTriggerRangeMode (mode);
                for (auto& c : triggerRows_)
                    if (c.layer == dr2.layer)
                        c.triggerRangeMode = normalised;
                refreshTriggerTableContent();
            };
            return withPopup (cell);
        }

        if (columnId == 9)
        {
            auto* combo = dynamic_cast<InlineSendTargetCell*> (existing);
            if (combo == nullptr) { if (existing != nullptr) existing->setVisible (false); combo = new InlineSendTargetCell(); }
            bool hasAny = false;
            int sharedIndex = 0;
            for (const auto& clip : triggerRows_)
            {
                if (clip.layer != dr.layer)
                    continue;
                if (! hasAny)
                {
                    sharedIndex = clip.sendTargetIndex;
                    hasAny = true;
                }
                else if (clip.sendTargetIndex != sharedIndex)
                {
                    sharedIndex = 0;
                    break;
                }
            }
            combo->setChoices (juce::jlimit (1, 8, resolumeSendTargetCount_), clampSendTargetIndex (sharedIndex));
            combo->onSelectionChanged = [this, rowNumber] (int selectedId)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                const auto& dr2 = displayRows_[(size_t) rowNumber];
                if (! dr2.isGroup) return;
                const int sendIndex = clampSendTargetIndex (selectedId - 1);
                for (auto& c : triggerRows_)
                    if (c.layer == dr2.layer)
                        c.sendTargetIndex = sendIndex;
                refreshTriggerTableContent();
            };
            return withPopup (combo);
        }

        if (columnId == 8)
        {
            auto* cell = dynamic_cast<InlineEndActionCell*> (existing);
            if (cell == nullptr) { if (existing != nullptr) existing->setVisible (false); cell = new InlineEndActionCell(); }

            // Detect shared End Action state across all clips in this layer
            juce::String sharedMode, sharedCol, sharedLayer, sharedClip;
            bool hasAny = false, mixed = false;
            for (const auto& c : triggerRows_)
            {
                if (c.layer != dr.layer) continue;
                const juce::String thisLayer = c.endActionMode == "gc" ? c.endActionGroup : c.endActionLayer;
                if (!hasAny)
                {
                    sharedMode  = c.endActionMode;
                    sharedCol   = c.endActionCol;
                    sharedLayer = thisLayer;
                    sharedClip  = c.endActionClip;
                    hasAny = true;
                }
                else if (c.endActionMode != sharedMode || c.endActionCol != sharedCol ||
                         thisLayer != sharedLayer || c.endActionClip != sharedClip)
                {
                    mixed = true;
                    break;
                }
            }
            juce::String groupTitle;
            for (const auto& c : triggerRows_)
                if (c.layer == dr.layer) { groupTitle = c.layerName; break; }

            if (mixed || !hasAny)
                cell->setState ({}, {}, {}, {}, groupTitle);
            else
                cell->setState (sharedMode, sharedCol, sharedLayer, sharedClip, groupTitle);

            cell->onChanged = [this, rowNumber] (const juce::String& mode,
                                                 const juce::String& col,
                                                 const juce::String& layer,
                                                 const juce::String& clipValue)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                const auto& dr2 = displayRows_[(size_t) rowNumber];
                if (! dr2.isGroup) return;
                for (auto& c : triggerRows_)
                {
                    if (c.layer != dr2.layer) continue;
                    c.endActionMode  = mode;
                    c.endActionCol   = (mode == "lc") ? juce::String() : col;
                    c.endActionGroup = (mode == "gc") ? layer : juce::String();
                    c.endActionLayer = (mode == "lc") ? layer : juce::String();
                    c.endActionClip  = (mode == "lc") ? clipValue : juce::String();
                }
                updateTableColumnWidths();
                refreshTriggerTableContent();
            };
            return withPopup (cell);
        }

        // For all other group-row columns, hide any stale clip-row component and remove it
        if (existing != nullptr)
            existing->setVisible (false);
        return nullptr;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return nullptr;

    auto& clip = triggerRows_[(size_t) dr.clipIndex];
    const bool fired = (currentTriggerKeys_.find ({ clip.layer, clip.clip }) != currentTriggerKeys_.end());

    if (columnId == 9)
    {
        auto* combo = dynamic_cast<InlineSendTargetCell*> (existing);
        if (combo == nullptr)
        {
            if (existing != nullptr) existing->setVisible (false);
            combo = new InlineSendTargetCell();
        }
        combo->setChoices (juce::jlimit (1, 8, resolumeSendTargetCount_), clampSendTargetIndex (clip.sendTargetIndex));
        combo->onSelectionChanged = [this, rowNumber] (int selectedId)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            triggerRows_[(size_t) dr2.clipIndex].sendTargetIndex = clampSendTargetIndex (selectedId - 1);
        };
        return withPopup (combo);
    }

    // ── Custom trigger clip row ──────────────────────────────────────────────
    if (clip.isCustom)
    {
        if (columnId == 12)
        {
            auto* btn = dynamic_cast<InlineTestButtonCell*> (existing);
            if (btn == nullptr) { if (existing != nullptr) existing->setVisible (false); btn = new InlineTestButtonCell(); }
            btn->onPress = [this, rowNumber]
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
                auto& c = triggerRows_[(size_t) dr2.clipIndex];
                fireCustomTrigger (c);
                for (auto& t : triggerRows_) t.testHighlight = false;
                c.testHighlight = true;
                refreshTriggerTableContent();
            };
            return withPopup (btn);
        }
        if (columnId == 10)
        {
            // Col / L-C selector widget (same pattern as End Action)
            auto* cell = dynamic_cast<InlineCustomTypeCell*> (existing);
            if (cell == nullptr) { if (existing != nullptr) existing->setVisible (false); cell = new InlineCustomTypeCell(); }
            cell->setState (clip.customType, clip.customSourceCol,
                            clip.customType == "gc" ? clip.customSourceGroup : clip.customSourceLayer,
                            clip.customSourceClip, clip.name);
            cell->setFired (fired);
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
                c.customSourceCol   = (type == "lc") ? juce::String() : col;   // col param = column in col/gc
                c.customSourceGroup = (type == "gc") ? layer : juce::String(); // layer param = group in gc
                c.customSourceLayer = (type == "lc") ? layer : juce::String();
                c.customSourceClip  = (type == "lc") ? clipVal : juce::String();
            };
            return withPopup (cell);
        }
        if (columnId == 11)
        {
            auto* btn = dynamic_cast<InlineDeleteButtonCell*> (existing);
            if (btn == nullptr) { if (existing != nullptr) existing->setVisible (false); btn = new InlineDeleteButtonCell(); }
            btn->onPress = [this, rowNumber]
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                deleteCustomTrigger (dr2.clipIndex);
            };
            return withPopup (btn);
        }
        // Cols 3, 5, 6, 7, 8 fall through to standard handlers below
    }

    if (columnId == 3 || columnId == 5 || columnId == 6 || columnId == 7)
    {
        if (columnId == 5)
        {
            auto* cell = dynamic_cast<InlineRangeCell*> (existing);
            if (cell == nullptr)
            {
                if (existing != nullptr) existing->setVisible (false);
                cell = new InlineRangeCell();
            }

            juce::Colour textCol = kTextPrimary;
            if (! clip.include)      textCol = kTextDisabled;
            else if (fired)          textCol = kTextAmberDark;
            cell->setTextAppearance (textCol, juce::Font (clipRowFont()));
            cell->setState (clip.triggerRangeSec, clip.triggerRangeMode);
            cell->onRangeCommit = [this, rowNumber] (const juce::String& v)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
                auto& c = triggerRows_[(size_t) dr2.clipIndex];
                c.triggerRangeSec = juce::jmax (0.0, v.trim().replaceCharacter (',', '.').getDoubleValue());
                triggerTable_.repaint();
            };
            cell->onModeChanged = [this, rowNumber] (const juce::String& mode)
            {
                if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
                auto dr2 = displayRows_[(size_t) rowNumber];
                if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
                auto& c = triggerRows_[(size_t) dr2.clipIndex];
                c.triggerRangeMode = normaliseTriggerRangeMode (mode);
                triggerTable_.repaint();
            };
            return withPopup (cell);
        }

        auto* ed = dynamic_cast<InlineTextCell*> (existing);
        if (ed == nullptr)
        {
            if (existing != nullptr) existing->setVisible (false);
            ed = new InlineTextCell();
        }

        juce::Colour textCol = kTextPrimary;
        if (! clip.include)      textCol = kTextDisabled;
        else if (fired)          textCol = kTextAmberDark;
        // applyColourToAllText/applyFontToAllText update existing sections immediately;
        // setColour(textColourId) alone only affects future inserts and is a no-op for existing text.
        const juce::Font cellFont (clipRowFont());
        ed->applyColourToAllText (textCol, true);
        ed->applyFontToAllText (cellFont, true);

        if (columnId == 3) ed->setText (clip.name, juce::dontSendNotification);
        if (columnId == 6) ed->setText (clip.triggerTc, juce::dontSendNotification);
        if (columnId == 7) ed->setText (clip.durationTc, juce::dontSendNotification);
        ed->onCommit = [this, rowNumber, columnId] (const juce::String& v)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            if (columnId == 3) c.name = v.trim();
            if (columnId == 6) c.triggerTc = v.trim();
            if (columnId == 7) c.durationTc = v.trim();
            triggerTable_.repaint();
        };
        return withPopup (ed);
    }

    if (columnId == 8)
    {
        auto* cell = dynamic_cast<InlineEndActionCell*> (existing);
        if (cell == nullptr)
        {
            if (existing != nullptr) existing->setVisible (false);
            cell = new InlineEndActionCell();
        }
        cell->setState (clip.endActionMode, clip.endActionCol,
                        clip.endActionMode == "gc" ? clip.endActionGroup : clip.endActionLayer,
                        clip.endActionClip, clip.name);
        cell->setFired (fired);
        cell->onChanged = [this, rowNumber] (const juce::String& mode,
                                             const juce::String& col,
                                             const juce::String& layer,
                                             const juce::String& clipValue)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            c.endActionMode  = mode;
            c.endActionCol   = (mode == "lc") ? juce::String() : col;    // col param = column in col/gc
            c.endActionGroup = (mode == "gc") ? layer : juce::String();  // layer param = group in gc
            c.endActionLayer = (mode == "lc") ? layer : juce::String();
            c.endActionClip  = (mode == "lc") ? clipValue : juce::String();
            updateTableColumnWidths();
            refreshTriggerTableContent();
        };
        return withPopup (cell);
    }

    const bool hasCustomColumns = triggerTable_.getHeader().getIndexOfColumnId (12, false) >= 0;
    const int testColumnId = hasCustomColumns ? 12 : 10;
    if (columnId == testColumnId)
    {
        auto* btn = dynamic_cast<InlineTestButtonCell*> (existing);
        if (btn == nullptr)
        {
            if (existing != nullptr) existing->setVisible (false);
            btn = new InlineTestButtonCell();
        }
        btn->onPress = [this, rowNumber]
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            sendTestTrigger (c.layer, c.clip, c.sendTargetIndex);
            for (auto& t : triggerRows_) t.testHighlight = false;
            c.testHighlight = true;

            // Schedule end action (same logic as evaluateAndFireTriggers)
            const juce::String eMode = c.endActionMode.trim().toLowerCase();
            if (eMode == "col" || eMode == "lc" || eMode == "gc")
            {
                int durFrames = 0;
                if (parseTcToFrames (c.durationTc, 25, durFrames) && durFrames > 0)
                {
                    const double durSec = (double) durFrames / 25.0;
                    PendingEndAction ea;
                    ea.executeTs = juce::Time::getMillisecondCounterHiRes() * 0.001 + durSec;
                    ea.mode  = eMode;
                    ea.col   = c.endActionCol;
                    ea.group = c.endActionGroup;
                    ea.layer = c.endActionLayer;
                    ea.clip  = c.endActionClip;
                    pendingEndActions_[{ c.layer, c.clip }] = ea;
                }
            }
            else
            {
                pendingEndActions_.erase ({ c.layer, c.clip });
            }

            refreshTriggerTableContent();
        };
        return withPopup (btn);
    }
    if (existing != nullptr)
        existing->setVisible (false);
    return nullptr;
}

void TriggerContentComponent::cellClicked (int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return;

    if (event.mods.isPopupMenu())
    {
        showRowContextMenu (rowNumber);
        return;
    }

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
        syncCustomGroupStateFromLayers();
        rebuildDisplayRows();
        refreshTriggerTableContent();
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
        syncCustomGroupStateFromLayers();
        triggerTable_.repaint();
        return;
    }
    juce::ignoreUnused (columnId);
}

void TriggerContentComponent::cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    juce::ignoreUnused (rowNumber, columnId);
}

void TriggerContentComponent::showRowContextMenu (int rowNumber)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return;

    const auto& dr = displayRows_[(size_t) rowNumber];
    if (dr.isGroup)
    {
        const bool isCustomGroup = trigger::model::isCustomLayer (dr.layer);
        const int groupId = isCustomGroup ? trigger::model::groupIdFromLayer (dr.layer) : 0;

        juce::PopupMenu menu;
        menu.addItem (1, "Move Group Up");
        menu.addItem (2, "Move Group Down");
        menu.addSeparator();
        menu.addItem (3, "Delete Group");

        menu.showMenuAsync (juce::PopupMenu::Options(),
                            [safe = juce::Component::SafePointer<TriggerContentComponent> (this), layer = dr.layer, groupId, isCustomGroup] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1) safe->moveLayerGroup (layer, -1);
                                if (result == 2) safe->moveLayerGroup (layer, 1);
                                if (result == 3)
                                {
                                    if (isCustomGroup) safe->deleteCustomGroup (groupId);
                                    else               safe->deleteLayerGroup (layer);
                                }
                            });
        return;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return;

    const auto& clip = triggerRows_[(size_t) dr.clipIndex];
    juce::PopupMenu menu;
    menu.addItem (1, "Move Up");
    menu.addItem (2, "Move Down");
    menu.addSeparator();
    menu.addItem (3, "Delete Trigger");

    menu.showMenuAsync (juce::PopupMenu::Options(),
                        [safe = juce::Component::SafePointer<TriggerContentComponent> (this), clipIndex = dr.clipIndex] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            if (result == 1) safe->moveClipRow (clipIndex, -1);
                            if (result == 2) safe->moveClipRow (clipIndex, 1);
                            if (result == 3) safe->deleteTriggerRow (clipIndex);
                        });
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
    settingsPanel_.viewport().getVerticalScrollBar().setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
    settingsPanel_.viewport().getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
    settingsPanel_.viewport().getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));

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
        e.setColour (juce::TextEditor::textColourId, kTextMuted);
        e.setJustification (juce::Justification::centredLeft);
        e.setIndents (8, 0);
    };
    for (auto* e : { &resolumeSendIp_, &resolumeSendPort_,
                     &resolumeSendIp2_, &resolumeSendPort2_,
                     &resolumeSendIp3_, &resolumeSendPort3_,
                     &resolumeSendIp4_, &resolumeSendPort4_,
                     &resolumeSendIp5_, &resolumeSendPort5_,
                     &resolumeSendIp6_, &resolumeSendPort6_,
                     &resolumeSendIp7_, &resolumeSendPort7_,
                     &resolumeSendIp8_, &resolumeSendPort8_,
                     &resolumeListenIp_, &resolumeListenPort_,
                     &resolumeMaxLayers_, &resolumeMaxClips_, &resolumeGlobalOffset_ })
    {
        styleEditor (*e);
    }
    for (auto* l : { &resolumeHeader_,
                     &triggerOutHeaderLabel_,
                     &resAdapterLbl_,
                     &resSendIpLbl_, &resSendIpLbl2_, &resSendIpLbl3_, &resSendIpLbl4_, &resSendIpLbl5_, &resSendIpLbl6_, &resSendIpLbl7_, &resSendIpLbl8_,
                     &resSendAdapterLbl1_, &resSendAdapterLbl2_, &resSendAdapterLbl3_, &resSendAdapterLbl4_, &resSendAdapterLbl5_, &resSendAdapterLbl6_, &resSendAdapterLbl7_, &resSendAdapterLbl8_,
                     &resSendPortLbl_, &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_, &resGlobalOffsetLbl_ })
    {
        styleValueLabel (*l);
        l->setJustificationType (juce::Justification::centredLeft);
    }
    for (auto* h : { &resolumeHeader_, &triggerOutHeaderLabel_ })
    {
        h->setColour (juce::Label::textColourId, kTextSecondary);
        h->setFont (juce::FontOptions (kSectionHeaderFontSize));
        h->setJustificationType (juce::Justification::centredLeft);
    }
    styleRowButton (getTriggersBtn_, row_);
    styleRowButton (createCustomBtn_, row_);
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

        if (deletedLayers_.count (c.layer) > 0)
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
        row.triggerRangeMode = "mid";
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
            row.triggerRangeMode = normaliseTriggerRangeMode (old.triggerRangeMode);
            row.endActionMode = old.endActionMode;
            row.endActionCol = old.endActionCol;
            row.endActionLayer = old.endActionLayer;
            row.endActionClip = old.endActionClip;
            row.endActionGroup = old.endActionGroup;
            row.sendTargetIndex = old.sendTargetIndex;
            row.orderIndex = old.orderIndex;
            row.testHighlight = old.testHighlight;
        }
        else
        {
            row.orderIndex = c.clip - 1;
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

    normaliseLayerOrder();
    rebuildDisplayRows();
    refreshTriggerTableContent();
}

void TriggerContentComponent::syncTriggerRowFeedbackFromCollector()
{
    const auto clips = clipCollector_.snapshot();
    std::map<std::pair<int, int>, bool> connectedByKey;
    bool hasLiveConnectedClip = false;
    for (const auto& c : clips)
    {
        connectedByKey[{ c.layer, c.clip }] = c.connected;
        hasLiveConnectedClip = hasLiveConnectedClip || c.connected;
    }

    bool changed = false;
    for (auto& row : triggerRows_)
    {
        if (row.isCustom)
            continue;

        const bool nextConnected = connectedByKey[{ row.layer, row.clip }];
        if (row.connected != nextConnected)
        {
            row.connected = nextConnected;
            changed = true;
        }

        if (hasLiveConnectedClip && row.testHighlight)
        {
            row.testHighlight = false;
            changed = true;
        }
    }

    if (changed)
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
        const bool inRangeWindow = isFrameInsideTriggerWindow (currentFrames, triggerFrames,
                                                               juce::jmax (1, (int) std::round (t.triggerRangeSec * fps)),
                                                               t.triggerRangeMode);
        const bool inDurationWindow = isFrameInsideClipDurationWindow (currentFrames, triggerFrames, t.durationTc, fps);
        t.timecodeHit = inRangeWindow || inDurationWindow;
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
    const int frameJump = currentFrames - prevFrames;

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
        refreshTriggerTableContent();
        return;
    }

    // Treat a large forward seek like a re-arm event: we only want cues at/near the
    // landed position, not every cue window crossed between the old and new timecode.
    const bool largeForwardJump = frameJump > fps * 2;
    if (largeForwardJump)
    {
        triggerRangeActive_.clear();
        currentTriggerKeys_.clear();
        pendingEndActions_.clear();
        for (auto& t : triggerRows_)
            t.connected = false;
    }

    struct Candidate
    {
        int index { -1 };
        int score { (std::numeric_limits<int>::max)() };
        bool isCrossOnly { false };
        bool isDurationActive { false };
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
        const auto [winStart, winEnd] = getTriggerWindowBounds (trig, range, t.triggerRangeMode);
        const bool inRangeNow = currentFrames >= winStart && currentFrames <= winEnd;
        const bool inDurationNow = isFrameInsideClipDurationWindow (currentFrames, trig, t.durationTc, fps);
        const bool inNow = inRangeNow || inDurationNow;
        const bool wasIn = triggerRangeActive_.count (key) > 0 ? triggerRangeActive_[key] : false;
        newRangeState[key] = inNow;

        bool crossed = false;
        if (! largeForwardJump && prevFrames != currentFrames)
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
            best.isDurationActive = inDurationNow;
            continue;
        }

        const int score = std::abs (currentFrames - trig);
        const bool crossOnly = ! inNow;
        if (! best.isDurationActive && inDurationNow)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = crossOnly;
            best.isDurationActive = true;
        }
        else if (best.isDurationActive == inDurationNow && best.isCrossOnly && ! crossOnly)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = crossOnly;
            best.isDurationActive = inDurationNow;
        }
        else if (best.isDurationActive == inDurationNow && best.isCrossOnly == crossOnly && score < best.score)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = crossOnly;
            best.isDurationActive = inDurationNow;
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
            sendTestTrigger (t.layer, t.clip, t.sendTargetIndex);
        for (auto& x : triggerRows_)
        {
            if (x.layer == t.layer)
                x.connected = false;
            x.testHighlight = false;
        }
        t.connected = true;

        // Schedule end action (Col / L-C / G-C mode), mirroring Python _schedule_end_action_for_trigger
        {
            const juce::String eMode = t.endActionMode.trim().toLowerCase();
            if (eMode == "col" || eMode == "lc" || eMode == "gc")
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
                    ea.group     = t.endActionGroup;
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
    refreshTriggerTableContent();
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
        else if (ea.mode == "gc")
        {
            const int grp = ea.group.getIntValue();
            const int col = ea.col.getIntValue();
            if (grp > 0 && col > 0)
                addr = "/composition/groups/" + juce::String (grp) + "/columns/" + juce::String (col) + "/connect";
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
            // Normal mode (Thru OFF) → stop thru; only open LTC output when enabled.
            bridgeEngine_.stopLtcThru();
            if (enabled)
            {
                bridgeEngine_.startLtcOutput (choice, channel, sampleRate, bufferSize, err);
                bridgeEngine_.setLtcOutputEnabled (true);
                if (err.isNotEmpty())
                    juce::MessageManager::callAsync ([safeThis, err]
                    {
                        if (safeThis != nullptr)
                            safeThis->setTimecodeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
                    });
            }
            else
            {
                bridgeEngine_.stopLtcOutput();
                bridgeEngine_.setLtcOutputEnabled (false);
            }
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
    const int sourceId = sourceCombo_.getSelectedId();

    if (sourceId == 1)
    {
        bridgeEngine_.stopMtcInput();
        bridgeEngine_.stopArtnetInput();
        bridgeEngine_.stopOscInput();

        const int ltcSelectedId = ltcInDeviceCombo_.getSelectedId();
        const int ltcIdx = ltcSelectedId > 0 ? ltcSelectedId - 1 : -1;
        if (juce::isPositiveAndBelow (ltcIdx, filteredInputIndices_.size()))
            bridgeEngine_.startLtcInput (inputChoices_[filteredInputIndices_[ltcIdx]],
                                         comboChannelIndex (ltcInChannelCombo_),
                                         comboSampleRate (ltcInSampleRateCombo_),
                                         0,
                                         err);
    }
    else if (sourceId == 2)
    {
        bridgeEngine_.stopLtcInput();
        bridgeEngine_.stopArtnetInput();
        bridgeEngine_.stopOscInput();

        if (mtcInCombo_.getNumItems() > 0)
            bridgeEngine_.startMtcInput (mtcInCombo_.getSelectedItemIndex(), err);
    }
    else if (sourceId == 3)
    {
        bridgeEngine_.stopLtcInput();
        bridgeEngine_.stopMtcInput();
        bridgeEngine_.stopOscInput();

        if (artnetInCombo_.getNumItems() > 0)
        {
            const auto artnetListenIp = artnetListenIpEditor_.getText().trim();
            bridgeEngine_.startArtnetInput (artnetInCombo_.getSelectedItemIndex(),
                                            (artnetListenIp == "0.0.0.0" ? juce::String() : artnetListenIp),
                                            err);
        }
    }
    else
    {
        bridgeEngine_.stopLtcInput();
        bridgeEngine_.stopMtcInput();
        bridgeEngine_.stopArtnetInput();

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
    }

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
    scanThread_->tempManager_ = std::make_unique<juce::AudioDeviceManager>();
    scanThread_->tempManager_->initialise (128, 128, nullptr, false);
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
    resolumeSendAdapterCombo6_.clear();
    resolumeSendAdapterCombo7_.clear();
    resolumeSendAdapterCombo8_.clear();
    oscAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo1_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo2_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo3_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo4_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo5_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo6_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo7_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    resolumeSendAdapterCombo8_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
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
        resolumeSendAdapterCombo6_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo7_.addItem (networkAdapterItems[i], id);
        resolumeSendAdapterCombo8_.addItem (networkAdapterItems[i], id);
    }
    if (artnetInCombo_.getNumItems() > 0)
        artnetInCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (oscAdapterCombo_.getNumItems() > 0)
        oscAdapterCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (resolumeAdapterCombo_.getNumItems() > 0)
        resolumeAdapterCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    for (auto* c : { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_, &resolumeSendAdapterCombo6_, &resolumeSendAdapterCombo7_, &resolumeSendAdapterCombo8_ })
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
        {
            auto& choice = inputChoices_.getReference (realIdx);
            if (choice.channelCount <= 0)
                choice.channelCount = bridge::engine::BridgeEngine::queryAudioChannelCount (choice, true);
            inputChannelCount = choice.channelCount;
        }
    }
    refill (ltcInChannelCombo_, inputChannelCount);

    int outputChannelCount = 0;
    const int outSelectedId = ltcOutDeviceCombo_.getSelectedId();
    const int outIdx = outSelectedId > 0 ? outSelectedId - 1 : -1;
    if (juce::isPositiveAndBelow (outIdx, filteredOutputIndices_.size()))
    {
        const int realIdx = filteredOutputIndices_[outIdx];
        if (juce::isPositiveAndBelow (realIdx, outputChoices_.size()))
        {
            auto& choice = outputChoices_.getReference (realIdx);
            if (choice.channelCount <= 0)
                choice.channelCount = bridge::engine::BridgeEngine::queryAudioChannelCount (choice, false);
            outputChannelCount = choice.channelCount;
        }
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
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_, &resolumeSendExpanded6_, &resolumeSendExpanded7_, &resolumeSendExpanded8_ };
    ExpandCircleButton* buttons[] = { &resolumeSendExpandBtn1_, &resolumeSendExpandBtn2_, &resolumeSendExpandBtn3_, &resolumeSendExpandBtn4_, &resolumeSendExpandBtn5_, &resolumeSendExpandBtn6_, &resolumeSendExpandBtn7_, &resolumeSendExpandBtn8_ };

    if (! juce::isPositiveAndBelow (index, 8))
        return;

    *expandedStates[(size_t) index] = ! *expandedStates[(size_t) index];
    buttons[(size_t) index]->setExpanded (*expandedStates[(size_t) index]);
    updateWindowHeight();
    resized();
    repaint();
}

void TriggerContentComponent::addResolumeSendTarget()
{
    if (resolumeSendTargetCount_ >= 8)
        return;

    auto adapters = std::array<juce::ComboBox*, 8> { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_, &resolumeSendAdapterCombo6_, &resolumeSendAdapterCombo7_, &resolumeSendAdapterCombo8_ };
    auto ips = std::array<juce::TextEditor*, 8> { &resolumeSendIp_, &resolumeSendIp2_, &resolumeSendIp3_, &resolumeSendIp4_, &resolumeSendIp5_, &resolumeSendIp6_, &resolumeSendIp7_, &resolumeSendIp8_ };
    auto ports = std::array<juce::TextEditor*, 8> { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_, &resolumeSendPort6_, &resolumeSendPort7_, &resolumeSendPort8_ };
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_, &resolumeSendExpanded6_, &resolumeSendExpanded7_, &resolumeSendExpanded8_ };
    const int src = juce::jmax (0, resolumeSendTargetCount_ - 1);
    adapters[(size_t) resolumeSendTargetCount_]->setText (adapters[(size_t) src]->getText(), juce::dontSendNotification);
    ips[(size_t) resolumeSendTargetCount_]->setText (ips[(size_t) src]->getText(), juce::dontSendNotification);
    ports[(size_t) resolumeSendTargetCount_]->setText (ports[(size_t) src]->getText(), juce::dontSendNotification);
    *expandedStates[(size_t) resolumeSendTargetCount_] = false;
    ++resolumeSendTargetCount_;
    refreshTriggerTableContent();
    resized();
    repaint();
}

void TriggerContentComponent::removeResolumeSendTarget (int targetIndex)
{
    if (targetIndex <= 0 || targetIndex >= resolumeSendTargetCount_ || targetIndex > 7)
        return;

    auto adapters = std::array<juce::ComboBox*, 8> { &resolumeSendAdapterCombo1_, &resolumeSendAdapterCombo2_, &resolumeSendAdapterCombo3_, &resolumeSendAdapterCombo4_, &resolumeSendAdapterCombo5_, &resolumeSendAdapterCombo6_, &resolumeSendAdapterCombo7_, &resolumeSendAdapterCombo8_ };
    auto ips = std::array<juce::TextEditor*, 8> { &resolumeSendIp_, &resolumeSendIp2_, &resolumeSendIp3_, &resolumeSendIp4_, &resolumeSendIp5_, &resolumeSendIp6_, &resolumeSendIp7_, &resolumeSendIp8_ };
    auto ports = std::array<juce::TextEditor*, 8> { &resolumeSendPort_, &resolumeSendPort2_, &resolumeSendPort3_, &resolumeSendPort4_, &resolumeSendPort5_, &resolumeSendPort6_, &resolumeSendPort7_, &resolumeSendPort8_ };
    bool* expandedStates[] = { &resolumeSendExpanded1_, &resolumeSendExpanded2_, &resolumeSendExpanded3_, &resolumeSendExpanded4_, &resolumeSendExpanded5_, &resolumeSendExpanded6_, &resolumeSendExpanded7_, &resolumeSendExpanded8_ };
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
    for (auto& clip : triggerRows_)
        clip.sendTargetIndex = clampSendTargetIndex (clip.sendTargetIndex);
    refreshTriggerTableContent();
    resized();
    repaint();
}

int TriggerContentComponent::clampSendTargetIndex (int index) const
{
    return juce::jlimit (0, juce::jlimit (1, 8, resolumeSendTargetCount_), index);
}

void TriggerContentComponent::populateSendTargetCombo (juce::ComboBox& combo, int selectedIndex) const
{
    combo.clear (juce::dontSendNotification);
    combo.addItem ("All", 1);
    const int maxTargets = juce::jlimit (1, 8, resolumeSendTargetCount_);
    for (int i = 1; i <= maxTargets; ++i)
        combo.addItem ("Send " + juce::String (i), i + 1);

    const int clamped = clampSendTargetIndex (selectedIndex);
    combo.setSelectedId (clamped + 1, juce::dontSendNotification);
}

std::vector<TriggerContentComponent::ResolumeSendTarget> TriggerContentComponent::collectResolumeSendTargets() const
{
    return collectResolumeSendTargets (0);
}

std::optional<TriggerContentComponent::ResolumeSendTarget> TriggerContentComponent::getConfiguredResolumeSendTarget (int oneBasedIndex) const
{
    if (oneBasedIndex < 1 || oneBasedIndex > juce::jlimit (1, 8, resolumeSendTargetCount_))
        return std::nullopt;

    std::vector<ResolumeSendTarget> targets;
    targets.reserve (1);

    auto addTarget = [&targets] (const juce::ComboBox& adapterCombo, const juce::TextEditor& ipEd, const juce::TextEditor& portEd)
    {
        ResolumeSendTarget target;
        target.localBindIp = parseBindIpFromAdapterLabel (adapterCombo.getText());
        target.ip = ipEd.getText().trim();
        if (target.ip.isEmpty())
            target.ip = "127.0.0.1";
        target.port = juce::jlimit (1, 65535, portEd.getText().trim().getIntValue());
        targets.emplace_back (target);
    };

    switch (oneBasedIndex)
    {
        case 1: addTarget (resolumeSendAdapterCombo1_, resolumeSendIp_,  resolumeSendPort_); break;
        case 2: addTarget (resolumeSendAdapterCombo2_, resolumeSendIp2_, resolumeSendPort2_); break;
        case 3: addTarget (resolumeSendAdapterCombo3_, resolumeSendIp3_, resolumeSendPort3_); break;
        case 4: addTarget (resolumeSendAdapterCombo4_, resolumeSendIp4_, resolumeSendPort4_); break;
        case 5: addTarget (resolumeSendAdapterCombo5_, resolumeSendIp5_, resolumeSendPort5_); break;
        case 6: addTarget (resolumeSendAdapterCombo6_, resolumeSendIp6_, resolumeSendPort6_); break;
        case 7: addTarget (resolumeSendAdapterCombo7_, resolumeSendIp7_, resolumeSendPort7_); break;
        case 8: addTarget (resolumeSendAdapterCombo8_, resolumeSendIp8_, resolumeSendPort8_); break;
        default: break;
    }

    if (targets.empty())
        return std::nullopt;
    return targets.front();
}

std::vector<TriggerContentComponent::ResolumeSendTarget> TriggerContentComponent::collectResolumeSendTargets (int sendTargetIndex) const
{
    std::vector<ResolumeSendTarget> targets;
    targets.reserve ((size_t) juce::jlimit (1, 8, resolumeSendTargetCount_));

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

    const int clampedSelection = clampSendTargetIndex (sendTargetIndex);
    auto addByIndex = [this, &addTarget] (int index)
    {
        switch (index)
        {
            case 1: addTarget (resolumeSendAdapterCombo1_, resolumeSendIp_,  resolumeSendPort_); break;
            case 2: addTarget (resolumeSendAdapterCombo2_, resolumeSendIp2_, resolumeSendPort2_); break;
            case 3: addTarget (resolumeSendAdapterCombo3_, resolumeSendIp3_, resolumeSendPort3_); break;
            case 4: addTarget (resolumeSendAdapterCombo4_, resolumeSendIp4_, resolumeSendPort4_); break;
            case 5: addTarget (resolumeSendAdapterCombo5_, resolumeSendIp5_, resolumeSendPort5_); break;
            case 6: addTarget (resolumeSendAdapterCombo6_, resolumeSendIp6_, resolumeSendPort6_); break;
            case 7: addTarget (resolumeSendAdapterCombo7_, resolumeSendIp7_, resolumeSendPort7_); break;
            case 8: addTarget (resolumeSendAdapterCombo8_, resolumeSendIp8_, resolumeSendPort8_); break;
            default: break;
        }
    };

    if (clampedSelection > 0)
    {
        addByIndex (clampedSelection);
        return targets;
    }

    addByIndex (1);
    if (resolumeSendTargetCount_ >= 2) addByIndex (2);
    if (resolumeSendTargetCount_ >= 3) addByIndex (3);
    if (resolumeSendTargetCount_ >= 4) addByIndex (4);
    if (resolumeSendTargetCount_ >= 5) addByIndex (5);
    if (resolumeSendTargetCount_ >= 6) addByIndex (6);
    if (resolumeSendTargetCount_ >= 7) addByIndex (7);
    if (resolumeSendTargetCount_ >= 8) addByIndex (8);
    return targets;
}

juce::String TriggerContentComponent::makeResolumeSendTargetKey (const ResolumeSendTarget& target)
{
    return target.localBindIp.trim() + "|" + target.ip.trim() + "|" + juce::String (target.port);
}

juce::String TriggerContentComponent::describeResolumeSendTarget (const ResolumeSendTarget& target)
{
    return target.ip.trim() + ":" + juce::String (target.port);
}

void TriggerContentComponent::updateSendTargetRuntimeState (const ResolumeSendTarget& target, bool ok, const juce::String& detail)
{
    auto& state = sendTargetRuntimeStates_[makeResolumeSendTargetKey (target)];
    state.ok = ok;
    state.detail = detail;
    if (ok)
        state.lastTxMs = juce::Time::currentTimeMillis();
}

bool TriggerContentComponent::sendOscPulse (const ResolumeSendTarget& target, const juce::String& addr)
{
    juce::DatagramSocket socket (true);
    const auto bindIp = target.localBindIp.trim().isNotEmpty() ? target.localBindIp.trim() : juce::String ("0.0.0.0");
    bool bound = false;
    if (bindIp != "0.0.0.0")
        bound = socket.bindToPort (0, bindIp);
    else
        bound = socket.bindToPort (0);
    if (! bound)
    {
        updateSendTargetRuntimeState (target, false, "bind failed (" + bindIp + ")");
        oscSendOk_ = false;
        oscSendState_ = "error";
        oscSendDetail_ = "send bind failed on " + bindIp;
        updateNetworkStatusIndicator();
        return false;
    }

    juce::OSCSender sender;
    if (! sender.connectToSocket (socket, target.ip, target.port))
    {
        updateSendTargetRuntimeState (target, false, "connect failed (" + describeResolumeSendTarget (target) + ")");
        oscSendOk_ = false;
        oscSendState_ = "error";
        oscSendDetail_ = "send connect failed to " + target.ip + ":" + juce::String (target.port);
        updateNetworkStatusIndicator();
        return false;
    }

    juce::OSCMessage on (addr);  on.addInt32 (1);
    juce::OSCMessage off (addr); off.addInt32 (0);
    const bool ok = sender.send (on) && sender.send (off);

    if (ok)
    {
        updateSendTargetRuntimeState (target, true, "ok (" + describeResolumeSendTarget (target) + ")");
        lastOscOutMs_ = juce::Time::currentTimeMillis();
        lastOscOutPort_ = target.port;
        oscSendOk_ = true;
        oscSendState_ = "ok";
        oscSendDetail_ = "send " + target.ip + ":" + juce::String (target.port);
        updateNetworkStatusIndicator();
        OscLogEntry e;
        e.dir         = OscLogEntry::Dir::output;
        e.timestampMs = juce::Time::currentTimeMillis();
        e.ip          = target.ip;
        e.port        = target.port;
        e.address     = addr;
        e.type        = "i32";
        e.value       = "1\xe2\x86\x92" "0";   // "1→0"
        oscLog_.push (std::move (e));
    }
    else
    {
        updateSendTargetRuntimeState (target, false, "send failed (" + describeResolumeSendTarget (target) + ")");
        oscSendOk_ = false;
        oscSendState_ = "error";
        oscSendDetail_ = "send failed to " + target.ip + ":" + juce::String (target.port);
        updateNetworkStatusIndicator();
    }

    return ok;
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
    statusBar_.setRightStatus (text, colour);
}

void TriggerContentComponent::setResolumeStatusText (const juce::String& text, juce::Colour colour)
{
    statusBar_.setLeftStatus (text, colour);
}

void TriggerContentComponent::setNetworkStatusText (const juce::String& text, juce::Colour colour)
{
    statusBar_.setCenterStatus (text, colour);
}

void TriggerContentComponent::updateNetworkStatusIndicator()
{
    const auto nowMs = juce::Time::currentTimeMillis();
    const bool inRecent = lastOscInMs_ > 0 && (nowMs - lastOscInMs_) <= 2000;
    bool outRecent = lastOscOutMs_ > 0 && (nowMs - lastOscOutMs_) <= 2000;
    const int configuredTargetCount = juce::jlimit (1, 8, resolumeSendTargetCount_);
    int okTargets = 0;
    int errorTargets = 0;
    for (int i = 1; i <= configuredTargetCount; ++i)
    {
        if (auto target = getConfiguredResolumeSendTarget (i); target.has_value())
        {
            if (auto it = sendTargetRuntimeStates_.find (makeResolumeSendTargetKey (*target)); it != sendTargetRuntimeStates_.end())
            {
                okTargets += it->second.ok ? 1 : 0;
                errorTargets += it->second.ok ? 0 : 1;
                outRecent = outRecent || (it->second.lastTxMs > 0 && (nowMs - it->second.lastTxMs) <= 2000);
            }
        }
    }

    juce::String text = "OSC ";
    juce::Colour colour = juce::Colour::fromRGB (0xa0, 0xa4, 0xac);

    if (oscListenState_ == "error")
    {
        text = "OSC IN ERR: " + oscListenDetail_;
        colour = juce::Colour::fromRGB (0xec, 0x48, 0x3c);
    }
    else if (oscSendState_ == "error")
    {
        text = "OSC OUT ERR: " + oscSendDetail_;
        colour = juce::Colour::fromRGB (0xde, 0x9b, 0x3c);
    }
    else
    {
        const auto listenPort = juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue());
        text = "IN " + juce::String (oscListenOk_ ? "OK " : "-- ") + juce::String (listenPort)
             + "  OUT " + juce::String (okTargets) + "/" + juce::String (configuredTargetCount) + " OK";

        if (inRecent || outRecent)
        {
            text += "  ";
            if (inRecent)  text += "RX";
            if (inRecent && outRecent) text += "/";
            if (outRecent) text += "TX";
            colour = juce::Colour::fromRGB (0x51, 0xc8, 0x7b);
        }
        else if (errorTargets > 0)
        {
            colour = juce::Colour::fromRGB (0xde, 0x9b, 0x3c);
        }
    }

    setNetworkStatusText (text, colour);
}

void TriggerContentComponent::requestUpdateCheck (bool manual)
{
    if (updateCheckInFlight_)
        return;

    updateCheckManual_ = manual;
    updateCheckInFlight_ = true;
    updateChecker_.checkAsync (bridge::version::kAppVersion);

    if (manual)
        setResolumeStatusText ("Checking for updates...", juce::Colour::fromRGB (0xa0, 0xa4, 0xac));
}

void TriggerContentComponent::showUpdatePrompt()
{
    if (availableVersion_.isEmpty() || availableReleaseUrl_.isEmpty())
        return;

    UpdatePromptWindow::show (bridge::version::kAppVersion,
                              availableVersion_,
                              availableReleaseNotes_,
                              [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
                              {
                                  if (safe != nullptr)
                                      safe->beginUpdateInstall();
                              },
                              getParentComponent());
}

void TriggerContentComponent::beginUpdateInstall()
{
    if (updateInstaller_.isBusy())
        return;

    const auto assetUrl = availableAssetUrl_.isNotEmpty() ? availableAssetUrl_ : availableReleaseUrl_;
    if (assetUrl.isEmpty())
    {
        openLatestReleasePage();
        return;
    }

    setResolumeStatusText ("Downloading update...", juce::Colour::fromRGB (0xa0, 0xa4, 0xac));
    updateInstaller_.downloadAsync (availableVersion_,
                                    assetUrl,
                                    [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (const juce::String& error)
                                    {
                                        if (safe == nullptr)
                                            return;

                                        safe->setResolumeStatusText ("Update download failed", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                        DarkDialog::show ("Easy Trigger", error, safe->getParentComponent());
                                    },
                                    [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (const juce::File& packageFile)
                                    {
                                        if (safe == nullptr)
                                            return;

                                        if (! safe->launchDownloadedUpdate (packageFile))
                                        {
                                            safe->setResolumeStatusText ("Update launch failed", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
                                            DarkDialog::show ("Easy Trigger",
                                                              "The update package was downloaded, but could not be started automatically.",
                                                              safe->getParentComponent());
                                        }
                                    });
}

void TriggerContentComponent::openLatestReleasePage()
{
    const auto preferredUrl = availableAssetUrl_.isNotEmpty() ? availableAssetUrl_ : availableReleaseUrl_;
    if (preferredUrl.isNotEmpty())
        juce::URL (preferredUrl).launchInDefaultBrowser();
}

bool TriggerContentComponent::launchDownloadedUpdate (const juce::File& packageFile)
{
    if (! packageFile.existsAsFile())
        return false;

#if JUCE_WINDOWS
    const auto pid = (int) ::GetCurrentProcessId();
    auto helperFile = packageFile.getSiblingFile ("EasyTrigger_Update_Helper_" + juce::String (pid) + ".ps1");

    auto escapePs = [] (juce::String s)
    {
        return s.replace ("'", "''");
    };

    const auto installerPath = escapePs (packageFile.getFullPathName());
    const auto helperPath = escapePs (helperFile.getFullPathName());
    const juce::String script =
        "$pidToWait = " + juce::String (pid) + "\n"
        "$installer = '" + installerPath + "'\n"
        "while (Get-Process -Id $pidToWait -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 500 }\n"
        "Start-Process -FilePath $installer -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART'\n"
        "Start-Sleep -Seconds 1\n"
        "Remove-Item -LiteralPath '" + helperPath + "' -Force -ErrorAction SilentlyContinue\n";

    if (! helperFile.replaceWithText (script))
        return false;

    auto powershellExe = juce::File::getSpecialLocation (juce::File::windowsSystemDirectory)
                             .getChildFile ("WindowsPowerShell")
                             .getChildFile ("v1.0")
                             .getChildFile ("powershell.exe");
    if (! powershellExe.existsAsFile())
        return false;

    const bool started = powershellExe.startAsProcess ("-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \""
                                                       + helperFile.getFullPathName() + "\"");
    if (! started)
        return false;

    setResolumeStatusText ("Installing update...", juce::Colour::fromRGB (0x51, 0xc8, 0x7b));
    if (auto* app = juce::JUCEApplication::getInstance())
        app->systemRequestedQuit();
    return true;
#else
    const bool started = packageFile.startAsProcess();
    if (started)
    {
       #if JUCE_MAC
        setResolumeStatusText ("Update image opened", juce::Colour::fromRGB (0x51, 0xc8, 0x7b));
        DarkDialog::show ("Easy Trigger",
                          "The update image has been opened.\nComplete the app replacement, then relaunch Easy Trigger.",
                          getParentComponent());
       #endif
    }
    return started;
#endif
}

void TriggerContentComponent::pollUpdateChecker()
{
    if (! updateCheckInFlight_ || ! updateChecker_.hasResult())
        return;

    updateCheckInFlight_ = false;
    availableVersion_.clear();
    availableReleaseUrl_.clear();
    availableAssetUrl_.clear();
    availableReleaseNotes_.clear();
    updateAvailable_ = false;

    if (updateChecker_.didCheckFail())
    {
        if (updateCheckManual_)
            DarkDialog::show ("Easy Trigger", "Update check failed.\nPlease try again later.", getParentComponent());
        updateCheckManual_ = false;
        return;
    }

    availableVersion_ = updateChecker_.getLatestVersion();
    availableReleaseUrl_ = updateChecker_.getReleaseUrl();
    availableAssetUrl_ = updateChecker_.getPreferredAssetUrl();
    availableReleaseNotes_ = updateChecker_.getReleaseNotes();
    updateAvailable_ = updateChecker_.isUpdateAvailable();

    if (updateAvailable_)
    {
        if (! updatePromptShown_ || updateCheckManual_)
        {
            updatePromptShown_ = true;
            showUpdatePrompt();
        }
    }
    else if (updateCheckManual_)
    {
        DarkDialog::show ("Easy Trigger", "You already have the latest version.", getParentComponent());
    }

    updateCheckManual_ = false;
}

void TriggerContentComponent::logGetClipsDiagnostic (const juce::String& message) const
{
    juce::ignoreUnused (message);
}

void TriggerContentComponent::fillStatusMonitorValues (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals) const
{
    keys.clearQuick();
    vals.clearQuick();

    juce::String fired;
    int okSendSlots = 0;
    const int totalSendSlots = juce::jlimit (1, 8, resolumeSendTargetCount_);
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

    keys.add ("Source:");        vals.add (sourceCombo_.getText());
    keys.add ("Input TC:");      vals.add (tcLabel_.getText()
                                           + "  (" + formatDisplayedInputFps (hasLatchedTc_, latchedFps_, fpsIndicatorStrip_.get()) + ")");
    keys.add ("TC Status:");     vals.add (statusBar_.getRightText());
    keys.add ("LTC Out:");       vals.add ((ltcOutSwitch_.getState() ? "ON" : "OFF")
                                           + juce::String ("  |  ") + ltcOutDeviceCombo_.getText());
    keys.add ("LTC Ch / Rate:"); vals.add (ltcOutChannelCombo_.getText()
                                           + "  |  " + ltcOutSampleRateCombo_.getText());
    keys.add ("MTC In:");        vals.add (mtcInCombo_.getText());
    keys.add ("Last Fired:");    vals.add (fired.isEmpty() ? "-" : fired);
    keys.add ("Resolume:");      vals.add (statusBar_.getLeftText());
    keys.add ("Network:");       vals.add (statusBar_.getCenterText());
    keys.add ("ArtNet In:");     vals.add (artnetInCombo_.getText()
                                           + "  |  " + artnetListenIpEditor_.getText());
    keys.add ("OSC Listen:");    vals.add (oscIpEditor_.getText() + ":" + oscPortEditor_.getText());
    keys.add ("Res Listen:");    vals.add (oscListenDetail_);
    for (int i = 1; i <= totalSendSlots; ++i)
    {
        if (auto target = getConfiguredResolumeSendTarget (i); target.has_value())
        {
            const auto key = makeResolumeSendTargetKey (*target);
            juce::String value = "idle  |  " + describeResolumeSendTarget (*target);
            if (auto it = sendTargetRuntimeStates_.find (key); it != sendTargetRuntimeStates_.end())
            {
                okSendSlots += it->second.ok ? 1 : 0;
                value = (it->second.ok ? "OK  |  " : "ERR |  ")
                      + describeResolumeSendTarget (*target)
                      + "  |  " + it->second.detail;
            }
            keys.add ("Send " + juce::String (i) + ":");
            vals.add (value);
        }
    }
    keys.add ("Send Summary:");  vals.add (juce::String (okSendSlots) + "/" + juce::String (totalSendSlots)
                                           + " OK  |  " + statusBar_.getCenterText());
    keys.add ("OSC Activity:");  vals.add ("RX "
                                           + juce::String (lastOscInMs_ > 0 ? juce::Time (lastOscInMs_).formatted ("%H:%M:%S") : "-")
                                           + "  |  TX "
                                           + juce::String (lastOscOutMs_ > 0 ? juce::Time (lastOscOutMs_).formatted ("%H:%M:%S") : "-"));
}

void TriggerContentComponent::openStatusMonitorWindow()
{
    // If already open, bring to front
    if (statusMonitor_ != nullptr)
    {
        statusMonitor_->toFront (true);
        return;
    }

    auto getter = [this] (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals)
    {
        fillStatusMonitorValues (keys, vals);
    };

    auto* win = new StatusMonitorWindow (getter, &oscLog_, getParentComponent());
    statusMonitor_ = win;
}

void TriggerContentComponent::sendTestTrigger (int layer, int clip, int sendTargetIndex)
{
    if (layer < 1 || clip < 1)
        return;
    const auto addr = "/composition/layers/" + juce::String (layer) + "/clips/" + juce::String (clip) + "/connect";
    for (const auto& target : collectResolumeSendTargets (sendTargetIndex))
        sendOscPulse (target, addr);
}

} // namespace trigger
