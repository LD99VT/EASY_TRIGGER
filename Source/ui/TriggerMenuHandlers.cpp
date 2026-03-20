// Included as a separate translation unit.
// Contains: openGetClipsOptions, queryResolume, resetSettings,
//           clearCustomTriggers, clearClipTriggers, clearAllGroups,
//           expandAllGroups, resetTableLayout,
//           openFileMenu, openManageMenu, openViewMenu,
//           openHelpMenu, openSettingsMenu, openPreferencesWindow
#include "../TriggerMainWindow.h"

namespace trigger
{
namespace
{
#include "windows/DarkDialog.h"
#include "windows/ColourPickerWindow.h"
#include "windows/GetClipsOptionsWindow.h"
#include "windows/PreferencesWindow.h"
#include "windows/AboutWindow.h"

constexpr int kConfigModeSettings = 1;
constexpr int kConfigModeClips    = 2;
constexpr int kConfigModeAll      = 3;

juce::File getRuntimePrefsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("EasyTrigger")
        .getChildFile ("runtime_prefs.json");
}
} // anonymous namespace

void TriggerContentComponent::openGetClipsOptions()
{
    if (getClipsOptionsWindow_ != nullptr)
    {
        logGetClipsDiagnostic ("Get Clips window already open; focusing existing window");
        getClipsOptionsWindow_->toFront (true);
        return;
    }

    logGetClipsDiagnostic ("Get Clips button/menu pressed; opening selection window");
    getClipsOptionsWindow_ = new GetClipsOptionsWindow (includeClipsWithOffset_,
                                                        includeClipsWithoutOffset_,
                                                        [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (bool includeWithOffset, bool includeWithoutOffset)
                                                        {
                                                            if (safe != nullptr)
                                                            {
                                                                safe->logGetClipsDiagnostic ("Get Clips selection applied: withOffset="
                                                                    + juce::String (includeWithOffset ? "true" : "false")
                                                                    + ", withoutOffset="
                                                                    + juce::String (includeWithoutOffset ? "true" : "false"));
                                                                safe->queryResolume (includeWithOffset, includeWithoutOffset);
                                                            }
                                                        },
                                                        getParentComponent());
}

void TriggerContentComponent::queryResolume (bool includeClipsWithOffset, bool includeClipsWithoutOffset)
{
    juce::String err;
    includeClipsWithOffset_ = includeClipsWithOffset;
    includeClipsWithoutOffset_ = includeClipsWithoutOffset;
    clipReceiveEnabled_ = true;
    clipRefreshQueued_ = false;
    clipRefreshDueMs_ = 0;
    clipFeedbackDirty_ = false;
    clipFeedbackDueMs_ = 0;
    clipImportQuietUntilMs_ = 0;
    rawOscDiagnosticLogsRemaining_ = 40;
    rawOscDiagnosticSuppressedLogged_ = false;
    deletedLayers_.clear();
    clipCollector_.clear();
    const auto listenIp = resolumeListenIp_.getText().trim().isNotEmpty() ? resolumeListenIp_.getText().trim() : "127.0.0.1";
    logGetClipsDiagnostic ("Get Clips query start: listenIp=" + listenIp
        + ", listenPort=" + juce::String (juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()))
        + ", includeWithOffset=" + juce::String (includeClipsWithOffset ? "true" : "false")
        + ", includeWithoutOffset=" + juce::String (includeClipsWithoutOffset ? "true" : "false"));
    if (! clipCollector_.startListening (listenIp, juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()), err))
    {
        clipReceiveEnabled_ = false;
        oscListenOk_ = false;
        oscListenState_ = "error";
        oscListenDetail_ = "port " + juce::String (juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()))
            + " busy/unavailable";
        updateNetworkStatusIndicator();
        logGetClipsDiagnostic ("startListening failed: " + err);
        setResolumeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        return;
    }
    oscListenOk_ = true;
    oscListenState_ = "ok";
    oscListenDetail_ = "listening " + listenIp + ":" + juce::String (juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()));
    updateNetworkStatusIndicator();
    auto targets = collectResolumeSendTargets();
    if (targets.empty())
        targets.push_back ({ "0.0.0.0", "127.0.0.1", 7000 });
    for (const auto& target : targets)
        updateSendTargetRuntimeState (target, false, "idle (" + describeResolumeSendTarget (target) + ")");

    const int configuredListenPort = juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue());
    if (configuredListenPort == targets.front().port)
    {
        clipReceiveEnabled_ = false;
        updateSendTargetRuntimeState (targets.front(), false, "listen/send port conflict");
        oscSendOk_ = false;
        oscSendState_ = "error";
        oscSendDetail_ = "listen port matches send port";
        updateNetworkStatusIndicator();
        const juce::String errText = "Resolume listen port must be different from send port";
        logGetClipsDiagnostic (errText + ": listenPort=" + juce::String (configuredListenPort)
            + ", sendPort=" + juce::String (targets.front().port));
        setResolumeStatusText (errText, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        DarkDialog::show ("Easy Trigger",
                          "Listen port must be different from Send port.\n"
                          "Example: Send 7000, Listen 7001 or 7002.",
                          getParentComponent());
        return;
    }

    logGetClipsDiagnostic ("configureSender target: localBindIp=" + targets.front().localBindIp
        + ", sendIp=" + targets.front().ip
        + ", sendPort=" + juce::String (targets.front().port));
    if (! clipCollector_.configureSender (targets.front().localBindIp, targets.front().ip, targets.front().port, err))
    {
        clipReceiveEnabled_ = false;
        updateSendTargetRuntimeState (targets.front(), false, "query sender failed (" + describeResolumeSendTarget (targets.front()) + ")");
        oscSendOk_ = false;
        oscSendState_ = "error";
        oscSendDetail_ = "send failed to " + targets.front().ip + ":" + juce::String (targets.front().port);
        updateNetworkStatusIndicator();
        logGetClipsDiagnostic ("configureSender failed: " + err);
        setResolumeStatusText (err, juce::Colour::fromRGB (0xde, 0x9b, 0x3c));
        return;
    }
    updateSendTargetRuntimeState (targets.front(), true, "query sender ready (" + describeResolumeSendTarget (targets.front()) + ")");
    oscSendOk_ = true;
    oscSendState_ = "ok";
    oscSendDetail_ = "send " + targets.front().ip + ":" + juce::String (targets.front().port);
    lastOscOutPort_ = targets.front().port;
    updateNetworkStatusIndicator();
    const int maxLayers = juce::jlimit (1, 64, resolumeMaxLayers_.getText().getIntValue());
    const int maxClips = juce::jlimit (1, 256, resolumeMaxClips_.getText().getIntValue());
    logGetClipsDiagnostic ("Sending Resolume query burst: maxLayers=" + juce::String (maxLayers)
        + ", maxClips=" + juce::String (maxClips));
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
    // refreshLtcDeviceListsByDriver already selects id=1 (first device); leave it
    ltcInChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcInSampleRateCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcInGainSlider_.setValue (0.0, juce::dontSendNotification);

    // LTC Out
    // refreshLtcDeviceListsByDriver already selects id=1 for out device; leave it
    ltcOutChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutSampleRateCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcConvertStrip_.setSelectedFps (std::nullopt);
    ltcOffsetEditor_.setText  ("00:00:00:00", juce::dontSendNotification);
    ltcOutLevelSlider_.setValue (0.0, juce::dontSendNotification);
    ltcOutSwitch_.setState (false);
    ltcThruDot_.setState   (false);

    // MTC / ArtNet / OSC
    mtcInCombo_.setSelectedId      (mtcInCombo_.getNumItems() > 0 ? 1 : 0,      juce::dontSendNotification);
    artnetInCombo_.setSelectedId   (artnetInCombo_.getNumItems() > 0 ? 1 : 0,   juce::dontSendNotification);
    oscAdapterCombo_.setSelectedId (oscAdapterCombo_.getNumItems() > 0 ? 1 : 0, juce::dontSendNotification);
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
    resolumeSendAdapterCombo6_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo7_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendAdapterCombo8_.setSelectedId (1, juce::dontSendNotification);
    resolumeSendIp_.setText        ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort_.setText      ("7000",       juce::dontSendNotification);
    resolumeSendTargetCount_ = 1;
    resolumeSendExpanded1_ = true;
    resolumeSendExpanded2_ = false;
    resolumeSendExpanded3_ = false;
    resolumeSendExpanded4_ = false;
    resolumeSendExpanded5_ = false;
    resolumeSendExpanded6_ = false;
    resolumeSendExpanded7_ = false;
    resolumeSendExpanded8_ = false;
    resolumeSendExpandBtn1_.setExpanded (resolumeSendExpanded1_);
    resolumeSendExpandBtn2_.setExpanded (resolumeSendExpanded2_);
    resolumeSendExpandBtn3_.setExpanded (resolumeSendExpanded3_);
    resolumeSendExpandBtn4_.setExpanded (resolumeSendExpanded4_);
    resolumeSendExpandBtn5_.setExpanded (resolumeSendExpanded5_);
    resolumeSendExpandBtn6_.setExpanded (resolumeSendExpanded6_);
    resolumeSendExpandBtn7_.setExpanded (resolumeSendExpanded7_);
    resolumeSendExpandBtn8_.setExpanded (resolumeSendExpanded8_);
    resolumeSendIp2_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort2_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp3_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort3_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp4_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort4_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp5_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort5_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp6_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort6_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp7_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort7_.setText     ("7000",       juce::dontSendNotification);
    resolumeSendIp8_.setText       ("127.0.0.1", juce::dontSendNotification);
    resolumeSendPort8_.setText     ("7000",       juce::dontSendNotification);
    resolumeListenIp_.setText      ("127.0.0.1", juce::dontSendNotification);
    resolumeListenPort_.setText    ("7001",       juce::dontSendNotification);
    resolumeMaxLayers_.setText     ("4",          juce::dontSendNotification);
    resolumeMaxClips_.setText      ("32",         juce::dontSendNotification);
    resolumeGlobalOffset_.setText  ("00:00:00:00", juce::dontSendNotification);
    syncResolumeListenIpWithAdapter();

    triggerRows_.erase (std::remove_if (triggerRows_.begin(), triggerRows_.end(),
                                        [] (const auto& clip) { return clip.isCustom; }),
                        triggerRows_.end());
    customGroups_.clear();
    layerOrder_.clear();
    currentTriggerKeys_.clear();
    pendingEndActions_.clear();
    triggerRangeActive_.clear();
    rebuildDisplayRows();
    triggerTable_.updateContent();
    updateWindowHeight (false);  // relax resize limits before removeCustomColumns
    ensureCustomColumnsState();
    triggerTable_.repaint();

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

void TriggerContentComponent::clearCustomTriggers()
{
    if (! hasCustomGroup())
        return;

    while (! customGroups_.empty())
        deleteCustomGroup (customGroups_.front().id);
    setTimecodeStatusText ("Custom triggers cleared", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
}

void TriggerContentComponent::clearClipTriggers()
{
    clipReceiveEnabled_ = false;
    deletedLayers_.clear();
    clipCollector_.clear();
    for (auto it = triggerRows_.begin(); it != triggerRows_.end();)
        it = (! it->isCustom) ? triggerRows_.erase (it) : ++it;
    for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        it = (it->first != 0) ? currentTriggerKeys_.erase (it) : ++it;
    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
        it = (it->first.first != 0) ? pendingEndActions_.erase (it) : ++it;
    for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
        it = (it->first.first != 0) ? triggerRangeActive_.erase (it) : ++it;
    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
    updateWindowHeight (false);
    resized();
    repaint();
    setTimecodeStatusText ("Clip triggers cleared", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
}

void TriggerContentComponent::clearAllGroups()
{
    // Remove all custom rows and groups without triggering intermediate rebuilds
    triggerRows_.erase (std::remove_if (triggerRows_.begin(), triggerRows_.end(),
                                        [] (const TriggerClip& c) { return c.isCustom; }),
                        triggerRows_.end());
    for (const auto& group : customGroups_)
    {
        const int layer = trigger::model::layerFromGroupId (group.id);
        currentTriggerKeys_.erase ({ layer, 0 });
        for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
            it = (it->first == layer) ? currentTriggerKeys_.erase (it) : ++it;
        for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
            it = (it->first.first == layer) ? pendingEndActions_.erase (it) : ++it;
        for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
            it = (it->first.first == layer) ? triggerRangeActive_.erase (it) : ++it;
        layerExpanded_.erase (layer);
        layerEnabled_.erase (layer);
    }
    customGroups_.clear();
    layerOrder_.clear();

    // Also clear clip triggers
    clipReceiveEnabled_ = false;
    deletedLayers_.clear();
    clipCollector_.clear();
    for (auto it = triggerRows_.begin(); it != triggerRows_.end();)
        it = (! it->isCustom) ? triggerRows_.erase (it) : ++it;
    for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        it = (it->first != 0) ? currentTriggerKeys_.erase (it) : ++it;
    for (auto it = pendingEndActions_.begin(); it != pendingEndActions_.end();)
        it = (it->first.first != 0) ? pendingEndActions_.erase (it) : ++it;
    for (auto it = triggerRangeActive_.begin(); it != triggerRangeActive_.end();)
        it = (it->first.first != 0) ? triggerRangeActive_.erase (it) : ++it;

    // Single rebuild at the end
    rebuildDisplayRows();
    triggerTable_.updateContent();
    // Relax resize limits BEFORE removing custom columns so that
    // removeCustomColumns() → setSize() is not silently clamped by the old minW.
    updateWindowHeight (false);
    ensureCustomColumnsState();
    triggerTable_.repaint();
    resized();
    repaint();
    setTimecodeStatusText ("All triggers cleared", juce::Colour::fromRGB (0xec, 0x48, 0x3c));
}

void TriggerContentComponent::expandAllGroups (bool expanded)
{
    for (auto& [layer, value] : layerExpanded_)
        value = expanded;
    syncCustomGroupStateFromLayers();
    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::resetTableLayout()
{
    auto& h = triggerTable_.getHeader();
    h.removeAllColumns();
    h.addColumn ("",           1,  30,  30);
    h.addColumn ("In",         2,  34,  34);
    h.addColumn ("Name",       3, 150, 150);
    h.addColumn ("Count",      4,  92,  92);
    h.addColumn ("Range",      5,  58,  58);
    h.addColumn ("Trigger",    6,  92,  92);
    h.addColumn ("Duration",   7,  92,  92);
    h.addColumn ("End Action", 8, 110, 110);
    h.addColumn ("Send",       9,  84,  84);
    h.addColumn ("Test",      10,  56,  56, 56, juce::TableHeaderComponent::notResizable);
    ensureCustomColumnsState();
    updateTableColumnWidths();
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::openFileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save Settings...");
    m.addItem (2, "Save Clips...");
    m.addItem (3, "Save All...");
    m.addSeparator();
    m.addItem (4, "Load Settings...");
    m.addItem (5, "Load Clips...");
    m.addItem (6, "Load All...");
    m.addSeparator();
    m.addItem (7, "Rescan Audio Devices");
    m.addItem (8, "Reset Settings");
    m.addSeparator();
    m.addItem (9, "Load Last Config", true, autoLoadOnStartup_);
    m.addItem (10, "Close To Tray", true, closeToTray_);
    m.addSeparator();
    m.addItem (11, "Quit");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileMenuBtn_),
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
                             case 8: safe->resetSettings(); break;
                             case 9:
                                 safe->autoLoadOnStartup_ = ! safe->autoLoadOnStartup_;
                                 safe->saveRuntimePrefs();
                                 break;
                             case 10:
                                 safe->closeToTray_ = ! safe->closeToTray_;
                                 safe->saveRuntimePrefs();
                                 break;
                             case 11:
                                 if (auto* window = safe->findParentComponentOfClass<MainWindow>())
                                     window->quitFromTray();
                                 else
                                     juce::JUCEApplication::getInstance()->systemRequestedQuit();
                                 break;
                             default: break;
                         }
                     });
}

