#include "MainWindow.h"
#include "core/BridgeVersion.h"
#include <cmath>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace bridge
{
namespace
{
const auto kBg = juce::Colour::fromRGB (0x17, 0x17, 0x17);      // UI_BG
const auto kRow = juce::Colour::fromRGB (0x3a, 0x3a, 0x3a);     // UI_BG_ROW
const auto kSection = juce::Colour::fromRGB (0x65, 0x65, 0x65); // UI_BG_SEC
const auto kInput = juce::Colour::fromRGB (0x24, 0x24, 0x24);   // UI_BG_INPUT
const auto kHeader = juce::Colour::fromRGB (0x2f, 0x2f, 0x32);
const auto kTeal = juce::Colour::fromRGB (0x3d, 0x80, 0x70);    // UI_TEAL
const auto kTealOff = juce::Colour::fromRGB (0x48, 0x48, 0x48); // UI_TEAL_OFF

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

void setupOffsetSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    s.setRange (-30, 30, 1);
    s.setValue (0);
}

void setupDbSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    s.setRange (-24, 24, 0.1);
    s.setValue (0.0);
}

void fillRateCombo (juce::ComboBox& combo)
{
    combo.clear();
    combo.addItem ("44100", 44100);
    combo.addItem ("48000", 48000);
    combo.addItem ("96000", 96000);
    combo.setSelectedId (44100, juce::dontSendNotification);
}

void fillChannelCombo (juce::ComboBox& combo)
{
    combo.clear();
    for (int i = 1; i <= 8; ++i)
        combo.addItem (juce::String (i), i);
    combo.addItem ("1+2", 100);
    combo.setSelectedId (1, juce::dontSendNotification);
}

juce::String normalizeDriverKey (juce::String s)
{
    s = s.toLowerCase().trim();
    if (s.contains ("asio")) return "asio";
    if (s.contains ("directsound")) return "directsound";
    if (s.contains ("wasapi") || s.contains ("windows audio")) return "windowsaudio";
    return s;
}

bool matchesDriverFilter (const juce::String& driverUi, const juce::String& typeName)
{
    const auto d = normalizeDriverKey (driverUi);
    if (d.startsWith ("default"))
        return true;
    const auto t = normalizeDriverKey (typeName);
    if (d == "asio") return t.contains ("asio");
    if (d == "directsound") return t.contains ("directsound");
    if (d == "windowsaudio") return t.contains ("windowsaudio");
    return true;
}

float dbToLinearGain (double db)
{
    return (float) std::pow (10.0, db / 20.0);
}

juce::File findBridgeBaseDirFromExe()
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
        auto candidate = r.getChildFile ("MTC_Bridge");
        if (candidate.exists())
            return candidate;
    }

    return {};
}

