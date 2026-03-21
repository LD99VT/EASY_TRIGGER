#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
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

#include "ui/monitoring/OscLog.h"
#include "ui/components/StatusBarComponent.h"
#include "ui/components/SettingsPanelComponent.h"
#include "ui/updates/UpdateChecker.h"
#include "ui/updates/UpdateInstaller.h"
#include "ui/style/TriggerColours.h"
#include "ui/style/TriggerLookAndFeel.h"
#include "ui/style/StyleHelpers.h"
#include "ui/windows/NativeWindowUtils.h"
#include "ui/widgets/CircleButtons.h"
#include "ui/widgets/DotToggle.h"
#include "ui/widgets/MacSwitch.h"
#include "ui/widgets/LevelMeter.h"
#include "trigger/CustomTriggerState.h"

namespace trigger
{
class FpsIndicatorStrip final : public juce::Component
{
public:
    void setActiveFps (std::optional<FrameRate> fps);
    juce::String getActiveFpsText() const;
    void paint (juce::Graphics& g) override;

private:
    std::optional<FrameRate> activeFps_;
};

class FpsConvertStrip final : public juce::Component
{
public:
    explicit FpsConvertStrip (std::initializer_list<FrameRate> availableRates);

    void setSelectedFps (std::optional<FrameRate> fps);
    std::optional<FrameRate> getSelectedFps() const { return selectedFps_; }
    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& event) override;

    std::function<void(std::optional<FrameRate>)> onChange;

private:
    juce::Array<FrameRate> availableRates_;
    std::optional<FrameRate> selectedFps_;
};

