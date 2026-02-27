#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "core/Timecode.h"
#include "engine/BridgeEngine.h"
#include "engine/ResolumeClipCollector.h"

namespace trigger
{
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
    struct TriggerRow
    {
        int layer { 0 };
        int clip { 0 };
        juce::String name;
        juce::String durationTc;
        juce::String triggerTc;
        bool connected { false };
    };

    void timerCallback() override;
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;

    void loadFonts();
    void applyTheme();
    void refreshInputsForSource();
    void refreshLtcOutDevices();
    void startInput();
    void applyLtcOutput();
    void refreshTriggerRows();
    void queryResolume();
    static juce::String secondsToTc (double sec, FrameRate fps);

    bridge::engine::BridgeEngine bridgeEngine_;
    trigger::engine::ResolumeClipCollector clipCollector_;
    juce::Array<bridge::engine::AudioChoice> ltcOutChoices_;
    std::vector<TriggerRow> triggerRows_;

    juce::Font headerBold_;
    juce::Font headerLight_;
    juce::Font mono_;

    juce::Label easyLabel_ { {}, "EASY" };
    juce::Label triggerLabel_ { {}, "TRIGGER" };
    juce::Label versionLabel_ { {}, "v2.4.13" };
    juce::Label tcLabel_ { {}, "00:00:00:00" };
    juce::Label fpsLabel_ { {}, "TC FPS: --" };
    juce::Label statusLabel_ { {}, "STOPPED" };
    juce::Label sourceLbl_ { {}, "Source:" };
    juce::Label sourceDeviceLbl_ { {}, "Device (input):" };
    juce::Label sourceChannelLbl_ { {}, "Channel:" };
    juce::Label oscPortLbl_ { {}, "OSC Port:" };
    juce::Label ltcOutDriverLbl_ { {}, "Out driver:" };
    juce::Label ltcOutDeviceLbl_ { {}, "Out device:" };
    juce::Label ltcOutChannelLbl_ { {}, "Out channel:" };
    juce::Label ltcOutRateLbl_ { {}, "Sample rate:" };
    juce::Label resSendIpLbl_ { {}, "Resolume send IP:" };
    juce::Label resSendPortLbl_ { {}, "Resolume send port:" };
    juce::Label resListenIpLbl_ { {}, "Resolume listen IP:" };
    juce::Label resListenPortLbl_ { {}, "Resolume listen port:" };
    juce::Label resMaxLayersLbl_ { {}, "Max layers:" };
    juce::Label resMaxClipsLbl_ { {}, "Max clips:" };

    juce::ComboBox sourceCombo_;
    juce::ComboBox sourceDeviceCombo_;
    juce::ComboBox sourceChannelCombo_;
    juce::TextEditor oscPortEditor_;

    juce::ToggleButton ltcOutEnable_;
    juce::ComboBox ltcOutDriverCombo_;
    juce::ComboBox ltcOutDeviceCombo_;
    juce::ComboBox ltcOutChannelCombo_;
    juce::ComboBox ltcOutRateCombo_;

    juce::TextEditor resolumeSendIp_;
    juce::TextEditor resolumeSendPort_;
    juce::TextEditor resolumeListenIp_;
    juce::TextEditor resolumeListenPort_;
    juce::TextEditor resolumeMaxLayers_;
    juce::TextEditor resolumeMaxClips_;
    juce::TextButton getTriggersBtn_ { "Get Triggers" };

    juce::TableListBox triggerTable_;
    juce::Viewport tableViewport_;
    juce::Array<juce::Rectangle<int>> leftRowRects_;
    juce::Array<juce::Rectangle<int>> rightSectionRects_;

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