juce::Image loadBridgeAppIcon()
{
    auto base = findBridgeBaseDirFromExe();
    if (! base.exists())
        return {};

    auto iconFile = base.getChildFile ("Icons/App_Icon.png");
    if (! iconFile.existsAsFile())
        return {};

    auto in = std::unique_ptr<juce::FileInputStream> (iconFile.createInputStream());
    if (in == nullptr)
        return {};

    return juce::ImageFileFormat::loadFrom (*in);
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
#endif

class BridgeTrayIcon final : public juce::SystemTrayIconComponent
{
public:
    explicit BridgeTrayIcon (MainWindow& owner) : owner_ (owner) {}

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

}

MainContentComponent::MainContentComponent()
{
    loadFontsAndIcon();
    applyLookAndFeel();

    titleEasyLabel_.setText ("EASY", juce::dontSendNotification);
    titleEasyLabel_.setJustificationType (juce::Justification::centredLeft);
    titleEasyLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    titleEasyLabel_.setFont ((titleEasyFont_.getHeight() > 0.0f ? titleEasyFont_ : juce::Font()).withHeight (34.0f));
    addAndMakeVisible (titleEasyLabel_);

    titleBridgeLabel_.setText ("BRIDGE", juce::dontSendNotification);
    titleBridgeLabel_.setJustificationType (juce::Justification::centredLeft);
    titleBridgeLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xa1, 0xa5, 0xac));
    titleBridgeLabel_.setFont ((titleBridgeFont_.getHeight() > 0.0f ? titleBridgeFont_ : juce::Font()).withHeight (34.0f));
    addAndMakeVisible (titleBridgeLabel_);

    titleVersionLabel_.setText ("v" + juce::String (version::kAppVersion), juce::dontSendNotification);
    titleVersionLabel_.setJustificationType (juce::Justification::centredRight);
    titleVersionLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0x8b, 0x91, 0x9a));
    titleVersionLabel_.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (titleVersionLabel_);

    helpButton_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x50, 0x52, 0x56));
    helpButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x50, 0x52, 0x56));
    helpButton_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xdf, 0xe3, 0xea));
    helpButton_.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (0xdf, 0xe3, 0xea));
    helpButton_.onClick = [this] { openHelpPage(); };
    addAndMakeVisible (helpButton_);

    tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
    tcLabel_.setJustificationType (juce::Justification::centred);
    tcLabel_.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (20, 20, 20));
    tcLabel_.setColour (juce::Label::outlineColourId, juce::Colour::fromRGB (48, 48, 48));
    tcLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (220, 216, 203));
    if (monoFont_.getHeight() > 0.0f)
        tcLabel_.setFont (monoFont_.withHeight (62.0f));
    addAndMakeVisible (tcLabel_);

    tcFpsLabel_.setText ("TC FPS: --", juce::dontSendNotification);
    addAndMakeVisible (tcFpsLabel_);

    statusButton_.setButtonText ("STOPPED");
    statusButton_.setColour (juce::TextButton::buttonColourId, kRow);
    statusButton_.setColour (juce::TextButton::buttonOnColourId, kRow);
    statusButton_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (255, 120, 110));
    statusButton_.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (255, 120, 110));
    statusButton_.onClick = [this] { openStatusMonitorWindow(); };
    addAndMakeVisible (statusButton_);

    settingsButton_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    settingsButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    settingsButton_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
    settingsButton_.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
    settingsButton_.onClick = [this] { openSettingsMenu(); };
    addAndMakeVisible (settingsButton_);

    quitButton_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0xb6, 0x45, 0x40));
    quitButton_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0xb6, 0x45, 0x40));
    quitButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    quitButton_.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    quitButton_.onClick = [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    addAndMakeVisible (quitButton_);

    sourceCombo_.addItem ("LTC", 1);
    sourceCombo_.addItem ("MTC", 2);
    sourceCombo_.addItem ("ArtNet", 3);
    sourceCombo_.addItem ("OSC", 4);
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    styleCombo (sourceCombo_);
    addAndMakeVisible (sourceHeaderLabel_);
    sourceExpandBtn_.setExpanded (true);
    sourceExpandBtn_.onClick = [this]
    {
        sourceExpanded_ = ! sourceExpanded_;
        sourceExpandBtn_.setExpanded (sourceExpanded_);
        updateWindowHeight();
        resized();
    };
    addAndMakeVisible (sourceExpandBtn_);
    addAndMakeVisible (sourceCombo_);
    sourceHeaderLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
    sourceHeaderLabel_.setFont (juce::FontOptions (14.0f));
    sourceHeaderLabel_.setJustificationType (juce::Justification::centredLeft);
    sourceHeaderLabel_.setBorderSize (juce::BorderSize<int> (0, 6, 0, 0));
    juce::Label* rowLabels[] = {
        &inDriverLbl_, &inDeviceLbl_, &inChannelLbl_, &inRateLbl_, &inLevelLbl_, &inGainLbl_,
        &mtcInLbl_, &artInLbl_, &oscAdapterLbl_, &oscIpLbl_, &oscPortLbl_, &oscFpsLbl_, &oscStrLbl_, &oscFloatLbl_,
        &outDriverLbl_, &outDeviceLbl_, &outChannelLbl_, &outRateLbl_, &outOffsetLbl_, &outLevelLbl_,
        &mtcOutLbl_, &mtcOffsetLbl_, &artOutLbl_, &artIpLbl_, &artOffsetLbl_
    };
    for (auto* l : rowLabels)
    {
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xba, 0xc5, 0xd6));
        l->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*l);
    }

    for (auto* c : { &ltcInDriverCombo_, &ltcOutDriverCombo_ })
    {
        c->addItem ("Default (all devices)", 1);
        c->addItem ("ASIO", 2);
        c->addItem ("WASAPI", 3);
        c->addItem ("DirectSound", 4);
        c->setSelectedId (1, juce::dontSendNotification);
        styleCombo (*c);
    }

    styleCombo (ltcInDeviceCombo_);
    styleCombo (ltcInChannelCombo_);
    styleCombo (ltcInSampleRateCombo_);
    styleCombo (mtcInCombo_);
    styleCombo (artnetInCombo_);
    styleCombo (oscAdapterCombo_);
    styleCombo (oscFpsCombo_);
    styleCombo (ltcOutDeviceCombo_);
    styleCombo (ltcOutChannelCombo_);
    styleCombo (ltcOutSampleRateCombo_);
    styleCombo (mtcOutCombo_);
    styleCombo (artnetOutCombo_);
    addAndMakeVisible (ltcInDriverCombo_);
    addAndMakeVisible (ltcOutDriverCombo_);

    fillChannelCombo (ltcInChannelCombo_);
    fillChannelCombo (ltcOutChannelCombo_);
    fillRateCombo (ltcInSampleRateCombo_);
    fillRateCombo (ltcOutSampleRateCombo_);

    oscPortEditor_.setText ("9000");
    oscIpEditor_.setText ("0.0.0.0");
    oscPortEditor_.setInputRestrictions (5, "0123456789");
    oscAddrStrEditor_.setText ("/frames/str");
    oscAddrFloatEditor_.setText ("/time");
    styleEditor (oscIpEditor_);
    styleEditor (oscPortEditor_);
    styleEditor (oscAddrStrEditor_);
    styleEditor (oscAddrFloatEditor_);
    styleEditor (artnetDestIpEditor_);
    artnetDestIpEditor_.setText ("255.255.255.255");

    oscFpsCombo_.addItem ("24", 1);
    oscFpsCombo_.addItem ("25", 2);
    oscFpsCombo_.addItem ("29.97", 3);
    oscFpsCombo_.addItem ("30", 4);
    oscFpsCombo_.setSelectedId (2, juce::dontSendNotification);

    setupDbSlider (ltcInGainSlider_);
    ltcInLevelBar_.setMeterColour (juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    setupDbSlider (ltcOutLevelSlider_);
    ltcOffsetEditor_.setInputRestrictions (4, "-0123456789");
    mtcOffsetEditor_.setInputRestrictions (4, "-0123456789");
    artnetOffsetEditor_.setInputRestrictions (4, "-0123456789");
    ltcOffsetEditor_.setText ("0");
    mtcOffsetEditor_.setText ("0");
    artnetOffsetEditor_.setText ("0");
    styleEditor (ltcOffsetEditor_);
    styleEditor (mtcOffsetEditor_);
    styleEditor (artnetOffsetEditor_);
    styleSlider (ltcInGainSlider_, true);
    styleSlider (ltcOutLevelSlider_, true);

    for (auto* h : { &outLtcHeaderLabel_, &outMtcHeaderLabel_, &outArtHeaderLabel_ })
    {
        h->setColour (juce::Label::backgroundColourId, kSection);
        h->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
        h->setFont (juce::FontOptions (14.0f));
        h->setJustificationType (juce::Justification::centredLeft);
        h->setBorderSize (juce::BorderSize<int> (0, 42, 0, 0));
        addAndMakeVisible (*h);
    }

    for (auto* b : { &outLtcExpandBtn_, &outMtcExpandBtn_, &outArtExpandBtn_ })
        addAndMakeVisible (*b);
    outLtcExpandBtn_.setExpanded (false);
    outMtcExpandBtn_.setExpanded (false);
    outArtExpandBtn_.setExpanded (false);
    outLtcExpandBtn_.onClick = [this] { outLtcExpanded_ = ! outLtcExpanded_; outLtcExpandBtn_.setExpanded (outLtcExpanded_); updateWindowHeight(); resized(); };
    outMtcExpandBtn_.onClick = [this] { outMtcExpanded_ = ! outMtcExpanded_; outMtcExpandBtn_.setExpanded (outMtcExpanded_); updateWindowHeight(); resized(); };
    outArtExpandBtn_.onClick = [this] { outArtExpanded_ = ! outArtExpanded_; outArtExpandBtn_.setExpanded (outArtExpanded_); updateWindowHeight(); resized(); };

    for (auto* c : { &ltcOutSwitch_, &mtcOutSwitch_, &artnetOutSwitch_ })
        addAndMakeVisible (*c);
    for (auto* c : { &ltcThruDot_, &mtcThruDot_ })
        addAndMakeVisible (*c);
    for (auto* l : { &ltcThruLbl_, &mtcThruLbl_ })
    {
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xd0, 0xd0, 0xd0));
        l->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*l);
    }

    for (auto* c : {
            &ltcInDeviceCombo_, &ltcInChannelCombo_, &ltcInSampleRateCombo_,
            &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_,
            &oscAdapterCombo_,
            &mtcInCombo_, &artnetInCombo_, &mtcOutCombo_, &artnetOutCombo_, &oscFpsCombo_ })
    {
        addAndMakeVisible (*c);
        c->onChange = [this] { onInputSettingsChanged(); };
    }
    ltcInDriverCombo_.onChange = [this]
    {
        refreshLtcDeviceListsByDriver();
        onInputSettingsChanged();
    };
    ltcOutDriverCombo_.onChange = [this]
    {
        refreshLtcDeviceListsByDriver();
        onOutputSettingsChanged();
    };
    oscAdapterCombo_.onChange = [this]
    {
        syncOscIpWithAdapter();
        onInputSettingsChanged();
    };

    for (auto* c : { &sourceCombo_ })
        c->onChange = [this]
        {
            restartSelectedSource();
            updateWindowHeight();
            resized();
        };

    ltcOutSwitch_.onToggle = [this] (bool) { onOutputToggleChanged(); };
    mtcOutSwitch_.onToggle = [this] (bool) { onOutputToggleChanged(); };
    artnetOutSwitch_.onToggle = [this] (bool) { onOutputToggleChanged(); };

    for (auto* c : { &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutSampleRateCombo_, &mtcOutCombo_, &artnetOutCombo_ })
        c->onChange = [this] { onOutputSettingsChanged(); };

    addAndMakeVisible (ltcInGainSlider_);
    addAndMakeVisible (ltcInLevelBar_);
    addAndMakeVisible (ltcOutLevelSlider_);
    addAndMakeVisible (ltcOffsetEditor_);
    addAndMakeVisible (mtcOffsetEditor_);
    addAndMakeVisible (artnetOffsetEditor_);
    addAndMakeVisible (oscIpEditor_);
    addAndMakeVisible (oscPortEditor_);
    addAndMakeVisible (oscAddrStrEditor_);
    addAndMakeVisible (oscAddrFloatEditor_);
    addAndMakeVisible (artnetDestIpEditor_);

    refreshDeviceLists();
    loadRuntimePrefs();
    maybeAutoLoadConfig();
    if (! (autoLoadOnStartup_ && lastConfigFile_.existsAsFile()))
        restartSelectedSource();
    // Parent window may not be attached yet inside the component ctor.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainContentComponent> (this)]
    {
        if (safe != nullptr)
            safe->updateWindowHeight();
    });
    startTimerHz (60);
}