void TriggerContentComponent::openManageMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Get Clips");
    m.addItem (2, "Create Custom Triggers", ! hasCustomGroupsAtLimit());
    m.addSeparator();
    m.addItem (3, "Update Clips");
    m.addSeparator();
    m.addItem (4, "Clear Clips", ! triggerRows_.empty());
    m.addItem (5, "Clear Custom Groups", hasCustomGroup());
    m.addItem (6, "Clear All", ! displayRows_.empty());

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&manageMenuBtn_),
                     [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (int result)
                     {
                         if (safe == nullptr)
                             return;
                         switch (result)
                         {
                             case 1: safe->openGetClipsOptions(); break;
                             case 2: safe->addCustomColTrigger(); break;
                             case 3: safe->queryResolume (safe->includeClipsWithOffset_, safe->includeClipsWithoutOffset_); break;
                             case 4: safe->clearClipTriggers(); break;
                             case 5: safe->clearCustomTriggers(); break;
                             case 6: safe->clearAllGroups(); break;
                             default: break;
                         }
                     });
}

void TriggerContentComponent::openViewMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Expand All Groups");
    m.addItem (2, "Collapse All Groups");
    m.addSeparator();
    m.addItem (3, "Reset Table Layout");
    m.addSeparator();
    m.addItem (4, "Preferences...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&viewMenuBtn_),
                     [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (int result)
                     {
                         if (safe == nullptr)
                             return;
                         if (result == 1) safe->expandAllGroups (true);
                         if (result == 2) safe->expandAllGroups (false);
                         if (result == 3) safe->resetTableLayout();
                         if (result == 4) safe->openPreferencesWindow();
                     });
}