class TriggerContentComponent final : public juce::Component,
                                      private juce::Timer,
                                      private juce::TableListBoxModel,
                                      private juce::TableHeaderComponent::Listener
{
public:
    TriggerContentComponent();
    ~TriggerContentComponent() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
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
        juce::String endActionGroup;
        bool hasOffset { false };
        bool connected { false };
        bool testHighlight { false };
        bool timecodeHit { false };
        bool isCustom { false };
        int customGroupId { 0 };
        int customClipId { 0 };
        int orderIndex { 0 };
        juce::String customType { "col" };
        juce::String customSourceCol;
        juce::String customSourceLayer;
        juce::String customSourceClip;
        juce::String customSourceGroup;
        int sendTargetIndex { 0 }; // 0 = All, 1..N = specific Send target
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
    void showRowContextMenu (int rowNumber);
    void openFileMenu();
    void openManageMenu();
    void openViewMenu();
    void openHelpMenu();
    void clearCustomTriggers();
    void clearClipTriggers();
    void clearAllGroups();
    void expandAllGroups (bool expanded);
    void resetTableLayout();

    void loadFonts();
    void applyTheme();
    void openHelpPage();
    void requestUpdateCheck (bool manual);
    void pollUpdateChecker();
    void showUpdatePrompt();
    void beginUpdateInstall();
    void openLatestReleasePage();
    bool launchDownloadedUpdate (const juce::File& packageFile);
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
    int calcHeightForState (bool sourceExpanded, int sourceId, bool triggerOutExpanded, bool outLtcExpanded, bool resolumeExpanded) const;
    void updateWindowHeight (bool forceGrow = true);
    void refreshTriggerRows();
    void syncTriggerRowFeedbackFromCollector();
    void rebuildDisplayRows();
    bool hasCustomGroup() const;
    bool hasCustomGroupsAtLimit() const;
    trigger::model::CustomTriggerGroup* findCustomGroupById (int groupId);
    const trigger::model::CustomTriggerGroup* findCustomGroupById (int groupId) const;
    void syncCustomGroupStateFromLayers();
    void syncLayerStateFromCustomGroups();
    void ensureCustomColumnsState();
    int nextCustomGroupId() const;
    int nextCustomClipId() const;
    int nextCustomClipOrder (int groupId) const;
    int addCustomGroup();
    void addCustomColTriggerToGroup (int groupId);
    void addCustomLcTriggerToGroup (int groupId);
    void addCustomGcTriggerToGroup (int groupId);
    void deleteCustomGroup (int groupId);
    void deleteLayerGroup (int layer);
    void moveCustomGroup (int groupId, int delta);
    void moveCustomClip (int clipIndex, int delta);
    void moveLayerGroup (int layer, int delta);
    void moveClipRow (int clipIndex, int delta);
    void normaliseLayerOrder();
    void moveCustomGroupToOrder (int groupId, int newOrder);
    void moveCustomClipToOrder (int clipIndex, int newOrder);
    void beginCustomDrag (bool draggingGroup, int identifier);
    void updateCustomDrag (juce::Point<int> tablePoint);
    void endCustomDrag (juce::Point<int> tablePoint);
    void addCustomColTrigger();
    void addCustomLcTrigger();
    void addCustomGcTrigger();
    void deleteCustomTrigger (int clipIndex);
    void deleteTriggerRow (int clipIndex);
    void fireCustomTrigger (const TriggerClip& clip);
    void updateTableColumnWidths();
    void refreshTriggerTableContent();
    void repaintTriggerTable();
    void addCustomColumns();
    void removeCustomColumns();
    void tableColumnsResized (juce::TableHeaderComponent*) override;
    void tableColumnsChanged (juce::TableHeaderComponent*) override {}
    void tableSortOrderChanged (juce::TableHeaderComponent*) override {}
    void openGetClipsOptions();
    void queryResolume (bool includeClipsWithOffset, bool includeClipsWithoutOffset);
    void updateClipCountdowns();
    void evaluateAndFireTriggers();
    void processEndActions();
    void startAudioDeviceScan();
    void onAudioScanComplete (const juce::Array<bridge::engine::AudioChoice>& inputs,
                              const juce::Array<bridge::engine::AudioChoice>& outputs);
    void refreshNetworkMidiLists();
    void refreshLtcDeviceListsByDriver();
    void refreshLtcChannelCombos();
    void fillAudioCombo (juce::ComboBox& combo, const juce::Array<bridge::engine::AudioChoice>& choices);
    static double comboSampleRate (const juce::ComboBox& combo);
    static int comboChannelIndex (const juce::ComboBox& combo);
    void syncOscIpWithAdapter();
    void syncResolumeListenIpWithAdapter();
    void toggleSendAdapterExpanded (int index);
    void addResolumeSendTarget();
    void removeResolumeSendTarget (int targetIndex);
    int clampSendTargetIndex (int index) const;
    void populateSendTargetCombo (juce::ComboBox& combo, int selectedIndex) const;
    struct ResolumeSendTarget
    {
        juce::String localBindIp;
        juce::String ip;
        int port { 7000 };
    };
    std::optional<ResolumeSendTarget> getConfiguredResolumeSendTarget (int oneBasedIndex) const;
    std::vector<ResolumeSendTarget> collectResolumeSendTargets() const;
    std::vector<ResolumeSendTarget> collectResolumeSendTargets (int sendTargetIndex) const;
    static juce::String makeResolumeSendTargetKey (const ResolumeSendTarget& target);
    static juce::String describeResolumeSendTarget (const ResolumeSendTarget& target);
    void updateSendTargetRuntimeState (const ResolumeSendTarget& target, bool ok, const juce::String& detail);
    bool sendOscPulse (const ResolumeSendTarget& target, const juce::String& addr);
    void sendTestTrigger (int layer, int clip, int sendTargetIndex);
    static juce::String secondsToTc (double sec, FrameRate fps);
    static bool parseTcToFrames (const juce::String& tc, int fps, int& outFrames);
    void setTimecodeStatusText (const juce::String& text, juce::Colour colour);
    void setResolumeStatusText (const juce::String& text, juce::Colour colour);
    void setNetworkStatusText (const juce::String& text, juce::Colour colour);
    void updateNetworkStatusIndicator();
    void logGetClipsDiagnostic (const juce::String& message) const;
    void fillStatusMonitorValues (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals) const;
    void openStatusMonitorWindow();
    void openPreferencesWindow();
    void updateLeftPanelVisibility (int src);
    void layoutSettingsPanelChrome (juce::Rectangle<int>& left,
                                    juce::Rectangle<int>& getTriggersRow,
                                    juce::Rectangle<int>& createCustomRow);
    void layoutMenuButtons();
    void bringChromeToFront();

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
    std::map<int, int> layerOrder_;
    std::set<int> deletedLayers_;
    std::set<std::pair<int, int>> currentTriggerKeys_;
    std::map<std::pair<int, int>, bool> triggerRangeActive_;
    int lastInputFrames_ { 0 };
    bool hasLastInputFrames_ { false };
    double lastTriggerFireTs_ { 0.0 };
    juce::Component::SafePointer<juce::Component> statusMonitor_;
    juce::Component::SafePointer<juce::Component> preferencesWindow_;
    juce::Component::SafePointer<juce::Component> getClipsOptionsWindow_;
    mutable OscLog oscLog_;
    UpdateChecker updateChecker_ { "https://api.github.com/repos/LD99VT/EASY_TRIGGER/releases/latest" };
    UpdateInstaller updateInstaller_;
    int updateCheckDelay_ { 90 };
    bool updateCheckInFlight_ { false };
    bool updateCheckManual_ { false };
    bool updatePromptShown_ { false };
    bool updateAvailable_ { false };
    juce::String availableVersion_;
    juce::String availableReleaseUrl_;
    juce::String availableAssetUrl_;
    juce::String availableReleaseNotes_;

    struct PendingEndAction
    {
        double executeTs { 0.0 };
        juce::String mode;
        juce::String col;
        juce::String layer;
        juce::String clip;
        juce::String group;
    };
    std::map<std::pair<int, int>, PendingEndAction> pendingEndActions_;
    struct SendTargetRuntimeState
    {
        bool ok { false };
        juce::String detail { "-" };
        juce::int64 lastTxMs { 0 };
    };
    std::map<juce::String, SendTargetRuntimeState> sendTargetRuntimeStates_;

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
    std::unique_ptr<FpsIndicatorStrip> fpsIndicatorStrip_;
    StatusBarComponent statusBar_;
    juce::Label sourceHeaderLabel_    { {}, "Source" };
    juce::Label triggerOutHeaderLabel_{ {}, "Trigger Out" };
    juce::Label outLtcHeaderLabel_    { {}, "Out LTC" };
    juce::Label resolumeHeader_       { {}, "Resolume Settings" };
    ExpandCircleButton sourceExpandBtn_;
    ExpandCircleButton triggerOutExpandBtn_;
    ExpandCircleButton outLtcExpandBtn_;
    ExpandCircleButton resolumeExpandBtn_;
    bool sourceExpanded_   { true };
    bool triggerOutExpanded_ { false };
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
    juce::Label outConvertLbl_     { {}, "Convert FPS:" };
    juce::Label outOffsetLbl_      { {}, "Offset (frames):" };
    juce::Label outLevelLbl_       { {}, "Output Gain:" };
    juce::Label ltcThruLbl_        { {}, "Thru" };
    juce::Label resAdapterLbl_     { {}, "Adapter:" };
    juce::Label resSendIpLbl_      { {}, "Send 1:" };
    juce::Label resSendIpLbl2_     { {}, "Send 2:" };
    juce::Label resSendIpLbl3_     { {}, "Send 3:" };
    juce::Label resSendIpLbl4_     { {}, "Send 4:" };
    juce::Label resSendIpLbl5_     { {}, "Send 5:" };
    juce::Label resSendIpLbl6_     { {}, "Send 6:" };
    juce::Label resSendIpLbl7_     { {}, "Send 7:" };
    juce::Label resSendIpLbl8_     { {}, "Send 8:" };
    juce::Label resSendAdapterLbl1_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl2_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl3_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl4_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl5_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl6_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl7_{ {}, "Adapter:" };
    juce::Label resSendAdapterLbl8_{ {}, "Adapter:" };
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
    FpsConvertStrip ltcConvertStrip_ { { FrameRate::FPS_2398, FrameRate::FPS_24, FrameRate::FPS_25, FrameRate::FPS_2997, FrameRate::FPS_30 } };
    juce::TextEditor ltcOffsetEditor_;
    juce::Slider ltcOutLevelSlider_;
    MacSwitch ltcOutSwitch_;
    DotToggle ltcThruDot_;

    juce::ComboBox resolumeAdapterCombo_;
    ExpandCircleButton resolumeSendExpandBtn1_;
    ExpandCircleButton resolumeSendExpandBtn2_;
    ExpandCircleButton resolumeSendExpandBtn3_;
    ExpandCircleButton resolumeSendExpandBtn4_;
    ExpandCircleButton resolumeSendExpandBtn5_;
    ExpandCircleButton resolumeSendExpandBtn6_;
    ExpandCircleButton resolumeSendExpandBtn7_;
    ExpandCircleButton resolumeSendExpandBtn8_;
    juce::ComboBox resolumeSendAdapterCombo1_;
    juce::ComboBox resolumeSendAdapterCombo2_;
    juce::ComboBox resolumeSendAdapterCombo3_;
    juce::ComboBox resolumeSendAdapterCombo4_;
    juce::ComboBox resolumeSendAdapterCombo5_;
    juce::ComboBox resolumeSendAdapterCombo6_;
    juce::ComboBox resolumeSendAdapterCombo7_;
    juce::ComboBox resolumeSendAdapterCombo8_;
    juce::TextEditor resolumeSendIp_;
    juce::TextEditor resolumeSendPort_;
    juce::TextEditor resolumeSendIp2_;
    juce::TextEditor resolumeSendPort2_;
    juce::TextEditor resolumeSendIp3_;
    juce::TextEditor resolumeSendPort3_;
    juce::TextEditor resolumeSendIp4_;
    juce::TextEditor resolumeSendPort4_;
    juce::TextEditor resolumeSendIp5_;
    juce::TextEditor resolumeSendPort5_;
    juce::TextEditor resolumeSendIp6_;
    juce::TextEditor resolumeSendPort6_;
    juce::TextEditor resolumeSendIp7_;
    juce::TextEditor resolumeSendPort7_;
    juce::TextEditor resolumeSendIp8_;
    juce::TextEditor resolumeSendPort8_;
    trigger::FlatIconButton resolumeAddTargetBtn_  { "+" };
    trigger::FlatIconButton resolumeDelTargetBtn2_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn3_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn4_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn5_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn6_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn7_ { "-" };
    trigger::FlatIconButton resolumeDelTargetBtn8_ { "-" };
    juce::TextEditor resolumeListenIp_;
    juce::TextEditor resolumeListenPort_;
    juce::TextEditor resolumeMaxLayers_;
    juce::TextEditor resolumeMaxClips_;
    juce::TextEditor resolumeGlobalOffset_;
    int resolumeSendTargetCount_ { 1 };
    bool resolumeSendExpanded1_ { true };
    bool resolumeSendExpanded2_ { false };
    bool resolumeSendExpanded3_ { false };
    bool resolumeSendExpanded4_ { false };
    bool resolumeSendExpanded5_ { false };
    bool resolumeSendExpanded6_ { false };
    bool resolumeSendExpanded7_ { false };
    bool resolumeSendExpanded8_ { false };
    juce::TextButton getTriggersBtn_  { "Get Clips" };
    juce::TextButton createCustomBtn_ { "Create Custom Triggers" };
    juce::TextButton fileMenuBtn_     { "File" };
    juce::TextButton manageMenuBtn_   { "Manage" };
    juce::TextButton viewMenuBtn_     { "View" };
    juce::TextButton helpMenuBtn_     { "Help" };
    juce::TextButton settingsButton_  { "Settings" };
    juce::TextButton quitButton_      { "Quit" };

    juce::TableListBox triggerTable_;
    juce::Array<juce::Rectangle<int>> leftRowRects_;
    juce::Array<juce::Rectangle<int>> sectionRowRects_;
    juce::Array<juce::Rectangle<int>> rightSectionRects_;
    juce::Rectangle<int> headerRect_;
    juce::Rectangle<int> menuBarRect_;
    juce::Rectangle<int> timerRect_;
    juce::Rectangle<int> leftViewportRect_;
    SettingsPanelComponent settingsPanel_ { getTriggersBtn_, createCustomBtn_ };
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
    bool includeClipsWithOffset_ { true };
    bool includeClipsWithoutOffset_ { false };
    bool clipRefreshQueued_ { false };
    juce::int64 clipRefreshDueMs_ { 0 };
    bool clipFeedbackDirty_ { false };
    juce::int64 clipFeedbackDueMs_ { 0 };
    juce::int64 clipImportQuietUntilMs_ { 0 };
    int rawOscDiagnosticLogsRemaining_ { 0 };
    bool rawOscDiagnosticSuppressedLogged_ { false };
    bool oscListenOk_ { false };
    bool oscSendOk_ { false };
    juce::String oscListenState_ { "idle" };
    juce::String oscSendState_ { "idle" };
    juce::String oscListenDetail_ { "-" };
    juce::String oscSendDetail_ { "-" };
    juce::int64 lastOscInMs_ { 0 };
    juce::int64 lastOscOutMs_ { 0 };
    int lastOscInPort_ { 0 };
    int lastOscOutPort_ { 0 };
    bool colWidthGuard_      { false };

    std::vector<trigger::model::CustomTriggerGroup> customGroups_;
    bool dragActive_ { false };
    bool dragGroup_ { false };
    int dragIdentifier_ { 0 };
    int dragHoverRow_ { -1 };

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