int MainContentComponent::calcPreferredHeight() const
{
    return calcHeightForState (
        sourceExpanded_,
        sourceCombo_.getSelectedId(),
        outLtcExpanded_,
        outMtcExpanded_,
        outArtExpanded_);
}

int MainContentComponent::calcHeightForState (bool sourceExpanded, int sourceId, bool outLtcExpanded, bool outMtcExpanded, bool outArtExpanded) const
{
    int h = 16; // outer margins
    h += 40 + 4;  // title
    h += 108;     // tc
    h += 24 + 4;  // tc fps

    auto addRows = [&h] (int count)
    {
        h += count * (40 + 4);
    };

    addRows (1); // source header
    if (sourceExpanded)
    {
        if (sourceId == 1) addRows (6);
        else if (sourceId == 2) addRows (1);
        else if (sourceId == 3) addRows (1);
        else if (sourceId == 4) addRows (6);
    }

    addRows (1); // out LTC header
    if (outLtcExpanded) addRows (6);
    addRows (1); // out MTC header
    if (outMtcExpanded) addRows (2);
    addRows (1); // out ArtNet header
    if (outArtExpanded) addRows (3);

    h += 24 + 4; // status
    h += 36 + 4; // settings/quit
    h += 8;  // bottom pad
    return juce::jlimit (420, 1600, h);
}

void MainContentComponent::updateWindowHeight()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        auto* content = window->getContentComponent();
        const int chrome = content != nullptr ? (window->getHeight() - content->getHeight()) : 0;

        const int collapsedContent = calcHeightForState (false, sourceCombo_.getSelectedId(), false, false, false);
        const int expandedContent = calcHeightForState (true, sourceCombo_.getSelectedId(), true, true, true);
        const int currentContent = calcPreferredHeight();

        const int minTotal = collapsedContent + chrome;
        const int maxTotal = expandedContent + chrome;
        const int targetTotal = juce::jlimit (minTotal, maxTotal, currentContent + chrome);

        window->setResizeLimits (430, minTotal, 430, maxTotal);
        if (window->getHeight() != targetTotal)
            window->setSize (window->getWidth(), targetTotal);
    }
}

void MainContentComponent::loadFontsAndIcon()
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

    juce::File base;
    for (auto r : roots)
    {
        auto candidate = r.getChildFile ("MTC_Bridge");
        if (candidate.exists())
        {
            base = candidate;
            break;
        }
    }
    if (! base.exists())
        return;

    auto loadFont = [] (const juce::File& f) -> juce::Font
    {
        if (! f.existsAsFile())
            return {};
        juce::MemoryBlock data;
        f.loadFileAsData (data);
        auto tf = juce::Typeface::createSystemTypefaceFor (data.getData(), data.getSize());
        if (tf == nullptr)
            return {};
        return juce::Font (juce::FontOptions (tf));
    };

    titleEasyFont_ = loadFont (base.getChildFile ("Fonts/Thunder-SemiBoldLC.ttf"));
    titleBridgeFont_ = loadFont (base.getChildFile ("Fonts/Thunder-LightLC.ttf"));
    monoFont_ = loadFont (base.getChildFile ("Fonts/JetBrainsMonoNL-Bold.ttf"));

    auto iconFile = base.getChildFile ("Icons/App_Icon.png");
    if (iconFile.existsAsFile())
    {
        auto in = std::unique_ptr<juce::FileInputStream> (iconFile.createInputStream());
        if (in != nullptr)
            appIcon_ = juce::ImageFileFormat::loadFrom (*in);
    }
}

void MainContentComponent::applyLookAndFeel()
{
    lookAndFeel_ = std::make_unique<juce::LookAndFeel_V4>();
    lookAndFeel_->setColour (juce::ComboBox::backgroundColourId, kInput);
    lookAndFeel_->setColour (juce::ComboBox::textColourId, juce::Colour::fromRGB (210, 220, 230));
    lookAndFeel_->setColour (juce::ComboBox::outlineColourId, kRow);
    lookAndFeel_->setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (0x9a, 0xa1, 0xac));

    // Dropdown list style (close to PySide theme).
    lookAndFeel_->setColour (juce::PopupMenu::backgroundColourId, kInput);
    lookAndFeel_->setColour (juce::PopupMenu::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
    lookAndFeel_->setColour (juce::PopupMenu::highlightedBackgroundColourId, kTeal);
    lookAndFeel_->setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    lookAndFeel_->setColour (juce::PopupMenu::headerTextColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));

    // Scrollbar inside popup menu.
    lookAndFeel_->setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));
    lookAndFeel_->setColour (juce::ScrollBar::thumbColourId, kTeal);
    lookAndFeel_->setColour (juce::ScrollBar::trackColourId, juce::Colour::fromRGB (0x1a, 0x1a, 0x1a));

    lookAndFeel_->setColour (juce::TextEditor::backgroundColourId, kInput);
    lookAndFeel_->setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (210, 220, 230));
    lookAndFeel_->setColour (juce::TextEditor::outlineColourId, kRow);
    setLookAndFeel (lookAndFeel_.get());
}

void MainContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    if (! headerRect_.isEmpty())
    {
        g.setColour (kHeader);
        g.fillRoundedRectangle (headerRect_.toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x3c, 0x3e, 0x42));
        g.drawRoundedRectangle (headerRect_.toFloat(), 5.0f, 1.0f);
    }

    g.setColour (kSection);
    for (auto r : sectionRowRects_)
        g.fillRoundedRectangle (r.toFloat(), 5.0f);

    g.setColour (kRow);
    for (auto r : paramRowRects_)
        g.fillRoundedRectangle (r.toFloat(), 5.0f);

    g.setColour (juce::Colour::fromRGB (0x2f, 0x2f, 0x2f));
    for (auto r : sectionRowRects_)
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
    for (auto r : paramRowRects_)
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);

    if (! statusRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x3f, 0x3f, 0x3f));
        g.drawRoundedRectangle (statusRect_.toFloat(), 4.0f, 1.0f);
    }
    if (! buttonRowRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x2f, 0x2f, 0x2f));
        g.drawRoundedRectangle (buttonRowRect_.toFloat(), 4.0f, 1.0f);
    }
}