void TriggerContentComponent::openPreferencesWindow()
{
    if (preferencesWindow_ != nullptr)
    {
        preferencesWindow_->toFront (true);
        return;
    }

    auto* win = PreferencesWindow::show (
        kClipFired, kClipConnectedTc, kClipConnectedPlain, kClipConnectedCustom,
        [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
        (juce::Colour fired, juce::Colour tc, juce::Colour plain, juce::Colour custom)
        {
            if (safe == nullptr) return;
            kClipFired           = fired;
            kClipConnectedTc     = tc;
            kClipConnectedPlain  = plain;
            kClipConnectedCustom = custom;
            safe->triggerTable_.repaint();
            safe->saveRuntimePrefs();
        },
        getParentComponent());

    preferencesWindow_ = win;
}

void TriggerContentComponent::openHelpMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Open Help");
    m.addItem (2, "Open Config Folder");
    m.addItem (3, "About");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&helpMenuBtn_),
                     [safe = juce::Component::SafePointer<TriggerContentComponent> (this)] (int result)
                     {
                         if (safe == nullptr)
                             return;
                         if (result == 1)
                         {
                             safe->openHelpPage();
                         }
                         else if (result == 2)
                         {
                             getRuntimePrefsFile().getParentDirectory().createDirectory();
                             getRuntimePrefsFile().getParentDirectory().startAsProcess();
                         }
                         else if (result == 3)
                         {
                             auto* parent = safe.getComponent();
                             if (parent == nullptr)
                                 return;
                             AboutDialog::show (parent, safe->headerBold_, safe->headerLight_);
                         }
                     });
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
    m.addItem (8, "Load Last Config", true, autoLoadOnStartup_);
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
                                     while (! safe->customGroups_.empty())
                                         safe->deleteCustomGroup (safe->customGroups_.front().id);
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
                                 safe->setTimecodeStatusText (safe->autoLoadOnStartup_ ? "Load Last Config ON" : "Load Last Config OFF",
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

} // namespace trigger
