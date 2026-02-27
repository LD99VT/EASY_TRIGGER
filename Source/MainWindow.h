#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "engine/BridgeEngine.h"

namespace bridge
{
class ExpandCircleButton final : public juce::Component
{
public:
    std::function<void()> onClick;

    void setExpanded (bool expanded)
    {
        expanded_ = expanded;
        repaint();
    }

    bool isExpanded() const { return expanded_; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        const auto bg = juce::Colour::fromRGB (0x48, 0x48, 0x48);
        g.setColour (bg);
        g.fillEllipse (b);
        g.setColour (juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
        g.drawEllipse (b, 1.0f);

        juce::Path p;
        const auto cx = b.getCentreX();
        const auto cy = b.getCentreY();
        const float s = 5.8f; // larger arrow
        if (expanded_)
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
        g.setColour (juce::Colour::fromRGB (0xf2, 0xf2, 0xf2));
        g.fillPath (p);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onClick)
            onClick();
    }

private:
    bool expanded_ { false };
};

class DotToggle final : public juce::Component
{
public:
    std::function<void(bool)> onToggle;
    void setState (bool s) { state_ = s; repaint(); }
    bool getState() const { return state_; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
        g.drawEllipse (b, 1.6f);
        g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
        g.fillEllipse (b.reduced (1.6f));
        if (state_)
        {
            g.setColour (juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
            g.fillEllipse (b.reduced (5.0f));
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        state_ = ! state_;
        repaint();
        if (onToggle)
            onToggle (state_);
    }

private:
    bool state_ { false };
};

class MacSwitch final : public juce::Component
{
public:
    std::function<void(bool)> onToggle;
    void setState (bool s) { state_ = s; repaint(); }
    bool getState() const { return state_; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (state_ ? juce::Colour::fromRGB (0x4c, 0xbd, 0x54) : juce::Colour::fromRGB (0x3f, 0x3f, 0x3f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (state_ ? juce::Colour::fromRGB (0x56, 0xc8, 0x5f) : juce::Colour::fromRGB (0x4b, 0x4b, 0x4b));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
        const float d = b.getHeight() - 4.0f;
        const float x = state_ ? (b.getRight() - d - 2.0f) : (b.getX() + 2.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x, b.getY() + 2.0f, d, d);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        state_ = ! state_;
        repaint();
        if (onToggle)
            onToggle (state_);
    }

private:
    bool state_ { false };
};

class LevelMeter final : public juce::Component
{
public:
    LevelMeter() { setOpaque (false); }

    void setLevel (float newLevel)
    {
        newLevel = juce::jlimit (0.0f, 2.0f, newLevel);
        if (std::abs (currentLevel_ - newLevel) > 0.001f)
        {
            currentLevel_ = newLevel;
            repaint();
        }
    }

    void setMeterColour (juce::Colour c) { meterColour_ = c; }

    void paint (juce::Graphics& g) override
    {
        auto intBounds = getLocalBounds();
        constexpr float corner = 2.0f;

        g.setColour (juce::Colour (0xFF0D0E12));
        g.fillRoundedRectangle (intBounds.toFloat(), corner);

        auto bounds = intBounds.toFloat().reduced (1.0f);
        if (currentLevel_ > 0.001f)
        {
            const float displayLevel = juce::jmin (1.0f, currentLevel_);
            const auto fillBounds = bounds.withWidth (bounds.getWidth() * displayLevel);

            juce::Colour barColour;
            if (currentLevel_ < 0.6f)
                barColour = meterColour_.withAlpha (0.7f);
            else if (currentLevel_ < 0.85f)
                barColour = juce::Colour (0xFFFFAB00).withAlpha (0.8f);
            else
                barColour = juce::Colour (0xFFC62828).withAlpha (0.9f);

            g.setColour (barColour);
            g.fillRoundedRectangle (fillBounds, corner);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (fillBounds.withHeight (fillBounds.getHeight() * 0.4f), corner);

            if (currentLevel_ > 1.0f)
            {
                g.setColour (juce::Colour (0xFFC62828).withAlpha (0.3f));
                g.fillRoundedRectangle (bounds, corner);
            }
        }

        g.setColour (juce::Colour (0xFF2A2D35));
        g.drawRoundedRectangle (bounds, corner, 0.5f);

        g.setColour (juce::Colour (0xFF2A2D35).withAlpha (0.6f));
        for (auto tp : { 0.25f, 0.5f, 0.75f })
        {
            const float x = bounds.getX() + bounds.getWidth() * tp;
            g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 0.5f);
        }
    }

private:
    float currentLevel_ { 0.0f };
    juce::Colour meterColour_ { juce::Colour (0xFF2E7D32) };
};

class MainContentComponent final : public juce::Component, private juce::Timer
{
public:
    MainContentComponent();
    ~MainContentComponent() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool closeToTrayEnabled() const { return closeToTray_; }

private:
    void timerCallback() override;

    void loadFontsAndIcon();
    void applyLookAndFeel();
    void restartSelectedSource();
    void onOutputToggleChanged();
    void onOutputSettingsChanged();
    void onInputSettingsChanged();
    int calcPreferredHeight() const;
    int calcHeightForState (bool sourceExpanded, int sourceId, bool outLtcExpanded, bool outMtcExpanded, bool outArtExpanded) const;
    void updateWindowHeight();

    void refreshDeviceLists();
    void refreshNetworkMidiLists();
    void refreshLtcDeviceListsByDriver();
    void fillAudioCombo (juce::ComboBox& combo, const juce::Array<engine::AudioChoice>& choices);
    static double comboSampleRate (const juce::ComboBox& combo);
    static int comboBufferSize (const juce::ComboBox& combo);
    static int comboChannelIndex (const juce::ComboBox& combo);
    static int offsetFromEditor (const juce::TextEditor& editor);
    static void styleCombo (juce::ComboBox& c);
    static void styleEditor (juce::TextEditor& e);
    static void styleSlider (juce::Slider& s, bool dbStyle);
    void syncOscIpWithAdapter();
    juce::File findBridgeBaseDir() const;
    void setStatusText (const juce::String& text, juce::Colour colour);
    void openStatusMonitorWindow();
    void openSettingsMenu();
    void saveConfigAs();
    void loadConfigFrom();
    void saveConfigToFile (const juce::File& cfgFile);
    void loadConfigFromFile (const juce::File& cfgFile);
    juce::File prefsFilePath() const;
    void loadRuntimePrefs();
    void saveRuntimePrefs() const;
    void maybeAutoLoadConfig();
    void resetToDefaults();
    void openHelpPage();

    engine::BridgeEngine bridgeEngine_;
    juce::Array<engine::AudioChoice> inputChoices_;
    juce::Array<engine::AudioChoice> outputChoices_;
    juce::Array<engine::AudioChoice> filteredInputChoices_;
    juce::Array<engine::AudioChoice> filteredOutputChoices_;
    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel_;
    juce::Image appIcon_;
    juce::Font titleEasyFont_;
    juce::Font titleBridgeFont_;
    juce::Font monoFont_;
    bool hasLatchedTc_ { false };
    Timecode latchedTc_ {};
    FrameRate latchedFps_ { FrameRate::FPS_25 };

    juce::Label titleEasyLabel_;
    juce::Label titleBridgeLabel_;
    juce::Label titleVersionLabel_;
    juce::TextButton helpButton_ { "?" };
    juce::Label tcLabel_;
    juce::Label tcFpsLabel_;
    juce::TextButton statusButton_;
    juce::TextButton settingsButton_ { "Settings" };
    juce::TextButton quitButton_ { "Quit" };
    bool closeToTray_ { false };
    bool autoLoadOnStartup_ { false };
    juce::File lastConfigFile_;
    std::unique_ptr<juce::FileChooser> saveChooser_;
    std::unique_ptr<juce::FileChooser> loadChooser_;

    juce::ComboBox sourceCombo_;
    ExpandCircleButton sourceExpandBtn_;
    juce::ComboBox ltcInDriverCombo_;
    juce::ComboBox ltcOutDriverCombo_;
    juce::Label sourceHeaderLabel_ { {}, "Source" };
    juce::Label outLtcHeaderLabel_ { {}, "Out LTC" };
    juce::Label outMtcHeaderLabel_ { {}, "Out MTC" };
    juce::Label outArtHeaderLabel_ { {}, "Out ArtNet" };
    ExpandCircleButton outLtcExpandBtn_;
    ExpandCircleButton outMtcExpandBtn_;
    ExpandCircleButton outArtExpandBtn_;
    juce::Label inDriverLbl_ { {}, "Driver:" };
    juce::Label inDeviceLbl_ { {}, "Device (input):" };
    juce::Label inChannelLbl_ { {}, "Channel:" };
    juce::Label inRateLbl_ { {}, "Sample rate:" };
    juce::Label inLevelLbl_ { {}, "Level:" };
    juce::Label inGainLbl_ { {}, "Input gain:" };
    juce::Label mtcInLbl_ { {}, "MTC Input:" };
    juce::Label artInLbl_ { {}, "ArtNet adapter:" };
    juce::Label oscIpLbl_ { {}, "OSC Listen IP:" };
    juce::Label oscPortLbl_ { {}, "OSC Port:" };
    juce::Label oscAdapterLbl_ { {}, "OSC adapter:" };
    juce::Label oscFpsLbl_ { {}, "OSC FPS:" };
    juce::Label oscStrLbl_ { {}, "OSC str cmd:" };
    juce::Label oscFloatLbl_ { {}, "OSC float cmd:" };

    juce::Label outDriverLbl_ { {}, "Driver:" };
    juce::Label outDeviceLbl_ { {}, "Device (out):" };
    juce::Label outChannelLbl_ { {}, "Channel:" };
    juce::Label outRateLbl_ { {}, "Sample rate:" };
    juce::Label outOffsetLbl_ { {}, "Offset (frames):" };
    juce::Label outLevelLbl_ { {}, "Output level:" };
    juce::Label mtcOutLbl_ { {}, "MIDI Output:" };
    juce::Label mtcOffsetLbl_ { {}, "Offset (frames):" };
    juce::Label artOutLbl_ { {}, "Interface:" };
    juce::Label artIpLbl_ { {}, "Destination IP:" };
    juce::Label artOffsetLbl_ { {}, "Offset (frames):" };

    juce::ComboBox ltcInDeviceCombo_;
    juce::ComboBox ltcInChannelCombo_;
    juce::ComboBox ltcInSampleRateCombo_;
    LevelMeter ltcInLevelBar_;
    float ltcInLevelSmoothed_ { 0.0f };
    juce::Slider ltcInGainSlider_;

    juce::ComboBox mtcInCombo_;
    juce::ComboBox artnetInCombo_;
    juce::ComboBox oscAdapterCombo_;
    juce::TextEditor oscIpEditor_;
    juce::TextEditor oscPortEditor_;
    juce::ComboBox oscFpsCombo_;
    juce::TextEditor oscAddrStrEditor_;
    juce::TextEditor oscAddrFloatEditor_;

    juce::ComboBox ltcOutDeviceCombo_;
    juce::ComboBox ltcOutChannelCombo_;
    juce::ComboBox ltcOutSampleRateCombo_;
    juce::TextEditor ltcOffsetEditor_;
    juce::Slider ltcOutLevelSlider_;
    MacSwitch ltcOutSwitch_;
    DotToggle ltcThruDot_;
    juce::Label ltcThruLbl_ { {}, "Thru" };

    juce::ComboBox mtcOutCombo_;
    juce::TextEditor mtcOffsetEditor_;
    MacSwitch mtcOutSwitch_;
    DotToggle mtcThruDot_;
    juce::Label mtcThruLbl_ { {}, "Thru" };

    juce::ComboBox artnetOutCombo_;
    juce::TextEditor artnetDestIpEditor_;
    juce::TextEditor artnetOffsetEditor_;
    MacSwitch artnetOutSwitch_;

    bool sourceExpanded_ { true };
    bool outLtcExpanded_ { false };
    bool outMtcExpanded_ { false };
    bool outArtExpanded_ { false };

    juce::Array<juce::Rectangle<int>> paramRowRects_;
    juce::Array<juce::Rectangle<int>> sectionRowRects_;
    juce::Rectangle<int> headerRect_;
    juce::Rectangle<int> statusRect_;
    juce::Rectangle<int> buttonRowRect_;
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow();
    ~MainWindow() override;
    void closeButtonPressed() override;
    void createTrayIcon();
    void showFromTray();
    void quitFromTray();

private:
    std::unique_ptr<juce::SystemTrayIconComponent> trayIcon_;
    bool quittingFromMenu_ { false };
};
} // namespace bridge
