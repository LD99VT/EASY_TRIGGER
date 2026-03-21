// Included as a separate translation unit.
// Contains: loadRuntimePrefs, saveRuntimePrefs, maybeAutoLoadConfig,
//           saveConfigAs, loadConfigFrom, saveConfigToFile, loadConfigFromFile
#include "../TriggerMainWindow.h"
#include "ConfigStore.h"

namespace trigger
{
namespace
{
constexpr int kConfigModeSettings = 1;
constexpr int kConfigModeClips    = 2;
constexpr int kConfigModeAll      = 3;

juce::String configModeKey (int modeId)
{
    if (modeId == kConfigModeSettings) return "settings";
    if (modeId == kConfigModeClips) return "clips";
    return "all";
}

juce::File getRuntimePrefsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("EasyTrigger")
        .getChildFile ("runtime_prefs.json");
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

#include "../ui/widgets/TrayIcon.h"

} // anonymous namespace

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

            auto loadCol = [&] (const char* key, juce::Colour& dst)
            {
                if (obj->hasProperty (key))
                    dst = juce::Colour ((juce::uint32) (juce::int64) obj->getProperty (key));
            };
            loadCol ("colour_fired",  kClipFired);
            loadCol ("colour_tc",     kClipConnectedTc);
            loadCol ("colour_plain",  kClipConnectedPlain);
            loadCol ("colour_custom", kClipConnectedCustom);
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
    obj->setProperty ("colour_fired",  (juce::int64) kClipFired.getARGB());
    obj->setProperty ("colour_tc",     (juce::int64) kClipConnectedTc.getARGB());
    obj->setProperty ("colour_plain",  (juce::int64) kClipConnectedPlain.getARGB());
    obj->setProperty ("colour_custom", (juce::int64) kClipConnectedCustom.getARGB());
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
    syncCustomGroupStateFromLayers();

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
        leftObj->setProperty ("res_send_adapter_6", resolumeSendAdapterCombo6_.getText());
        leftObj->setProperty ("res_send_ip_6", resolumeSendIp6_.getText());
        leftObj->setProperty ("res_send_port_6", resolumeSendPort6_.getText());
        leftObj->setProperty ("res_send_expanded_6", resolumeSendExpanded6_);
        leftObj->setProperty ("res_send_adapter_7", resolumeSendAdapterCombo7_.getText());
        leftObj->setProperty ("res_send_ip_7", resolumeSendIp7_.getText());
        leftObj->setProperty ("res_send_port_7", resolumeSendPort7_.getText());
        leftObj->setProperty ("res_send_expanded_7", resolumeSendExpanded7_);
        leftObj->setProperty ("res_send_adapter_8", resolumeSendAdapterCombo8_.getText());
        leftObj->setProperty ("res_send_ip_8", resolumeSendIp8_.getText());
        leftObj->setProperty ("res_send_port_8", resolumeSendPort8_.getText());
        leftObj->setProperty ("res_send_expanded_8", resolumeSendExpanded8_);
        leftObj->setProperty ("res_listen_ip", resolumeListenIp_.getText());
        leftObj->setProperty ("res_listen_port", resolumeListenPort_.getText());
        leftObj->setProperty ("res_max_layers", resolumeMaxLayers_.getText());
        leftObj->setProperty ("res_max_clips", resolumeMaxClips_.getText());
        leftObj->setProperty ("res_global_offset", resolumeGlobalOffset_.getText());
        rootObj->setProperty ("left", juce::var (leftObj));
    }

    if (modeId == kConfigModeClips || modeId == kConfigModeAll)
    {
        normaliseLayerOrder();
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
            rowObj->setProperty ("end_action_group", clip.endActionGroup);
            rowObj->setProperty ("is_custom", clip.isCustom);
            rowObj->setProperty ("send_target", clip.sendTargetIndex);
            rowObj->setProperty ("order_index", clip.orderIndex);
            if (clip.isCustom)
            {
                rowObj->setProperty ("custom_group_id", clip.customGroupId);
                rowObj->setProperty ("custom_clip_id", clip.customClipId);
                rowObj->setProperty ("custom_type", clip.customType);
                rowObj->setProperty ("custom_source_col", clip.customSourceCol);
                rowObj->setProperty ("custom_source_layer", clip.customSourceLayer);
                rowObj->setProperty ("custom_source_clip", clip.customSourceClip);
                rowObj->setProperty ("custom_source_group", clip.customSourceGroup);
            }
            rows.add (juce::var (rowObj));
        }
        rootObj->setProperty ("clips", juce::var (rows));

        juce::Array<juce::var> groupOrders;
        std::vector<std::pair<int, int>> orderedLayers;
        orderedLayers.reserve (layerOrder_.size());
        for (const auto& [layer, order] : layerOrder_)
            orderedLayers.emplace_back (layer, order);
        std::sort (orderedLayers.begin(), orderedLayers.end(), [] (const auto& a, const auto& b)
        {
            if (a.second != b.second)
                return a.second < b.second;
            return a.first < b.first;
        });
        for (const auto& [layer, order] : orderedLayers)
        {
            auto* groupOrderObj = new juce::DynamicObject();
            groupOrderObj->setProperty ("layer", layer);
            groupOrderObj->setProperty ("order", order);
            groupOrders.add (juce::var (groupOrderObj));
        }
        rootObj->setProperty ("group_orders", juce::var (groupOrders));
    }

    if (modeId == kConfigModeSettings || modeId == kConfigModeAll)
        rootObj->setProperty ("table_columns", juce::var (trigger::model::serialiseTableColumns (triggerTable_.getHeader())));

    if (modeId == kConfigModeClips || modeId == kConfigModeAll)
    {
        juce::Array<juce::var> customGroups;
        for (const auto& group : customGroups_)
        {
            auto* groupObj = new juce::DynamicObject();
            groupObj->setProperty ("id", group.id);
            groupObj->setProperty ("name", group.name);
            groupObj->setProperty ("expanded", group.expanded);
            groupObj->setProperty ("enabled", group.enabled);
            groupObj->setProperty ("order", group.orderIndex);
            customGroups.add (juce::var (groupObj));
        }
        rootObj->setProperty ("custom_groups", juce::var (customGroups));
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

    juce::Array<juce::var> savedTableColumns;
    if (rootObj->hasProperty ("table_columns"))
        if (auto* arr = rootObj->getProperty ("table_columns").getArray())
            savedTableColumns = *arr;

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
            resolumeSendTargetCount_ = juce::jlimit (1, 8, (int) leftObj->getProperty ("res_send_target_count"));
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
            resolumeSendIp6_.setText (leftObj->hasProperty ("res_send_ip_6") ? leftObj->getProperty ("res_send_ip_6").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort6_.setText (leftObj->hasProperty ("res_send_port_6") ? leftObj->getProperty ("res_send_port_6").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo6_, leftObj->hasProperty ("res_send_adapter_6") ? leftObj->getProperty ("res_send_adapter_6").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded6_ = leftObj->hasProperty ("res_send_expanded_6") ? (bool) leftObj->getProperty ("res_send_expanded_6") : false;
            resolumeSendIp7_.setText (leftObj->hasProperty ("res_send_ip_7") ? leftObj->getProperty ("res_send_ip_7").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort7_.setText (leftObj->hasProperty ("res_send_port_7") ? leftObj->getProperty ("res_send_port_7").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo7_, leftObj->hasProperty ("res_send_adapter_7") ? leftObj->getProperty ("res_send_adapter_7").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded7_ = leftObj->hasProperty ("res_send_expanded_7") ? (bool) leftObj->getProperty ("res_send_expanded_7") : false;
            resolumeSendIp8_.setText (leftObj->hasProperty ("res_send_ip_8") ? leftObj->getProperty ("res_send_ip_8").toString() : juce::String ("127.0.0.1"), juce::dontSendNotification);
            resolumeSendPort8_.setText (leftObj->hasProperty ("res_send_port_8") ? leftObj->getProperty ("res_send_port_8").toString() : juce::String ("7000"), juce::dontSendNotification);
            setComboText (resolumeSendAdapterCombo8_, leftObj->hasProperty ("res_send_adapter_8") ? leftObj->getProperty ("res_send_adapter_8").toString()
                                                                                                   : juce::String ("ALL INTERFACES (0.0.0.0)"));
            resolumeSendExpanded8_ = leftObj->hasProperty ("res_send_expanded_8") ? (bool) leftObj->getProperty ("res_send_expanded_8") : false;
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
            resolumeSendExpandBtn6_.setExpanded (resolumeSendExpanded6_);
            resolumeSendExpandBtn7_.setExpanded (resolumeSendExpanded7_);
            resolumeSendExpandBtn8_.setExpanded (resolumeSendExpanded8_);
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
        customGroups_.clear();
        layerOrder_.clear();
        currentTriggerKeys_.clear();
        pendingEndActions_.clear();
        triggerRangeActive_.clear();
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
                    clip.endActionGroup = rowObj->getProperty ("end_action_group").toString();
                    clip.isCustom = rowObj->hasProperty ("is_custom") && (bool) rowObj->getProperty ("is_custom");
                    clip.sendTargetIndex = rowObj->hasProperty ("send_target")
                        ? (int) rowObj->getProperty ("send_target")
                        : 0;
                    clip.sendTargetIndex = clampSendTargetIndex (clip.sendTargetIndex);
                    clip.orderIndex = rowObj->hasProperty ("order_index")
                        ? (int) rowObj->getProperty ("order_index")
                        : clip.clip - 1;
                    clip.hasOffset = rowObj->hasProperty ("has_offset")
                        ? (bool) rowObj->getProperty ("has_offset")
                        : clip.isCustom;
                    if (clip.isCustom)
                        clip.hasOffset = true;
                    if (clip.isCustom)
                    {
                        clip.customGroupId = rowObj->hasProperty ("custom_group_id")
                            ? (int) rowObj->getProperty ("custom_group_id")
                            : 1;
                        clip.customClipId = rowObj->hasProperty ("custom_clip_id")
                            ? (int) rowObj->getProperty ("custom_clip_id")
                            : clip.clip;
                        clip.customType = rowObj->getProperty ("custom_type").toString();
                        if (clip.customType.isEmpty()) clip.customType = "col";
                        clip.customSourceCol   = rowObj->getProperty ("custom_source_col").toString();
                        clip.customSourceLayer = rowObj->getProperty ("custom_source_layer").toString();
                        clip.customSourceClip  = rowObj->getProperty ("custom_source_clip").toString();
                        clip.customSourceGroup = rowObj->getProperty ("custom_source_group").toString();
                        clip.layer = trigger::model::layerFromGroupId (clip.customGroupId);
                    }
                    triggerRows_.push_back (clip);
                }
            }
        }

        if (rootObj->hasProperty ("custom_groups"))
        {
            if (auto* arr = rootObj->getProperty ("custom_groups").getArray())
            {
                for (const auto& entry : *arr)
                {
                    auto* obj = entry.getDynamicObject();
                    if (obj == nullptr)
                        continue;
                    customGroups_.push_back ({
                        (int) obj->getProperty ("id"),
                        obj->getProperty ("name").toString(),
                        obj->hasProperty ("expanded") ? (bool) obj->getProperty ("expanded") : true,
                        obj->hasProperty ("enabled") ? (bool) obj->getProperty ("enabled") : true,
                        obj->hasProperty ("order") ? (int) obj->getProperty ("order") : (int) customGroups_.size()
                    });
                }
            }
        }

        if (rootObj->hasProperty ("group_orders"))
        {
            if (auto* arr = rootObj->getProperty ("group_orders").getArray())
            {
                for (const auto& entry : *arr)
                {
                    auto* obj = entry.getDynamicObject();
                    if (obj == nullptr)
                        continue;
                    layerOrder_[(int) obj->getProperty ("layer")] = (int) obj->getProperty ("order");
                }
            }
        }

        if (customGroups_.empty())
        {
            bool hasLegacyCustom = false;
            for (const auto& clip : triggerRows_)
                if (clip.isCustom)
                    hasLegacyCustom = true;
            if (hasLegacyCustom)
            {
                const auto legacyName = rootObj->hasProperty ("custom_group_name")
                    ? rootObj->getProperty ("custom_group_name").toString()
                    : juce::String ("Custom Trigger");
                customGroups_.push_back ({ 1, legacyName, true, true, 0 });
                for (auto& clip : triggerRows_)
                {
                    if (! clip.isCustom)
                        continue;
                    clip.customGroupId = 1;
                    clip.layer = trigger::model::layerFromGroupId (1);
                    if (clip.customClipId <= 0)
                        clip.customClipId = clip.clip;
                }
            }
        }

        trigger::model::normaliseGroupOrder (customGroups_);
        syncLayerStateFromCustomGroups();
        normaliseLayerOrder();
        ensureCustomColumnsState();
        rebuildDisplayRows();
        refreshTriggerTableContent();
        createCustomBtn_.setEnabled (! hasCustomGroupsAtLimit());
    }

    if (! savedTableColumns.isEmpty())
        trigger::model::applyTableColumns (triggerTable_.getHeader(), savedTableColumns);

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
    // Min width: left panel (390) + margins (34) + cols min sum (793) + test (56) + scrollbar+pad (10)
    setResizeLimits (1284, 428, 10000, 10000);
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
    const auto requestQuit = []
    {
        juce::MessageManager::callAsync ([]
        {
            if (auto* app = juce::JUCEApplication::getInstance())
                app->systemRequestedQuit();
        });
    };

    if (quittingFromMenu_)
    {
        const bool wasMaximized = isNativeWindowMaximizedForShutdown (*this);
        hideWindowForShutdown (*this);
        if (wasMaximized)
            removeFromDesktop();
        requestQuit();
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

    const bool wasMaximized = isNativeWindowMaximizedForShutdown (*this);
    hideWindowForShutdown (*this);
    if (wasMaximized)
        removeFromDesktop();
    requestQuit();
}

void MainWindow::prepareForShutdown()
{
    juce::ModalComponentManager::getInstance()->cancelAllModalComponents();
    hideWindowForShutdown (*this);
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