void MainContentComponent::resized()
{
    paramRowRects_.clear();
    sectionRowRects_.clear();

    auto a = getLocalBounds().reduced (8);
    headerRect_ = a.removeFromTop (40);
    auto header = headerRect_.reduced (8, 0);
    auto help = header.removeFromRight (28);
    helpButton_.setBounds (help.withSizeKeepingCentre (24, 24));
    auto versionZone = header.removeFromRight (64);
    titleVersionLabel_.setBounds (versionZone);

    auto titleArea = header;
    const int easyW = juce::jmax (46, titleEasyLabel_.getFont().getStringWidth ("EASY") + 6);
    const int bridgeW = juce::jmax (74, titleBridgeLabel_.getFont().getStringWidth ("BRIDGE") + 6);
    const int startX = titleArea.getX() + 2;
    const int titleYOffset = 3;
    titleEasyLabel_.setBounds (startX, titleArea.getY() + titleYOffset, easyW, titleArea.getHeight() - titleYOffset);
    titleBridgeLabel_.setBounds (startX + easyW, titleArea.getY() + titleYOffset, bridgeW, titleArea.getHeight() - titleYOffset);
    a.removeFromTop (4);
    tcLabel_.setBounds (a.removeFromTop (108));
    tcFpsLabel_.setBounds (a.removeFromTop (24));
    a.removeFromTop (4);

    auto row = [&a] (int h = 40)
    {
        auto r = a.removeFromTop (h);
        a.removeFromTop (4);
        return r;
    };
    auto fieldRow = [&] (juce::Label& lbl, juce::Component& editor)
    {
        auto r = row();
        paramRowRects_.add (r);
        lbl.setVisible (true);
        editor.setVisible (true);
        auto labelArea = r.removeFromLeft (112);
        auto controlArea = r.reduced (0, 3);
        lbl.setBounds (labelArea.reduced (10, 0));
        if (&editor == &ltcInLevelBar_)
        {
            auto bar = controlArea.reduced (6, 0);
            const int h = 8; // thinner meter like the original UI
            bar = juce::Rectangle<int> (bar.getX(), bar.getCentreY() - h / 2, bar.getWidth(), h);
            editor.setBounds (bar);
        }
        else
        {
            editor.setBounds (controlArea.reduced (2, 0));
        }
    };
    auto hideRowLabels = [this]
    {
        juce::Label* labels[] = {
            &inDriverLbl_, &inDeviceLbl_, &inChannelLbl_, &inRateLbl_, &inLevelLbl_, &inGainLbl_,
            &mtcInLbl_, &artInLbl_, &oscAdapterLbl_, &oscIpLbl_, &oscPortLbl_, &oscFpsLbl_, &oscStrLbl_, &oscFloatLbl_,
            &outDriverLbl_, &outDeviceLbl_, &outChannelLbl_, &outRateLbl_, &outOffsetLbl_, &outLevelLbl_,
            &mtcOutLbl_, &mtcOffsetLbl_, &artOutLbl_, &artIpLbl_, &artOffsetLbl_
        };
        for (auto* l : labels)
            l->setVisible (false);
    };
    hideRowLabels();

    auto sourceRow = row();
    sectionRowRects_.add (sourceRow);
    auto sourceLabelZone = sourceRow.removeFromLeft (112);
    {
        auto btnHost = sourceLabelZone.removeFromLeft (36);
        const int d = 28;
        sourceExpandBtn_.setBounds (
            btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
            btnHost.getY() + (btnHost.getHeight() - d) / 2,
            d, d);
    }
    sourceHeaderLabel_.setBounds (sourceLabelZone);
    sourceCombo_.setBounds (sourceRow.reduced (2, 3));
    sourceHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    sourceExpandBtn_.setExpanded (sourceExpanded_);

    const auto src = sourceCombo_.getSelectedId();
    if (sourceExpanded_ && src == 1)
    {
        fieldRow (inDriverLbl_, ltcInDriverCombo_);
        fieldRow (inDeviceLbl_, ltcInDeviceCombo_);
        fieldRow (inChannelLbl_, ltcInChannelCombo_);
        fieldRow (inRateLbl_, ltcInSampleRateCombo_);
        fieldRow (inLevelLbl_, ltcInLevelBar_);
        fieldRow (inGainLbl_, ltcInGainSlider_);
    }
    else if (sourceExpanded_ && src == 2)
    {
        fieldRow (mtcInLbl_, mtcInCombo_);
    }
    else if (sourceExpanded_ && src == 3)
    {
        fieldRow (artInLbl_, artnetInCombo_);
    }
    else if (sourceExpanded_)
    {
        fieldRow (oscAdapterLbl_, oscAdapterCombo_);
        fieldRow (oscIpLbl_, oscIpEditor_);
        fieldRow (oscPortLbl_, oscPortEditor_);
        fieldRow (oscFpsLbl_, oscFpsCombo_);
        fieldRow (oscStrLbl_, oscAddrStrEditor_);
        fieldRow (oscFloatLbl_, oscAddrFloatEditor_);
    }

    auto ltcHeader = row();
    sectionRowRects_.add (ltcHeader);
    auto ltcHeaderCopy = ltcHeader;
    outLtcHeaderLabel_.setBounds (ltcHeaderCopy);
    {
        auto btnHost = ltcHeader.removeFromLeft (36);
        const int d = 28;
        outLtcExpandBtn_.setBounds (
            btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
            btnHost.getY() + (btnHost.getHeight() - d) / 2,
            d, d);
    }
    ltcHeader.removeFromLeft (110);
    ltcOutSwitch_.setBounds (ltcHeader.removeFromRight (54).reduced (0, 6));
    {
        auto dotHost = ltcHeader.removeFromRight (22);
        const int d = 18;
        ltcThruDot_.setBounds (dotHost.getCentreX() - d / 2, dotHost.getCentreY() - d / 2, d, d);
    }
    ltcThruLbl_.setBounds (ltcHeader.removeFromRight (40));
    outLtcHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    outLtcExpandBtn_.setExpanded (outLtcExpanded_);

    if (outLtcExpanded_)
    {
        fieldRow (outDriverLbl_, ltcOutDriverCombo_);
        fieldRow (outDeviceLbl_, ltcOutDeviceCombo_);
        fieldRow (outChannelLbl_, ltcOutChannelCombo_);
        fieldRow (outRateLbl_, ltcOutSampleRateCombo_);
        fieldRow (outOffsetLbl_, ltcOffsetEditor_);
        fieldRow (outLevelLbl_, ltcOutLevelSlider_);
    }

    auto mtcHeader = row();
    sectionRowRects_.add (mtcHeader);
    auto mtcHeaderCopy = mtcHeader;
    outMtcHeaderLabel_.setBounds (mtcHeaderCopy);
    {
        auto btnHost = mtcHeader.removeFromLeft (36);
        const int d = 28;
        outMtcExpandBtn_.setBounds (
            btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
            btnHost.getY() + (btnHost.getHeight() - d) / 2,
            d, d);
    }
    mtcHeader.removeFromLeft (110);
    mtcOutSwitch_.setBounds (mtcHeader.removeFromRight (54).reduced (0, 6));
    {
        auto dotHost = mtcHeader.removeFromRight (22);
        const int d = 18;
        mtcThruDot_.setBounds (dotHost.getCentreX() - d / 2, dotHost.getCentreY() - d / 2, d, d);
    }
    mtcThruLbl_.setBounds (mtcHeader.removeFromRight (40));
    outMtcHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    outMtcExpandBtn_.setExpanded (outMtcExpanded_);

    if (outMtcExpanded_)
    {
        fieldRow (mtcOutLbl_, mtcOutCombo_);
        fieldRow (mtcOffsetLbl_, mtcOffsetEditor_);
    }

    auto artHeader = row();
    sectionRowRects_.add (artHeader);
    auto artHeaderCopy = artHeader;
    outArtHeaderLabel_.setBounds (artHeaderCopy);
    {
        auto btnHost = artHeader.removeFromLeft (36);
        const int d = 28;
        outArtExpandBtn_.setBounds (
            btnHost.getX() + 3 + (btnHost.getWidth() - d) / 2,
            btnHost.getY() + (btnHost.getHeight() - d) / 2,
            d, d);
    }
    artHeader.removeFromLeft (110);
    artnetOutSwitch_.setBounds (artHeader.removeFromRight (54).reduced (0, 6));
    outArtHeaderLabel_.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    outArtExpandBtn_.setExpanded (outArtExpanded_);
    if (outArtExpanded_)
    {
        fieldRow (artOutLbl_, artnetOutCombo_);
        fieldRow (artIpLbl_, artnetDestIpEditor_);
        fieldRow (artOffsetLbl_, artnetOffsetEditor_);
    }

    buttonRowRect_ = a.removeFromBottom (36);
    auto buttons = buttonRowRect_.reduced (0, 0);
    settingsButton_.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
    quitButton_.setBounds (buttons.reduced (1, 0));
    a.removeFromBottom (4);
    statusRect_ = a.removeFromBottom (24);
    statusButton_.setBounds (statusRect_.reduced (0, 0));

    auto hideAll = [this]
    {
        juce::Component* comps[] = {
            &ltcInDriverCombo_, &ltcInDeviceCombo_, &ltcInChannelCombo_, &ltcInSampleRateCombo_, &ltcInLevelBar_, &ltcInGainSlider_,
            &mtcInCombo_, &artnetInCombo_, &oscAdapterCombo_, &oscIpEditor_, &oscPortEditor_, &oscFpsCombo_, &oscAddrStrEditor_, &oscAddrFloatEditor_
        };
        for (auto* c : comps)
            if (c != nullptr)
                c->setVisible (false);
    };
    hideAll();
    if (src == 1) { ltcInDriverCombo_.setVisible (true); ltcInDeviceCombo_.setVisible (true); ltcInChannelCombo_.setVisible (true); ltcInSampleRateCombo_.setVisible (true); ltcInLevelBar_.setVisible (true); ltcInGainSlider_.setVisible (true); }
    if (src == 2) mtcInCombo_.setVisible (true);
    if (src == 3) artnetInCombo_.setVisible (true);
    if (src == 4) { oscAdapterCombo_.setVisible (true); oscIpEditor_.setVisible (true); oscPortEditor_.setVisible (true); oscFpsCombo_.setVisible (true); oscAddrStrEditor_.setVisible (true); oscAddrFloatEditor_.setVisible (true); }
    ltcInDriverCombo_.setVisible (sourceExpanded_ && src == 1);
    ltcInDeviceCombo_.setVisible (sourceExpanded_ && src == 1);
    ltcInChannelCombo_.setVisible (sourceExpanded_ && src == 1);
    ltcInSampleRateCombo_.setVisible (sourceExpanded_ && src == 1);
    ltcInLevelBar_.setVisible (sourceExpanded_ && src == 1);
    ltcInGainSlider_.setVisible (sourceExpanded_ && src == 1);
    mtcInCombo_.setVisible (sourceExpanded_ && src == 2);
    artnetInCombo_.setVisible (sourceExpanded_ && src == 3);
    oscAdapterCombo_.setVisible (sourceExpanded_ && src == 4);
    oscIpEditor_.setVisible (sourceExpanded_ && src == 4);
    oscPortEditor_.setVisible (sourceExpanded_ && src == 4);
    oscFpsCombo_.setVisible (sourceExpanded_ && src == 4);
    oscAddrStrEditor_.setVisible (sourceExpanded_ && src == 4);
    oscAddrFloatEditor_.setVisible (sourceExpanded_ && src == 4);

    ltcOutDriverCombo_.setVisible (outLtcExpanded_);
    ltcOutDeviceCombo_.setVisible (outLtcExpanded_);
    ltcOutChannelCombo_.setVisible (outLtcExpanded_);
    ltcOutSampleRateCombo_.setVisible (outLtcExpanded_);
    ltcOffsetEditor_.setVisible (outLtcExpanded_);
    ltcOutLevelSlider_.setVisible (outLtcExpanded_);

    mtcOutCombo_.setVisible (outMtcExpanded_);
    mtcOffsetEditor_.setVisible (outMtcExpanded_);

    artnetOutCombo_.setVisible (outArtExpanded_);
    artnetDestIpEditor_.setVisible (outArtExpanded_);
    artnetOffsetEditor_.setVisible (outArtExpanded_);
}

