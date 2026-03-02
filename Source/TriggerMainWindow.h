#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <condition_variable>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <atomic>
#include <thread>

#include "Version.h"
#include "core/Timecode.h"
#include "engine/BridgeEngine.h"
#include "engine/ResolumeClipCollector.h"

#include "ui/style/TriggerColours.h"
#include "ui/style/TriggerLookAndFeel.h"
#include "ui/style/StyleHelpers.h"
#include "ui/widgets/CircleButtons.h"
#include "ui/widgets/DotToggle.h"
#include "ui/widgets/MacSwitch.h"
#include "ui/widgets/LevelMeter.h"

namespace trigger
{
class TriggerContentComponent final : public juce::Component,
                                      private juce::Timer,
                                      private juce::TableListBoxModel,
                                      private juce::TableHeaderComponent::Listener
{
public:
    TriggerContentComponent();
    ~TriggerContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& event) override;
    bool closeToTrayEnabled() const noexcept { return closeToTray_; }

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
        bool isCustom { false };
        juce::String customType { "col" };
        juce::String customSourceCol;
        juce::String customSourceLayer;
        juce::String customSourceClip;
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
    void openHelpPage();
    void restartSelectedSource();
    void queueLtcOutputApply();
    void ltcOutputApplyLoop();
    void onInputSettingsChanged();
    void onOutputSettingsChanged();
    void onOutputToggleChanged();
    void openSettingsMenu();
    void resetSettings();
    void loadRuntimePrefs();
    void saveRuntimePrefs() const;
    void maybeAutoLoadConfig();
    void saveConfigAs (int modeId);
    void loadConfigFrom (int modeId);
    void saveConfigToFile (const juce::File& file, int modeId);
    void loadConfigFromFile (const juce::File& file, int modeId);
    int calcPreferredHeight() const;
    int calcHeightForState (bool sourceExpanded, int sourceId, bool outLtcExpanded, bool resolumeExpanded) const;
    void updateWindowHeight();
    void refreshTriggerRows();
    void rebuildDisplayRows();
    bool hasCustomGroup() const;
    void addCustomColTrigger();
    void addCustomLcTrigger();
    void deleteCustomTrigger (int clipIndex);
    void deleteCustomGroup();
    void fireCustomTrigger (const TriggerClip& clip);
    void updateTableColumnWidths();
    void addCustomColumns();
    void removeCustomColumns();
    void tableColumnsResized (juce::TableHeaderComponent*) override;
    void tableColumnsChanged (juce::TableHeaderComponent*) override {}
    void tableSortOrderChanged (juce::TableHeaderComponent*) override {}
    void queryResolume();
    void updateClipCountdowns();
    void evaluateAndFireTriggers();
    void processEndActions();
    void startAudioDeviceScan();
    void onAudioScanComplete (const juce::Array<bridge::engine::AudioChoice>& inputs,
                              const juce::Array<bridge::engine::AudioChoice>& outputs);
    void refreshNetworkMidiLists();
    void refreshLtcDeviceListsByDriver();
    void fillAudioCombo (juce::ComboBox& combo, const juce::Array<bridge::engine::AudioChoice>& choices);
    static double comboSampleRate (const juce::ComboBox& combo);
    static int comboChannelIndex (const juce::ComboBox& combo);
    void syncOscIpWithAdapter();
    void sendTestTrigger (int layer, int clip);
    static juce::String secondsToTc (double sec, FrameRate fps);
    static bool parseTcToFrames (const juce::String& tc, int fps, int& outFrames);
    void setTimecodeStatusText (const juce::String& text, juce::Colour colour);
    void setResolumeStatusText (const juce::String& text, juce::Colour colour);
    void openStatusMonitorWindow();

    bridge::engine::BridgeEngine bridgeEngine_;
    trigger::engine::ResolumeClipCollector clipCollector_;
    juce::Array<bridge::engine::AudioChoice> inputChoices_;
    juce::Array<bridge::engine::AudioChoice> outputChoices_;
    juce::Array<bridge::engine::AudioChoice> filteredInputChoices_;
    juce::Array<bridge::engine::AudioChoice> filteredOutputChoices_;
    juce::Array<int> filteredInputIndices_;
    juce::Array<int> filteredOutputIndices_;
    std::unique_ptr<AudioScanThread> scanThread_;
    std::unique_ptr<juce::FileChooser> saveChooser_;
    std::unique_ptr<juce::FileChooser> loadChooser_;
    std::vector<TriggerClip> triggerRows_;
    std::vector<DisplayRow> displayRows_;
    std::map<int, bool> layerExpanded_;
    std::map<int, bool> layerEnabled_;
    std::set<std::pair<int, int>> currentTriggerKeys_;
    std::map<std::pair<int, int>, bool> triggerRangeActive_;
    int lastInputFrames_ { 0 };
    bool hasLastInputFrames_ { false };
    double lastTriggerFireTs_ { 0.0 };
    juce::Component::SafePointer<juce::Component> statusMonitor_;

