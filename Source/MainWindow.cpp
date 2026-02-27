#include "MainWindow.h"
#include "core/BridgeVersion.h"
#include <cmath>

namespace bridge
{
namespace
{
const auto kBg = juce::Colour::fromRGB (0x17, 0x17, 0x17);      // UI_BG
const auto kRow = juce::Colour::fromRGB (0x3a, 0x3a, 0x3a);     // UI_BG_ROW
const auto kSection = juce::Colour::fromRGB (0x65, 0x65, 0x65); // UI_BG_SEC
const auto kInput = juce::Colour::fromRGB (0x24, 0x24, 0x24);   // UI_BG_INPUT
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

}

MainContentComponent::MainContentComponent()
{
    loadFontsAndIcon();
    applyLookAndFeel();

    titleLabel_.setText ("EASY BRIDGE  v" + juce::String (version::kAppVersion), juce::dontSendNotification);
    titleLabel_.setJustificationType (juce::Justification::centredLeft);
    if (titleEasyFont_.getHeight() > 0.0f)
        titleLabel_.setFont (titleEasyFont_.withHeight (28.0f));
    addAndMakeVisible (titleLabel_);

    tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
    tcLabel_.setJustificationType (juce::Justification::centred);
    tcLabel_.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (20, 20, 20));
    tcLabel_.setColour (juce::Label::outlineColourId, juce::Colour::fromRGB (48, 48, 48));
    tcLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (220, 216, 203));
    if (monoFont_.getHeight() > 0.0f)
        tcLabel_.setFont (monoFont_.withHeight (58.0f));
    addAndMakeVisible (tcLabel_);

    tcFpsLabel_.setText ("TC FPS: --", juce::dontSendNotification);
    addAndMakeVisible (tcFpsLabel_);

    statusLabel_.setText ("STOPPED", juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::backgroundColourId, kRow);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (255, 120, 110));
    addAndMakeVisible (statusLabel_);

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
    h += 44 + 4;  // title
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

    h += 34; // status
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
}

void MainContentComponent::resized()
{
    paramRowRects_.clear();
    sectionRowRects_.clear();

    auto a = getLocalBounds().reduced (8);
    titleLabel_.setBounds (a.removeFromTop (44));
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

    statusLabel_.setBounds (a.removeFromBottom (34));

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
        statusLabel_.setText (err, juce::dontSendNotification);
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
        statusLabel_.setText (err, juce::dontSendNotification);
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
    // Same behavior as SuperTimecodeConverter: instant attack, exponential decay.
    ltcInLevelSmoothed_ = (peak > ltcInLevelSmoothed_) ? peak : (ltcInLevelSmoothed_ * 0.85f);
    ltcInLevelBar_.setLevel (ltcInLevelSmoothed_);

    if (st.hasInputTc)
    {
        hasLatchedTc_ = true;
        latchedTc_ = st.inputTc;
        latchedFps_ = st.inputFps;
        tcLabel_.setText (st.inputTc.toDisplayString (st.inputFps).replaceCharacter ('.', ':'), juce::dontSendNotification);
        tcFpsLabel_.setText ("TC FPS: " + frameRateToString (st.inputFps), juce::dontSendNotification);
        statusLabel_.setText ("RUNNING | LTC " + st.ltcOutStatus + " | MTC " + st.mtcOutStatus + " | ArtNet " + st.artnetOutStatus, juce::dontSendNotification);
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

MainWindow::MainWindow()
    : juce::DocumentWindow ("Easy Bridge v2 " + juce::String (version::kAppVersion),
                            juce::Colours::black,
                            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (false);
    setResizable (false, false);
    setResizeLimits (430, 420, 430, 1600);
    setContentOwned (new MainContentComponent(), true);
    centreWithSize (430, 420);
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
} // namespace bridge