void MainContentComponent::restartSelectedSource()
{
    bridgeEngine_.stopLtcInput();
    bridgeEngine_.stopMtcInput();
    bridgeEngine_.stopArtnetInput();
    bridgeEngine_.stopOscInput();

    juce::String err;
    const int src = sourceCombo_.getSelectedId();
    if (src == 1)
    {
        bridgeEngine_.setInputSource (engine::InputSource::LTC);
        const int idx = ltcInDeviceCombo_.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, filteredInputChoices_.size()))
            bridgeEngine_.startLtcInput (filteredInputChoices_[idx], comboChannelIndex (ltcInChannelCombo_), comboSampleRate (ltcInSampleRateCombo_), 512, err);
    }
    else if (src == 2)
    {
        bridgeEngine_.setInputSource (engine::InputSource::MTC);
        bridgeEngine_.startMtcInput (mtcInCombo_.getSelectedItemIndex(), err);
    }
    else if (src == 3)
    {
        bridgeEngine_.setInputSource (engine::InputSource::ArtNet);
        bridgeEngine_.startArtnetInput (artnetInCombo_.getSelectedItemIndex(), err);
    }
    else
    {
        bridgeEngine_.setInputSource (engine::InputSource::OSC);
        FrameRate fps = FrameRate::FPS_25;
        if (oscFpsCombo_.getSelectedId() == 1) fps = FrameRate::FPS_24;
        if (oscFpsCombo_.getSelectedId() == 3) fps = FrameRate::FPS_2997;
        if (oscFpsCombo_.getSelectedId() == 4) fps = FrameRate::FPS_30;
        const auto bindIp = (oscIpEditor_.getText().trim().isNotEmpty() ? oscIpEditor_.getText().trim() : parseBindIpFromAdapterLabel (oscAdapterCombo_.getText()));
        bridgeEngine_.startOscInput (juce::jlimit (1, 65535, oscPortEditor_.getText().getIntValue()), bindIp, fps, oscAddrStrEditor_.getText(), oscAddrFloatEditor_.getText(), err);
    }

    if (err.isNotEmpty())
        setStatusText (err, juce::Colour::fromRGB (0xff, 0x9f, 0x43));
}

void MainContentComponent::onOutputToggleChanged()
{
    onOutputSettingsChanged();
}

void MainContentComponent::onOutputSettingsChanged()
{
    juce::String err;
    if (ltcOutSwitch_.getState())
    {
        const int idx = ltcOutDeviceCombo_.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, filteredOutputChoices_.size()))
            bridgeEngine_.startLtcOutput (filteredOutputChoices_[idx], comboChannelIndex (ltcOutChannelCombo_), comboSampleRate (ltcOutSampleRateCombo_), 512, err);
        bridgeEngine_.setLtcOutputEnabled (true);
    }
    else
    {
        bridgeEngine_.setLtcOutputEnabled (false);
    }

    if (mtcOutSwitch_.getState())
    {
        bridgeEngine_.startMtcOutput (mtcOutCombo_.getSelectedItemIndex(), err);
        bridgeEngine_.setMtcOutputEnabled (true);
    }
    else
        bridgeEngine_.setMtcOutputEnabled (false);

    if (artnetOutSwitch_.getState())
    {
        bridgeEngine_.startArtnetOutput (artnetOutCombo_.getSelectedItemIndex(), err);
        bridgeEngine_.setArtnetOutputEnabled (true);
    }
    else
        bridgeEngine_.setArtnetOutputEnabled (false);

    if (err.isNotEmpty())
        setStatusText (err, juce::Colour::fromRGB (0xff, 0x9f, 0x43));
}

void MainContentComponent::onInputSettingsChanged()
{
    restartSelectedSource();
}