    struct PendingEndAction
    {
        double executeTs { 0.0 };
        juce::String mode;
        juce::String col;
        juce::String layer;
        juce::String clip;
    };
    std::map<std::pair<int, int>, PendingEndAction> pendingEndActions_;

    juce::Font headerBold_;
    juce::Font headerLight_;
    juce::Font mono_;
    bool closeToTray_ { false };
    bool autoLoadOnStartup_ { false };
    bool pendingAutoLoad_ { false };
    bool hasLatchedTc_ { false };
    Timecode latchedTc_ {};
    FrameRate latchedFps_ { FrameRate::FPS_25 };
    bool hasLiveInputTc_ { false };
    Timecode liveInputTc_ {};
    FrameRate liveInputFps_ { FrameRate::FPS_25 };
    juce::File lastConfigFile_;

    juce::Label easyLabel_            { {}, "EASY" };
    juce::Label triggerLabel_         { {}, "TRIGGER" };
    juce::Label versionLabel_         { {}, juce::String ("v") + bridge::version::kAppVersion };
    HelpCircleButton helpButton_;
    juce::Label tcLabel_              { {}, "00:00:00:00" };
    juce::Label fpsLabel_             { {}, "TC FPS: --" };
    juce::Label resolumeStatusLabel_  { {}, "Resolume idle" };
    juce::Label statusLabel_          { {}, "STOPPED" };
    juce::Label sourceHeaderLabel_    { {}, "Source" };
    juce::Label outLtcHeaderLabel_    { {}, "Out LTC" };
    juce::Label resolumeHeader_       { {}, "Resolume Settings" };
    ExpandCircleButton sourceExpandBtn_;
    ExpandCircleButton outLtcExpandBtn_;
    ExpandCircleButton resolumeExpandBtn_;
    bool sourceExpanded_   { true };
    bool outLtcExpanded_   { false };
    bool resolumeExpanded_ { false };
    juce::Label inDriverLbl_       { {}, "Driver:" };
    juce::Label inDeviceLbl_       { {}, "Device (input):" };
    juce::Label inChannelLbl_      { {}, "Channel:" };
    juce::Label inRateLbl_         { {}, "Sample rate:" };
    juce::Label inLevelLbl_        { {}, "Level:" };
    juce::Label inGainLbl_         { {}, "Input Gain:" };
    juce::Label mtcInLbl_          { {}, "MTC Input:" };
    juce::Label artInLbl_          { {}, "ArtNet adapter:" };
    juce::Label artInListenIpLbl_  { {}, "Listen IP:" };
    juce::Label oscAdapterLbl_     { {}, "OSC adapter:" };
    juce::Label oscIpLbl_          { {}, "OSC Listen IP:" };
    juce::Label oscPortLbl_        { {}, "OSC Port:" };
    juce::Label oscFpsLbl_         { {}, "OSC FPS:" };
    juce::Label oscStrLbl_         { {}, "OSC str cmd:" };
    juce::Label oscFloatLbl_       { {}, "OSC float cmd:" };
    juce::Label oscFloatTypeLbl_   { {}, "Float type:" };
    juce::Label oscFloatMaxLbl_    { {}, "Float max (s):" };
    juce::Label outDriverLbl_      { {}, "Driver:" };
    juce::Label outDeviceLbl_      { {}, "Device (out):" };
    juce::Label outChannelLbl_     { {}, "Channel:" };
    juce::Label outRateLbl_        { {}, "Sample rate:" };
    juce::Label outOffsetLbl_      { {}, "Offset (frames):" };
    juce::Label outLevelLbl_       { {}, "Output Gain:" };
    juce::Label ltcThruLbl_        { {}, "Thru" };
    juce::Label resSendIpLbl_      { {}, "Send IP:" };
    juce::Label resSendPortLbl_    { {}, "Send port:" };
    juce::Label resListenIpLbl_    { {}, "Listen IP:" };
    juce::Label resListenPortLbl_  { {}, "Listen port:" };
    juce::Label resMaxLayersLbl_   { {}, "Max layers:" };
    juce::Label resMaxClipsLbl_    { {}, "Max clips:" };
    juce::Label resGlobalOffsetLbl_{ {}, "Global offset:" };

