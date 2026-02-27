#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <map>
#include <set>

#include "core/Timecode.h"
#include "engine/BridgeEngine.h"
#include "engine/ResolumeClipCollector.h"

namespace trigger
{
class ExpandCircleButton final : public juce::Component
{
public:
    std::function<void()> onClick;
    void setExpanded (bool expanded) { expanded_ = expanded; repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (0x48, 0x48, 0x48));
        g.fillEllipse (b);
        g.setColour (juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
        g.drawEllipse (b, 1.0f);

        juce::Path p;
        const auto cx = b.getCentreX();
        const auto cy = b.getCentreY();
        const float s = 5.6f;
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
        if (onClick) onClick();
    }
private:
    bool expanded_ { true };
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
        g.drawEllipse (b, 1.4f);
        g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
        g.fillEllipse (b.reduced (1.6f));
        if (state_)
        {
            g.setColour (juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
            g.fillEllipse (b.reduced (4.8f));
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        state_ = ! state_;
        repaint();
        if (onToggle) onToggle (state_);
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
        if (onToggle) onToggle (state_);
    }
private:
    bool state_ { false };
};

class LevelMeter final : public juce::Component
{
public:
    LevelMeter() { setOpaque (false); }
    void setMeterColour (juce::Colour c) { meterColour_ = c; }
    void setLevel (float l)
    {
        l = juce::jlimit (0.0f, 2.0f, l);
        if (std::abs (level_ - l) > 0.001f)
        {
            level_ = l;
            repaint();
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto intBounds = getLocalBounds();
        constexpr float corner = 2.0f;

        g.setColour (juce::Colour (0xFF0D0E12));
        g.fillRoundedRectangle (intBounds.toFloat(), corner);

        auto bounds = intBounds.toFloat().reduced (1.0f);
        if (level_ > 0.001f)
        {
            const float display = juce::jmin (1.0f, level_);
            const auto fill = bounds.withWidth (bounds.getWidth() * display);

            juce::Colour bar;
            if (level_ < 0.6f)      bar = meterColour_.withAlpha (0.7f);
            else if (level_ < 0.85f) bar = juce::Colour (0xFFFFAB00).withAlpha (0.8f);
            else                    bar = juce::Colour (0xFFC62828).withAlpha (0.9f);

            g.setColour (bar);
            g.fillRoundedRectangle (fill, corner);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (fill.withHeight (fill.getHeight() * 0.4f), corner);

            if (level_ > 1.0f)
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
    float level_ { 0.0f };
    juce::Colour meterColour_ { juce::Colour (0xFF3D8070) };
};

class TriggerContentComponent final : public juce::Component,
                                      private juce::Timer,
                                      private juce::TableListBoxModel
{
public:
    TriggerContentComponent();
    ~TriggerContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class AudioScanThread;
    struct TriggerClip
    {
        int layer { 0 };
        int clip { 0 };
        bool include { true };
        juce::String name;
        juce::String layerName;
        juce::String countdownTc { "00:00:00:00" };
        double triggerRangeSec { 5.0 };
        juce::String durationTc;
        juce::String triggerTc;
        juce::String endActionMode { "off" };
        juce::String endActionCol;
        juce::String endActionLayer;
        juce::String endActionClip;
        bool connected { false };
        bool timecodeHit { false };
    };
    struct DisplayRow
    {
        bool isGroup { false };
        int layer { 0 };
        int clipIndex { -1 };
    };

    void timerCallback() override;
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void loadFonts();
    void applyTheme();
    void refreshInputsForSource();
    void refreshLtcOutDevices();
    void startInput();
    void applyLtcOutput();
    void refreshTriggerRows();
    void rebuildDisplayRows();
    void updateTableColumnWidths();
    void queryResolume();
    void updateClipCountdowns();
    void evaluateAndFireTriggers();
    void startAudioDeviceScan();
    void onAudioScanComplete (const juce::Array<bridge::engine::AudioChoice>& inputs,
                              const juce::Array<bridge::engine::AudioChoice>& outputs);
    void sendTestTrigger (int layer, int clip);
    juce::Array<bridge::engine::AudioChoice> filteredLtcInputs();
    static juce::String secondsToTc (double sec, FrameRate fps);
    static bool parseTcToFrames (const juce::String& tc, int fps, int& outFrames);

    bridge::engine::BridgeEngine bridgeEngine_;
    trigger::engine::ResolumeClipCollector clipCollector_;
    juce::Array<bridge::engine::AudioChoice> allInputChoices_;
    juce::Array<bridge::engine::AudioChoice> allOutputChoices_;
    juce::Array<int> filteredInputIndices_;
    juce::Array<int> filteredOutputIndices_;
    std::unique_ptr<AudioScanThread> scanThread_;
    juce::Array<bridge::engine::AudioChoice> ltcOutChoices_;
    std::vector<TriggerClip> triggerRows_;
    std::vector<DisplayRow> displayRows_;
    std::map<int, bool> layerExpanded_;
    std::map<int, bool> layerEnabled_;
    std::set<std::pair<int, int>> currentTriggerKeys_;
    juce::Array<bridge::engine::AudioChoice> sourceLtcChoices_;
    std::map<std::pair<int, int>, bool> triggerRangeActive_;
    int lastInputFrames_ { 0 };
    bool hasLastInputFrames_ { false };
    double lastTriggerFireTs_ { 0.0 };

    juce::Font headerBold_;
    juce::Font headerLight_;
    juce::Font mono_;
    bool hasLiveInputTc_ { false };
    Timecode liveInputTc_ {};
    FrameRate liveInputFps_ { FrameRate::FPS_25 };

    juce::Label easyLabel_ { {}, "EASY" };
    juce::Label triggerLabel_ { {}, "TRIGGER" };
    juce::Label versionLabel_ { {}, "v2.4.13" };
    juce::Label tcLabel_ { {}, "00:00:00:00" };
    juce::Label fpsLabel_ { {}, "TC FPS: --" };
    juce::Label statusLabel_ { {}, "STOPPED" };
    juce::Label sourceHeader_ { {}, "Source" };
    juce::Label resolumeHeader_ { {}, "Resolume Settings" };
    juce::Label ltcOutHeader_ { {}, "Out LTC" };
    ExpandCircleButton sourceExpandBtn_;
    ExpandCircleButton resolumeExpandBtn_;
    ExpandCircleButton ltcOutExpandBtn_;
    bool sourceExpanded_ { true };
    bool resolumeExpanded_ { false };
    bool ltcOutExpanded_ { false };
    juce::Label sourceLbl_ { {}, "Source:" };
    juce::Label sourceDriverLbl_ { {}, "Driver:" };
    juce::Label sourceDeviceLbl_ { {}, "Device (input):" };
    juce::Label sourceChannelLbl_ { {}, "Channel:" };
    juce::Label sourceRateLbl_ { {}, "Sample rate:" };
    juce::Label sourceLevelLbl_ { {}, "Level:" };
    juce::Label sourceGainLbl_ { {}, "Input gain:" };
    juce::Label sourceMtcLbl_ { {}, "MTC Input:" };
    juce::Label sourceArtLbl_ { {}, "ArtNet adapter:" };
    juce::Label oscPortLbl_ { {}, "OSC Port:" };
    juce::Label oscAdapterLbl_ { {}, "OSC adapter:" };
    juce::Label oscIpLbl_ { {}, "OSC Listen IP:" };
    juce::Label oscFpsLbl_ { {}, "OSC FPS:" };
    juce::Label oscCmdStrLbl_ { {}, "OSC str cmd:" };
    juce::Label oscCmdFloatLbl_ { {}, "OSC float cmd:" };
    juce::Label ltcOutDriverLbl_ { {}, "Out driver:" };
    juce::Label ltcOutDeviceLbl_ { {}, "Out device:" };
    juce::Label ltcOutChannelLbl_ { {}, "Out channel:" };
    juce::Label ltcOutRateLbl_ { {}, "Sample rate:" };
    juce::Label ltcOutOffsetLbl_ { {}, "Offset (frames):" };
    juce::Label ltcOutLevelLbl_ { {}, "Output level:" };
    juce::Label ltcThruLbl_ { {}, "Thru" };
    juce::Label resSendIpLbl_ { {}, "Resolume send IP:" };
    juce::Label resSendPortLbl_ { {}, "Resolume send port:" };
    juce::Label resListenIpLbl_ { {}, "Resolume listen IP:" };
    juce::Label resListenPortLbl_ { {}, "Resolume listen port:" };
    juce::Label resMaxLayersLbl_ { {}, "Max layers:" };
    juce::Label resMaxClipsLbl_ { {}, "Max clips:" };

    juce::ComboBox sourceCombo_;
    juce::ComboBox sourceDriverCombo_;
    juce::ComboBox sourceDeviceCombo_;
    juce::ComboBox sourceChannelCombo_;
    juce::ComboBox sourceRateCombo_;
    LevelMeter sourceLevelMeter_;
    float sourceLevelSmoothed_ { 0.0f };
    juce::Slider sourceGainSlider_;
    juce::ComboBox sourceMtcCombo_;
    juce::ComboBox sourceArtCombo_;
    juce::ComboBox oscAdapterCombo_;
    juce::TextEditor oscIpEditor_;
    juce::TextEditor oscPortEditor_;
    juce::ComboBox oscFpsCombo_;
    juce::TextEditor oscCmdStrEditor_;
    juce::TextEditor oscCmdFloatEditor_;

    MacSwitch ltcOutSwitch_;
    DotToggle ltcThruDot_;
    juce::ComboBox ltcOutDriverCombo_;
    juce::ComboBox ltcOutDeviceCombo_;
    juce::ComboBox ltcOutChannelCombo_;
    juce::ComboBox ltcOutRateCombo_;
    juce::TextEditor ltcOutOffsetEditor_;
    juce::Slider ltcOutLevelSlider_;

    juce::TextEditor resolumeSendIp_;
    juce::TextEditor resolumeSendPort_;
    juce::TextEditor resolumeListenIp_;
    juce::TextEditor resolumeListenPort_;
    juce::TextEditor resolumeMaxLayers_;
    juce::TextEditor resolumeMaxClips_;
    juce::TextButton getTriggersBtn_ { "Get Triggers" };

    juce::TableListBox triggerTable_;
    juce::Array<juce::Rectangle<int>> leftRowRects_;
    juce::Array<juce::Rectangle<int>> sectionRowRects_;
    juce::Array<juce::Rectangle<int>> rightSectionRects_;
    juce::Rectangle<int> headerRect_;

    juce::Colour bg_ { juce::Colour::fromRGB (0x17, 0x17, 0x17) };
    juce::Colour row_ { juce::Colour::fromRGB (0x3a, 0x3a, 0x3a) };
    juce::Colour input_ { juce::Colour::fromRGB (0x24, 0x24, 0x24) };
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow();
    void closeButtonPressed() override;
};
} // namespace trigger