void MainContentComponent::timerCallback()
{
    bridgeEngine_.setLtcInputGain (dbToLinearGain (ltcInGainSlider_.getValue()));
    bridgeEngine_.setLtcOutputGain (dbToLinearGain (ltcOutLevelSlider_.getValue()));
    bridgeEngine_.setOffsets (
        offsetFromEditor (ltcOffsetEditor_),
        offsetFromEditor (mtcOffsetEditor_),
        offsetFromEditor (artnetOffsetEditor_));
    auto st = bridgeEngine_.tick();

    const float peak = bridgeEngine_.getLtcInputPeakLevel();
    // Instant attack with exponential decay for responsive level metering.
    ltcInLevelSmoothed_ = (peak > ltcInLevelSmoothed_) ? peak : (ltcInLevelSmoothed_ * 0.85f);
    ltcInLevelBar_.setLevel (ltcInLevelSmoothed_);

    if (st.hasInputTc)
    {
        hasLatchedTc_ = true;
        latchedTc_ = st.inputTc;
        latchedFps_ = st.inputFps;
        tcLabel_.setText (st.inputTc.toDisplayString (st.inputFps).replaceCharacter ('.', ':'), juce::dontSendNotification);
        tcFpsLabel_.setText ("TC FPS: " + frameRateToString (st.inputFps), juce::dontSendNotification);
        setStatusText ("RUNNING | LTC " + st.ltcOutStatus + " | MTC " + st.mtcOutStatus + " | ArtNet " + st.artnetOutStatus,
                       juce::Colour::fromRGB (0x71, 0xd1, 0x7a));
    }
    else
    {
        if (hasLatchedTc_)
        {
            tcLabel_.setText (latchedTc_.toDisplayString (latchedFps_).replaceCharacter ('.', ':'), juce::dontSendNotification);
            tcFpsLabel_.setText ("TC FPS: " + frameRateToString (latchedFps_), juce::dontSendNotification);
        }
        else
        {
            tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
            tcFpsLabel_.setText ("TC FPS: --", juce::dontSendNotification);
        }

        if (statusButton_.getButtonText().startsWithIgnoreCase ("RUNNING"))
            setStatusText ("STOPPED - no timecode", juce::Colour::fromRGB (255, 120, 110));
    }
}

void MainContentComponent::refreshDeviceLists()
{
    inputChoices_ = bridgeEngine_.scanAudioInputs();
    outputChoices_ = bridgeEngine_.scanAudioOutputs();
    refreshLtcDeviceListsByDriver();
    refreshNetworkMidiLists();
}

void MainContentComponent::refreshLtcDeviceListsByDriver()
{
    const auto prevIn = ltcInDeviceCombo_.getText();
    const auto prevOut = ltcOutDeviceCombo_.getText();

    filteredInputChoices_.clear();
    filteredOutputChoices_.clear();

    for (const auto& c : inputChoices_)
        if (matchesDriverFilter (ltcInDriverCombo_.getText(), c.typeName))
            filteredInputChoices_.add (c);
    for (const auto& c : outputChoices_)
        if (matchesDriverFilter (ltcOutDriverCombo_.getText(), c.typeName))
            filteredOutputChoices_.add (c);

    fillAudioCombo (ltcInDeviceCombo_, filteredInputChoices_);
    fillAudioCombo (ltcOutDeviceCombo_, filteredOutputChoices_);

    auto restoreByText = [] (juce::ComboBox& combo, const juce::String& text)
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
    if (prevIn.isNotEmpty()) restoreByText (ltcInDeviceCombo_, prevIn);
    if (prevOut.isNotEmpty()) restoreByText (ltcOutDeviceCombo_, prevOut);
}

void MainContentComponent::refreshNetworkMidiLists()
{
    mtcInCombo_.clear();
    auto ins = bridgeEngine_.midiInputs();
    for (int i = 0; i < ins.size(); ++i)
        mtcInCombo_.addItem (ins[i], i + 1);
    if (mtcInCombo_.getNumItems() > 0)
        mtcInCombo_.setSelectedItemIndex (0, juce::dontSendNotification);

    mtcOutCombo_.clear();
    auto outs = bridgeEngine_.midiOutputs();
    for (int i = 0; i < outs.size(); ++i)
        mtcOutCombo_.addItem (outs[i], i + 1);
    if (mtcOutCombo_.getNumItems() > 0)
        mtcOutCombo_.setSelectedItemIndex (0, juce::dontSendNotification);

    auto ifaces = bridgeEngine_.artnetInterfaces();
    artnetInCombo_.clear();
    artnetOutCombo_.clear();
    oscAdapterCombo_.clear();
    oscAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    oscAdapterCombo_.addItem ("Loopback (127.0.0.1)", 2);
    for (int i = 0; i < ifaces.size(); ++i)
    {
        artnetInCombo_.addItem (ifaces[i], i + 1);
        artnetOutCombo_.addItem (ifaces[i], i + 1);
        if (! ifaces[i].startsWithIgnoreCase ("ALL INTERFACES"))
            oscAdapterCombo_.addItem (ifaces[i], i + 3);
    }
    if (artnetInCombo_.getNumItems() > 0)
        artnetInCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (artnetOutCombo_.getNumItems() > 0)
        artnetOutCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    if (oscAdapterCombo_.getNumItems() > 0)
        oscAdapterCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    syncOscIpWithAdapter();
}

void MainContentComponent::fillAudioCombo (juce::ComboBox& combo, const juce::Array<engine::AudioChoice>& choices)
{
    combo.clear();
    for (int i = 0; i < choices.size(); ++i)
        combo.addItem (choices[i].displayName, i + 1);
    if (combo.getNumItems() > 0)
        combo.setSelectedItemIndex (0, juce::dontSendNotification);
}

double MainContentComponent::comboSampleRate (const juce::ComboBox& combo)
{
    return juce::jmax (1.0, combo.getText().getDoubleValue());
}

int MainContentComponent::comboBufferSize (const juce::ComboBox& combo)
{
    auto v = combo.getText().getIntValue();
    return v <= 0 ? 512 : v;
}

int MainContentComponent::comboChannelIndex (const juce::ComboBox& combo)
{
    if (combo.getSelectedId() == 100)
        return -1;
    return juce::jmax (0, combo.getSelectedItemIndex());
}

int MainContentComponent::offsetFromEditor (const juce::TextEditor& editor)
{
    return juce::jlimit (-30, 30, editor.getText().getIntValue());
}

void MainContentComponent::styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, kInput);
    c.setColour (juce::ComboBox::outlineColourId, kRow);
    c.setColour (juce::ComboBox::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
}

void MainContentComponent::styleEditor (juce::TextEditor& e)
{
    e.setColour (juce::TextEditor::backgroundColourId, kInput);
    e.setColour (juce::TextEditor::outlineColourId, kRow);
    e.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
}

void MainContentComponent::styleSlider (juce::Slider& s, bool dbStyle)
{
    s.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (0x20, 0x20, 0x20));
    s.setColour (juce::Slider::trackColourId, dbStyle ? kTeal : juce::Colour::fromRGB (0x1f, 0x3b, 0x45));
    s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
    s.setColour (juce::Slider::textBoxOutlineColourId, kRow);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kInput);
}

void MainContentComponent::syncOscIpWithAdapter()
{
    const auto ip = parseBindIpFromAdapterLabel (oscAdapterCombo_.getText());
    const auto lockIp = (ip != "0.0.0.0");
    if (lockIp)
        oscIpEditor_.setText (ip, juce::dontSendNotification);
    else if (oscIpEditor_.getText().trim().isEmpty() || oscIpEditor_.getText().trim() == "127.0.0.1")
        oscIpEditor_.setText ("0.0.0.0", juce::dontSendNotification);
    oscIpEditor_.setReadOnly (lockIp);
}

juce::File MainContentComponent::findBridgeBaseDir() const
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
        auto candidate = r.getChildFile ("MTC_Bridge");
        if (candidate.exists())
            return candidate;
    }

    return {};
}