    juce::ComboBox sourceCombo_;
    juce::ComboBox ltcInDriverCombo_;
    juce::ComboBox ltcOutDriverCombo_;
    juce::ComboBox ltcInDeviceCombo_;
    juce::ComboBox ltcInChannelCombo_;
    juce::ComboBox ltcInSampleRateCombo_;
    LevelMeter ltcInLevelBar_;
    float ltcInLevelSmoothed_ { 0.0f };
    juce::Slider ltcInGainSlider_;
    juce::ComboBox mtcInCombo_;
    juce::ComboBox artnetInCombo_;
    juce::TextEditor artnetListenIpEditor_;
    juce::ComboBox oscAdapterCombo_;
    juce::TextEditor oscIpEditor_;
    juce::TextEditor oscPortEditor_;
    juce::ComboBox oscFpsCombo_;
    juce::TextEditor oscAddrStrEditor_;
    juce::TextEditor oscAddrFloatEditor_;
    juce::ComboBox   oscFloatTypeCombo_;
    juce::TextEditor oscFloatMaxEditor_;
    juce::ComboBox ltcOutDeviceCombo_;
    juce::ComboBox ltcOutChannelCombo_;
    juce::ComboBox ltcOutSampleRateCombo_;
    juce::TextEditor ltcOffsetEditor_;
    juce::Slider ltcOutLevelSlider_;
    MacSwitch ltcOutSwitch_;
    DotToggle ltcThruDot_;

    juce::TextEditor resolumeSendIp_;
    juce::TextEditor resolumeSendPort_;
    juce::TextEditor resolumeListenIp_;
    juce::TextEditor resolumeListenPort_;
    juce::TextEditor resolumeMaxLayers_;
    juce::TextEditor resolumeMaxClips_;
    juce::TextEditor resolumeGlobalOffset_;
    juce::TextButton getTriggersBtn_  { "Get Clips" };
    juce::TextButton createCustomBtn_ { "Create Custom Trigger" };
    juce::TextButton settingsButton_  { "Settings" };
    juce::TextButton quitButton_      { "Quit" };

    juce::TableListBox triggerTable_;
    juce::Array<juce::Rectangle<int>> leftRowRects_;
    juce::Array<juce::Rectangle<int>> sectionRowRects_;
    juce::Array<juce::Rectangle<int>> rightSectionRects_;
    juce::Rectangle<int> headerRect_;
    juce::Rectangle<int> timerRect_;
    juce::Rectangle<int> statusBarRect_;
    juce::Rectangle<int> statusLeftRect_;
    juce::Rectangle<int> statusRightRect_;
    juce::Rectangle<int> leftViewportRect_;
    juce::Viewport leftViewport_;
    juce::Component leftViewportContent_;
    std::unique_ptr<TriggerLookAndFeel> lookAndFeel_;
    std::thread ltcOutputApplyThread_;
    std::mutex ltcOutputApplyMutex_;
    std::condition_variable ltcOutputApplyCv_;
    bool ltcOutputApplyExit_      { false };
    bool ltcOutputApplyPending_   { false };
    bridge::engine::AudioChoice pendingLtcOutputChoice_;
    int pendingLtcOutputChannel_      { 0 };
    double pendingLtcOutputSampleRate_{ 0.0 };
    int pendingLtcOutputBufferSize_   { 0 };
    bool pendingLtcOutputEnabled_     { false };
    bool pendingLtcThruMode_          { false };
    bool queryPending_    { false };
    juce::int64 queryStartMs_ { 0 };
    bool clipReceiveEnabled_ { false };
    bool colWidthGuard_      { false };

    juce::String customGroupName_ { "Custom Trigger" };

    juce::Colour bg_    { kBg };
    juce::Colour row_   { kRow };
    juce::Colour input_ { kInput };
};

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow();
    ~MainWindow() override;
    void closeButtonPressed() override;
    void showFromTray();
    void quitFromTray();
    void prepareForShutdown();

private:
    void createTrayIcon();

    std::unique_ptr<juce::SystemTrayIconComponent> trayIcon_;
    bool quittingFromMenu_ { false };
};
} // namespace trigger