void MainContentComponent::setStatusText (const juce::String& text, juce::Colour colour)
{
    statusButton_.setButtonText (text);
    statusButton_.setColour (juce::TextButton::textColourOffId, colour);
    statusButton_.setColour (juce::TextButton::textColourOnId, colour);
}

void MainContentComponent::openStatusMonitorWindow()
{
    juce::String details;
    details << "Source: " << sourceCombo_.getText() << "\n";
    details << "Input TC: " << tcLabel_.getText() << " (" << tcFpsLabel_.getText().fromFirstOccurrenceOf (": ", false, false) << ")\n";
    details << "LTC Out: " << (ltcOutSwitch_.getState() ? "ON" : "OFF") << " | device: " << ltcOutDeviceCombo_.getText() << "\n";
    details << "MTC Out: " << (mtcOutSwitch_.getState() ? "ON" : "OFF") << " | port: " << mtcOutCombo_.getText() << "\n";
    details << "ArtNet Out: " << (artnetOutSwitch_.getState() ? "ON" : "OFF") << " | iface: " << artnetOutCombo_.getText() << "\n";
    details << "OSC Listen: " << oscIpEditor_.getText() << ":" << oscPortEditor_.getText() << "\n";
    details << "Status: " << statusButton_.getButtonText();

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Bridge Status Monitor",
                                            details);
}

void MainContentComponent::saveConfigAs()
{
    saveChooser_ = std::make_unique<juce::FileChooser> (
        "Save Easy Bridge config",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("easy_bridge.ebrp"),
        "*.ebrp");

    saveChooser_->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [safe = juce::Component::SafePointer<MainContentComponent> (this)] (const juce::FileChooser& chooser)
                               {
                                   if (safe == nullptr)
                                       return;
                                   auto file = chooser.getResult();
                                   if (file == juce::File{})
                                       return;
                                   if (! file.hasFileExtension (".ebrp"))
                                       file = file.withFileExtension (".ebrp");
                                   safe->saveConfigToFile (file);
                                   safe->saveChooser_.reset();
                               });
}

void MainContentComponent::loadConfigFrom()
{
    loadChooser_ = std::make_unique<juce::FileChooser> (
        "Load Easy Bridge config",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.ebrp");

    loadChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safe = juce::Component::SafePointer<MainContentComponent> (this)] (const juce::FileChooser& chooser)
                               {
                                   if (safe == nullptr)
                                       return;
                                   auto file = chooser.getResult();
                                   if (file != juce::File{})
                                       safe->loadConfigFromFile (file);
                                   safe->loadChooser_.reset();
                               });
}

void MainContentComponent::saveConfigToFile (const juce::File& cfgFile)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("source", sourceCombo_.getSelectedId());
    obj->setProperty ("source_expanded", sourceExpanded_);
    obj->setProperty ("out_ltc_expanded", outLtcExpanded_);
    obj->setProperty ("out_mtc_expanded", outMtcExpanded_);
    obj->setProperty ("out_artnet_expanded", outArtExpanded_);
    obj->setProperty ("close_to_tray", closeToTray_);
    obj->setProperty ("ltc_in_driver", ltcInDriverCombo_.getText());
    obj->setProperty ("ltc_out_driver", ltcOutDriverCombo_.getText());
    obj->setProperty ("ltc_in_device", ltcInDeviceCombo_.getText());
    obj->setProperty ("ltc_out_device", ltcOutDeviceCombo_.getText());
    obj->setProperty ("mtc_in", mtcInCombo_.getText());
    obj->setProperty ("mtc_out", mtcOutCombo_.getText());
    obj->setProperty ("artnet_in", artnetInCombo_.getText());
    obj->setProperty ("artnet_out", artnetOutCombo_.getText());
    obj->setProperty ("osc_adapter", oscAdapterCombo_.getText());
    obj->setProperty ("osc_ip", oscIpEditor_.getText());
    obj->setProperty ("osc_port", oscPortEditor_.getText());
    obj->setProperty ("osc_fps", oscFpsCombo_.getText());
    obj->setProperty ("osc_str", oscAddrStrEditor_.getText());
    obj->setProperty ("osc_float", oscAddrFloatEditor_.getText());
    obj->setProperty ("artnet_dest", artnetDestIpEditor_.getText());
    obj->setProperty ("ltc_offset", ltcOffsetEditor_.getText());
    obj->setProperty ("mtc_offset", mtcOffsetEditor_.getText());
    obj->setProperty ("artnet_offset", artnetOffsetEditor_.getText());

    if (cfgFile.replaceWithText (juce::JSON::toString (juce::var (obj.get()), true)))
    {
        lastConfigFile_ = cfgFile;
        saveRuntimePrefs();
        setStatusText ("STOPPED - config saved: " + cfgFile.getFileName(), juce::Colour::fromRGB (255, 120, 110));
    }
    else
        setStatusText ("STOPPED - failed to save config", juce::Colour::fromRGB (0xff, 0x9f, 0x43));
}

void MainContentComponent::loadConfigFromFile (const juce::File& cfgFile)
{
    if (! cfgFile.existsAsFile())
    {
        setStatusText ("STOPPED - config not found", juce::Colour::fromRGB (0xff, 0x9f, 0x43));
        return;
    }

    const auto parsed = juce::JSON::parse (cfgFile);
    if (! parsed.isObject())
    {
        setStatusText ("STOPPED - invalid config", juce::Colour::fromRGB (0xff, 0x9f, 0x43));
        return;
    }

    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return;

    auto propOr = [obj] (juce::Identifier key, juce::var fallback) -> juce::var
    {
        return obj->hasProperty (key) ? obj->getProperty (key) : fallback;
    };

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

    sourceCombo_.setSelectedId ((int) propOr ("source", sourceCombo_.getSelectedId()), juce::dontSendNotification);
    sourceExpanded_ = (bool) propOr ("source_expanded", sourceExpanded_);
    outLtcExpanded_ = (bool) propOr ("out_ltc_expanded", outLtcExpanded_);
    outMtcExpanded_ = (bool) propOr ("out_mtc_expanded", outMtcExpanded_);
    outArtExpanded_ = (bool) propOr ("out_artnet_expanded", outArtExpanded_);
    closeToTray_ = (bool) propOr ("close_to_tray", closeToTray_);

    setComboText (ltcInDriverCombo_, propOr ("ltc_in_driver", ltcInDriverCombo_.getText()).toString());
    setComboText (ltcOutDriverCombo_, propOr ("ltc_out_driver", ltcOutDriverCombo_.getText()).toString());
    refreshLtcDeviceListsByDriver();
    setComboText (ltcInDeviceCombo_, propOr ("ltc_in_device", ltcInDeviceCombo_.getText()).toString());
    setComboText (ltcOutDeviceCombo_, propOr ("ltc_out_device", ltcOutDeviceCombo_.getText()).toString());
    setComboText (mtcInCombo_, propOr ("mtc_in", mtcInCombo_.getText()).toString());
    setComboText (mtcOutCombo_, propOr ("mtc_out", mtcOutCombo_.getText()).toString());
    setComboText (artnetInCombo_, propOr ("artnet_in", artnetInCombo_.getText()).toString());
    setComboText (artnetOutCombo_, propOr ("artnet_out", artnetOutCombo_.getText()).toString());
    setComboText (oscAdapterCombo_, propOr ("osc_adapter", oscAdapterCombo_.getText()).toString());
    setComboText (oscFpsCombo_, propOr ("osc_fps", oscFpsCombo_.getText()).toString());

    oscIpEditor_.setText (propOr ("osc_ip", oscIpEditor_.getText()).toString(), juce::dontSendNotification);
    oscPortEditor_.setText (propOr ("osc_port", oscPortEditor_.getText()).toString(), juce::dontSendNotification);
    oscAddrStrEditor_.setText (propOr ("osc_str", oscAddrStrEditor_.getText()).toString(), juce::dontSendNotification);
    oscAddrFloatEditor_.setText (propOr ("osc_float", oscAddrFloatEditor_.getText()).toString(), juce::dontSendNotification);
    artnetDestIpEditor_.setText (propOr ("artnet_dest", artnetDestIpEditor_.getText()).toString(), juce::dontSendNotification);
    ltcOffsetEditor_.setText (propOr ("ltc_offset", ltcOffsetEditor_.getText()).toString(), juce::dontSendNotification);
    mtcOffsetEditor_.setText (propOr ("mtc_offset", mtcOffsetEditor_.getText()).toString(), juce::dontSendNotification);
    artnetOffsetEditor_.setText (propOr ("artnet_offset", artnetOffsetEditor_.getText()).toString(), juce::dontSendNotification);

    sourceExpandBtn_.setExpanded (sourceExpanded_);
    outLtcExpandBtn_.setExpanded (outLtcExpanded_);
    outMtcExpandBtn_.setExpanded (outMtcExpanded_);
    outArtExpandBtn_.setExpanded (outArtExpanded_);

    restartSelectedSource();
    onOutputSettingsChanged();
    updateWindowHeight();
    resized();
    lastConfigFile_ = cfgFile;
    saveRuntimePrefs();
    setStatusText ("STOPPED - config loaded: " + cfgFile.getFileName(), juce::Colour::fromRGB (255, 120, 110));
}

juce::File MainContentComponent::prefsFilePath() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("EasyBridge")
        .getChildFile ("runtime_prefs.json");
}

void MainContentComponent::loadRuntimePrefs()
{
    const auto prefs = prefsFilePath();
    if (! prefs.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (prefs);
    if (! parsed.isObject())
        return;

    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty ("auto_load_on_startup"))
            autoLoadOnStartup_ = (bool) obj->getProperty ("auto_load_on_startup");
        if (obj->hasProperty ("close_to_tray"))
            closeToTray_ = (bool) obj->getProperty ("close_to_tray");
        if (obj->hasProperty ("last_config_path"))
        {
            const auto path = obj->getProperty ("last_config_path").toString();
            if (path.isNotEmpty())
                lastConfigFile_ = juce::File (path);
        }
    }
}

void MainContentComponent::saveRuntimePrefs() const
{
    auto prefs = prefsFilePath();
    prefs.getParentDirectory().createDirectory();

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("auto_load_on_startup", autoLoadOnStartup_);
    obj->setProperty ("close_to_tray", closeToTray_);
    obj->setProperty ("last_config_path", lastConfigFile_.getFullPathName());
    prefs.replaceWithText (juce::JSON::toString (juce::var (obj.get()), true));
}

void MainContentComponent::maybeAutoLoadConfig()
{
    if (autoLoadOnStartup_ && lastConfigFile_.existsAsFile())
        loadConfigFromFile (lastConfigFile_);
}

void MainContentComponent::resetToDefaults()
{
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceExpanded_ = true;
    outLtcExpanded_ = false;
    outMtcExpanded_ = false;
    outArtExpanded_ = false;
    sourceExpandBtn_.setExpanded (sourceExpanded_);
    outLtcExpandBtn_.setExpanded (outLtcExpanded_);
    outMtcExpandBtn_.setExpanded (outMtcExpanded_);
    outArtExpandBtn_.setExpanded (outArtExpanded_);

    ltcOutSwitch_.setState (false);
    mtcOutSwitch_.setState (false);
    artnetOutSwitch_.setState (false);
    ltcThruDot_.setState (false);
    mtcThruDot_.setState (false);
    ltcOffsetEditor_.setText ("0", juce::dontSendNotification);
    mtcOffsetEditor_.setText ("0", juce::dontSendNotification);
    artnetOffsetEditor_.setText ("0", juce::dontSendNotification);
    oscPortEditor_.setText ("9000", juce::dontSendNotification);
    oscIpEditor_.setText ("0.0.0.0", juce::dontSendNotification);
    oscAddrStrEditor_.setText ("/frames/str", juce::dontSendNotification);
    oscAddrFloatEditor_.setText ("/time", juce::dontSendNotification);

    restartSelectedSource();
    onOutputSettingsChanged();
    updateWindowHeight();
    resized();
    setStatusText ("Config reset", juce::Colour::fromRGB (255, 120, 110));
}

void MainContentComponent::openSettingsMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save config...");
    m.addItem (2, "Load config...");
    m.addItem (3, "Reset config");
    m.addSeparator();
    m.addItem (5, "Load on startup", true, autoLoadOnStartup_);
    m.addItem (4, "Close to tray", true, closeToTray_);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton_),
                     [safe = juce::Component::SafePointer<MainContentComponent> (this)] (int result)
                     {
                         if (safe == nullptr)
                             return;

                         switch (result)
                         {
                             case 1: safe->saveConfigAs(); break;
                             case 2: safe->loadConfigFrom(); break;
                             case 3: safe->resetToDefaults(); break;
                             case 4:
                                 safe->closeToTray_ = ! safe->closeToTray_;
                                 safe->saveRuntimePrefs();
                                 safe->setStatusText (safe->closeToTray_ ? "STOPPED - close to tray ON" : "STOPPED - close to tray OFF",
                                                      juce::Colour::fromRGB (255, 120, 110));
                                 break;
                             case 5:
                                 safe->autoLoadOnStartup_ = ! safe->autoLoadOnStartup_;
                                 safe->saveRuntimePrefs();
                                 safe->setStatusText (safe->autoLoadOnStartup_ ? "STOPPED - load on startup ON"
                                                                                : "STOPPED - load on startup OFF",
                                                      juce::Colour::fromRGB (255, 120, 110));
                                 break;
                             default: break;
                         }
                     });
}

void MainContentComponent::openHelpPage()
{
    auto base = findBridgeBaseDir();
    if (! base.exists())
        return;

    auto help = base.getChildFile ("Help/easy_bridge_v2_help.html");
    if (help.existsAsFile())
        juce::URL (help.getFullPathName()).launchInDefaultBrowser();
}

MainWindow::MainWindow()
    : juce::DocumentWindow ("Easy Bridge",
                            juce::Colours::black,
                            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (false, false);
    setResizeLimits (430, 420, 430, 1600);
    setContentOwned (new MainContentComponent(), true);
    const auto icon = loadBridgeAppIcon();
    setIcon (icon);
    createTrayIcon();
    if (trayIcon_ != nullptr && icon.isValid())
        trayIcon_->setIconImage (icon, icon);
    centreWithSize (430, 420);
    setVisible (true);

#if JUCE_WINDOWS
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainWindow> (this)]
    {
        if (safe != nullptr)
            applyNativeDarkTitleBar (*safe);
    });
#endif
}

MainWindow::~MainWindow() = default;

void MainWindow::closeButtonPressed()
{
    if (quittingFromMenu_)
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return;
    }

    bool closeToTray = true;
    if (auto* content = dynamic_cast<MainContentComponent*> (getContentComponent()))
        closeToTray = content->closeToTrayEnabled();

    if (closeToTray)
    {
        setVisible (false);
        if (trayIcon_ != nullptr)
            trayIcon_->showInfoBubble ("Easy Bridge", "Running in system tray");
        return;
    }

    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void MainWindow::createTrayIcon()
{
    trayIcon_ = std::make_unique<BridgeTrayIcon> (*this);
    trayIcon_->setIconTooltip ("Easy Bridge");
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
} // namespace bridge
