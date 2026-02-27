#include "TriggerMainWindow.h"
#include "core/BridgeVersion.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace trigger
{
namespace
{
juce::String inputSourceName (int id)
{
    switch (id)
    {
        case 1: return "LTC";
        case 2: return "MTC";
        case 3: return "ArtNet";
        case 4: return "OSC";
        default: return "LTC";
    }
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
        auto trigger = r.getChildFile ("EasyTrigger");
        if (trigger.exists())
            return trigger;

        auto bridge = r.getChildFile ("MTC_Bridge");
        if (bridge.exists())
            return bridge;
    }

    return {};
}

void logUi (const juce::String& line)
{
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto file = exeDir.getChildFile ("easytrigger_ui.log");
    const auto text = juce::Time::getCurrentTime().toString (true, true) + " | " + line + "\n";
    file.appendText (text, false, false, "\n");
}

class InlineTextCell final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> onCommit;
    InlineTextCell()
    {
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
        setBorder (juce::BorderSize<int> (0));
        setIndents (4, 0);
        setJustification (juce::Justification::centredLeft);
        onReturnKey = [this] { if (onCommit) onCommit (getText()); };
        onFocusLost = [this] { if (onCommit) onCommit (getText()); };
    }
};

class InlineButtonCell final : public juce::TextButton
{
public:
    std::function<void()> onPress;
    InlineButtonCell()
    {
        onClick = [this] { if (onPress) onPress(); };
    }
};

class InlineTestButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;

    InlineTestButtonCell() : juce::Button ("test")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (30.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());

        auto fill = juce::Colour::fromRGB (0x2a, 0x2a, 0x2a);
        auto stroke = juce::Colour::fromRGB (0x4a, 0x4a, 0x4a);
        auto icon = juce::Colour::fromRGB (0x90, 0x90, 0x90);
        if (isHovered) fill = fill.brighter (0.12f);
        if (isDown) fill = fill.brighter (0.18f);

        g.setColour (fill);
        g.fillRoundedRectangle (sq, 5.0f);
        g.setColour (stroke);
        g.drawRoundedRectangle (sq, 5.0f, 1.0f);

        juce::Path tri;
        const auto cx = sq.getCentreX();
        const auto cy = sq.getCentreY();
        const float w = 7.0f;
        const float h = 9.0f;
        tri.startNewSubPath (cx - w * 0.5f, cy - h * 0.5f);
        tri.lineTo (cx - w * 0.5f, cy + h * 0.5f);
        tri.lineTo (cx + w * 0.6f, cy);
        tri.closeSubPath();
        g.setColour (icon);
        g.fillPath (tri);
    }
};

class InlineEndActionCell final : public juce::Component
{
public:
    std::function<void(const juce::String&, const juce::String&, const juce::String&, const juce::String&)> onChanged;

    InlineEndActionCell()
    {
        addAndMakeVisible (modeBtn_);
        addAndMakeVisible (value1_);
        addAndMakeVisible (value2_);

        modeBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
        modeBtn_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
        modeBtn_.onClick = [this]
        {
            if (mode_ == "off") mode_ = "col";
            else if (mode_ == "col") mode_ = "lc";
            else mode_ = "off";
            applyModeToUi();
            emitChanged();
        };

        auto styleEditor = [] (juce::TextEditor& e)
        {
            e.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0x24, 0x24, 0x24));
            e.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
            e.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
            e.setJustification (juce::Justification::centredLeft);
            e.onReturnKey = [&e] { e.giveAwayKeyboardFocus(); };
        };
        styleEditor (value1_);
        styleEditor (value2_);

        value1_.onFocusLost = [this] { emitChanged(); };
        value2_.onFocusLost = [this] { emitChanged(); };
    }

    void setState (const juce::String& mode, const juce::String& col, const juce::String& layer, const juce::String& clip)
    {
        mode_ = mode.isNotEmpty() ? mode : "off";
        col_ = col;
        layer_ = layer;
        clip_ = clip;
        value1_.setText (mode_ == "col" ? col_ : layer_, juce::dontSendNotification);
        value2_.setText (clip_, juce::dontSendNotification);
        applyModeToUi();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1, 4);
        modeBtn_.setBounds (r.removeFromLeft (62));
        r.removeFromLeft (4);
        if (mode_ == "col")
        {
            value1_.setBounds (r.removeFromLeft (juce::jmin (66, r.getWidth())));
            value2_.setBounds (0, 0, 0, 0);
        }
        else if (mode_ == "lc")
        {
            const int w = juce::jmin (56, juce::jmax (42, (r.getWidth() - 4) / 2));
            value1_.setBounds (r.removeFromLeft (w));
            r.removeFromLeft (4);
            value2_.setBounds (r.removeFromLeft (w));
        }
        else
        {
            value1_.setBounds (0, 0, 0, 0);
            value2_.setBounds (0, 0, 0, 0);
        }
    }

private:
    void applyModeToUi()
    {
        const auto m = mode_.toLowerCase();
        if (m == "col")
        {
            mode_ = "col";
            modeBtn_.setButtonText ("Col");
            value1_.setVisible (true);
            value2_.setVisible (false);
        }
        else if (m == "lc")
        {
            mode_ = "lc";
            modeBtn_.setButtonText ("L/C");
            value1_.setVisible (true);
            value2_.setVisible (true);
        }
        else
        {
            mode_ = "off";
            modeBtn_.setButtonText ("Off");
            value1_.setVisible (false);
            value2_.setVisible (false);
        }
        resized();
    }

    void emitChanged()
    {
        if (mode_ == "col")
        {
            col_ = value1_.getText().trim();
            layer_.clear();
            clip_.clear();
        }
        else if (mode_ == "lc")
        {
            layer_ = value1_.getText().trim();
            clip_ = value2_.getText().trim();
            col_.clear();
        }
        else
        {
            col_.clear();
            layer_.clear();
            clip_.clear();
        }

        if (onChanged)
            onChanged (mode_, col_, mode_ == "lc" ? layer_ : juce::String(), mode_ == "lc" ? clip_ : juce::String());
    }

    juce::TextButton modeBtn_;
    juce::TextEditor value1_;
    juce::TextEditor value2_;
    juce::String mode_ { "off" };
    juce::String col_;
    juce::String layer_;
    juce::String clip_;
};
}

TriggerContentComponent::TriggerContentComponent()
{
    loadFonts();
    applyTheme();

    easyLabel_.setText ("EASY", juce::dontSendNotification);
    easyLabel_.setJustificationType (juce::Justification::centredLeft);
    easyLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    easyLabel_.setFont (headerBold_.withHeight (34.0f));
    triggerLabel_.setText ("TRIGGER", juce::dontSendNotification);
    triggerLabel_.setJustificationType (juce::Justification::centredLeft);
    triggerLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xa1, 0xa5, 0xac));
    triggerLabel_.setFont (headerLight_.withHeight (34.0f));
    versionLabel_.setJustificationType (juce::Justification::centredRight);
    versionLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0x8b, 0x91, 0x9a));
    versionLabel_.setFont (juce::FontOptions (12.0f));
    tcLabel_.setText ("00:00:00:00", juce::dontSendNotification);
    tcLabel_.setJustificationType (juce::Justification::centred);
    tcLabel_.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (20, 20, 20));
    tcLabel_.setColour (juce::Label::outlineColourId, juce::Colour::fromRGB (48, 48, 48));
    tcLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (220, 216, 203));
    tcLabel_.setFont (mono_.withHeight (62.0f));
    fpsLabel_.setColour (juce::Label::textColourId, juce::Colours::white);
    sourceLevelMeter_.setMeterColour (juce::Colour::fromRGB (0x3d, 0x80, 0x70));

    sourceCombo_.addItem ("LTC", 1);
    sourceCombo_.addItem ("MTC", 2);
    sourceCombo_.addItem ("ArtNet", 3);
    sourceCombo_.addItem ("OSC", 4);
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceCombo_.onChange = [this]
    {
        refreshInputsForSource();
        startInput();
        resized();
        repaint();
    };

    sourceDriverCombo_.addItem ("Default (all devices)", 1);
    sourceDriverCombo_.addItem ("ASIO", 2);
    sourceDriverCombo_.addItem ("WASAPI", 3);
    sourceDriverCombo_.addItem ("DirectSound", 4);
    sourceDriverCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceDriverCombo_.onChange = [this] { refreshInputsForSource(); startInput(); };

    for (int i = 1; i <= 8; ++i)
    {
        sourceChannelCombo_.addItem (juce::String (i), i);
        ltcOutChannelCombo_.addItem (juce::String (i), i);
    }
    sourceRateCombo_.addItem ("44100", 1);
    sourceRateCombo_.addItem ("48000", 2);
    sourceRateCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceChannelCombo_.onChange = [this] { startInput(); };
    sourceRateCombo_.onChange = [this] { startInput(); };
    sourceGainSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    sourceGainSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    sourceGainSlider_.setRange (-24.0, 24.0, 0.1);
    sourceGainSlider_.setValue (0.0);
    sourceGainSlider_.onValueChange = [this]
    {
        bridgeEngine_.setLtcInputGain ((float) std::pow (10.0, sourceGainSlider_.getValue() / 20.0));
    };

    oscAdapterCombo_.addItem ("ALL INTERFACES (0.0.0.0)", 1);
    oscAdapterCombo_.addItem ("Loopback (127.0.0.1)", 2);
    oscAdapterCombo_.setSelectedId (1, juce::dontSendNotification);
    oscIpEditor_.setText ("0.0.0.0");
    oscFpsCombo_.addItem ("24", 1);
    oscFpsCombo_.addItem ("25", 2);
    oscFpsCombo_.addItem ("29.97", 3);
    oscFpsCombo_.addItem ("30", 4);
    oscFpsCombo_.setSelectedId (2, juce::dontSendNotification);
    oscCmdStrEditor_.setText ("/frames/str");
    oscCmdFloatEditor_.setText ("/time");

    oscPortEditor_.setText ("9000");
    oscPortEditor_.setInputRestrictions (5, "0123456789");
    oscPortEditor_.onTextChange = [this] { if (sourceCombo_.getSelectedId() == 4) startInput(); };
    oscCmdStrEditor_.onTextChange = [this] { if (sourceCombo_.getSelectedId() == 4) startInput(); };
    oscCmdFloatEditor_.onTextChange = [this] { if (sourceCombo_.getSelectedId() == 4) startInput(); };

    ltcOutSwitch_.onToggle = [this] (bool) { applyLtcOutput(); };
    ltcThruDot_.setState (false);

    ltcOutDriverCombo_.addItem ("Default (all devices)", 1);
    ltcOutDriverCombo_.addItem ("ASIO", 2);
    ltcOutDriverCombo_.addItem ("WASAPI", 3);
    ltcOutDriverCombo_.addItem ("DirectSound", 4);
    ltcOutDriverCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutDriverCombo_.onChange = [this] { refreshLtcOutDevices(); applyLtcOutput(); };
    ltcOutDeviceCombo_.onChange = [this] { applyLtcOutput(); };
    ltcOutChannelCombo_.onChange = [this] { applyLtcOutput(); };
    ltcOutRateCombo_.addItem ("44100", 1);
    ltcOutRateCombo_.addItem ("48000", 2);
    ltcOutRateCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutRateCombo_.onChange = [this] { applyLtcOutput(); };
    ltcOutOffsetEditor_.setText ("0");
    ltcOutOffsetEditor_.setInputRestrictions (4, "-0123456789");
    ltcOutLevelSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    ltcOutLevelSlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    ltcOutLevelSlider_.setRange (-24.0, 24.0, 0.1);
    ltcOutLevelSlider_.setValue (0.0);
    ltcOutLevelSlider_.onValueChange = [this]
    {
        bridgeEngine_.setLtcOutputGain ((float) std::pow (10.0, ltcOutLevelSlider_.getValue() / 20.0));
    };

    resolumeSendIp_.setText ("127.0.0.1");
    resolumeSendPort_.setText ("7000");
    resolumeListenIp_.setText ("0.0.0.0");
    resolumeListenPort_.setText ("7001");
    resolumeMaxLayers_.setText ("12");
    resolumeMaxClips_.setText ("32");
    getTriggersBtn_.onClick = [this] { queryResolume(); };
    sourceExpandBtn_.setExpanded (true);
    resolumeExpandBtn_.setExpanded (false);
    ltcOutExpandBtn_.setExpanded (false);
    sourceExpandBtn_.onClick = [this] { sourceExpanded_ = ! sourceExpanded_; sourceExpandBtn_.setExpanded (sourceExpanded_); resized(); repaint(); };
    resolumeExpandBtn_.onClick = [this] { resolumeExpanded_ = ! resolumeExpanded_; resolumeExpandBtn_.setExpanded (resolumeExpanded_); resized(); repaint(); };
    ltcOutExpandBtn_.onClick = [this] { ltcOutExpanded_ = ! ltcOutExpanded_; ltcOutExpandBtn_.setExpanded (ltcOutExpanded_); resized(); repaint(); };

    triggerTable_.setModel (this);
    triggerTable_.setRowHeight (36);
    triggerTable_.setOutlineThickness (0);
    triggerTable_.setColour (juce::ListBox::outlineColourId, juce::Colour::fromRGB (0x3f, 0x3f, 0x3f));
    triggerTable_.setColour (juce::ListBox::backgroundColourId, bg_);
    auto& h = triggerTable_.getHeader();
    h.addColumn ("", 1, 40);
    h.addColumn ("In", 2, 46);
    h.addColumn ("Name", 3, 260);
    h.addColumn ("Count", 4, 110);
    h.addColumn ("Range", 5, 70);
    h.addColumn ("Trigger", 6, 110);
    h.addColumn ("Duration", 7, 110);
    h.addColumn ("End Action", 8, 180);
    h.addColumn ("Test", 9, 56);
    h.setStretchToFitActive (false);
    h.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
    h.setColour (juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));

    clipCollector_.onChanged = [safe = juce::Component::SafePointer<TriggerContentComponent> (this)]
    {
        if (safe == nullptr)
            return;
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
                safe->refreshTriggerRows();
        });
    };

    addAndMakeVisible (easyLabel_);
    addAndMakeVisible (triggerLabel_);
    addAndMakeVisible (versionLabel_);
    addAndMakeVisible (tcLabel_);
    addAndMakeVisible (fpsLabel_);
    addAndMakeVisible (statusLabel_);
    addAndMakeVisible (sourceHeader_);
    addAndMakeVisible (resolumeHeader_);
    addAndMakeVisible (ltcOutHeader_);
    addAndMakeVisible (sourceExpandBtn_);
    addAndMakeVisible (resolumeExpandBtn_);
    addAndMakeVisible (ltcOutExpandBtn_);
    addAndMakeVisible (sourceLbl_);
    addAndMakeVisible (sourceDriverLbl_);
    addAndMakeVisible (sourceDeviceLbl_);
    addAndMakeVisible (sourceChannelLbl_);
    addAndMakeVisible (sourceRateLbl_);
    addAndMakeVisible (sourceLevelLbl_);
    addAndMakeVisible (sourceGainLbl_);
    addAndMakeVisible (sourceMtcLbl_);
    addAndMakeVisible (sourceArtLbl_);
    addAndMakeVisible (oscPortLbl_);
    addAndMakeVisible (oscAdapterLbl_);
    addAndMakeVisible (oscIpLbl_);
    addAndMakeVisible (oscFpsLbl_);
    addAndMakeVisible (oscCmdStrLbl_);
    addAndMakeVisible (oscCmdFloatLbl_);
    addAndMakeVisible (ltcOutDriverLbl_);
    addAndMakeVisible (ltcOutDeviceLbl_);
    addAndMakeVisible (ltcOutChannelLbl_);
    addAndMakeVisible (ltcOutRateLbl_);
    addAndMakeVisible (ltcOutOffsetLbl_);
    addAndMakeVisible (ltcOutLevelLbl_);
    addAndMakeVisible (ltcThruLbl_);
    addAndMakeVisible (resSendIpLbl_);
    addAndMakeVisible (resSendPortLbl_);
    addAndMakeVisible (resListenIpLbl_);
    addAndMakeVisible (resListenPortLbl_);
    addAndMakeVisible (resMaxLayersLbl_);
    addAndMakeVisible (resMaxClipsLbl_);
    addAndMakeVisible (sourceCombo_);
    addAndMakeVisible (sourceDriverCombo_);
    addAndMakeVisible (sourceDeviceCombo_);
    addAndMakeVisible (sourceChannelCombo_);
    addAndMakeVisible (sourceRateCombo_);
    addAndMakeVisible (sourceLevelMeter_);
    addAndMakeVisible (sourceGainSlider_);
    addAndMakeVisible (sourceMtcCombo_);
    addAndMakeVisible (sourceArtCombo_);
    addAndMakeVisible (oscAdapterCombo_);
    addAndMakeVisible (oscIpEditor_);
    addAndMakeVisible (oscPortEditor_);
    addAndMakeVisible (oscFpsCombo_);
    addAndMakeVisible (oscCmdStrEditor_);
    addAndMakeVisible (oscCmdFloatEditor_);
    addAndMakeVisible (ltcOutSwitch_);
    addAndMakeVisible (ltcThruDot_);
    addAndMakeVisible (ltcOutDriverCombo_);
    addAndMakeVisible (ltcOutDeviceCombo_);
    addAndMakeVisible (ltcOutChannelCombo_);
    addAndMakeVisible (ltcOutRateCombo_);
    addAndMakeVisible (ltcOutOffsetEditor_);
    addAndMakeVisible (ltcOutLevelSlider_);
    addAndMakeVisible (resolumeSendIp_);
    addAndMakeVisible (resolumeSendPort_);
    addAndMakeVisible (resolumeListenIp_);
    addAndMakeVisible (resolumeListenPort_);
    addAndMakeVisible (resolumeMaxLayers_);
    addAndMakeVisible (resolumeMaxClips_);
    addAndMakeVisible (getTriggersBtn_);
    addAndMakeVisible (triggerTable_);

    refreshInputsForSource();
    refreshLtcOutDevices();
    startInput();
    startTimerHz (30);
}

TriggerContentComponent::~TriggerContentComponent()
{
    clipCollector_.stopListening();
}

void TriggerContentComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg_);
    if (! headerRect_.isEmpty())
    {
        g.setColour (juce::Colour::fromRGB (0x2f, 0x2f, 0x32));
        g.fillRoundedRectangle (headerRect_.toFloat(), 5.0f);
        g.setColour (juce::Colour::fromRGB (0x3c, 0x3e, 0x42));
        g.drawRoundedRectangle (headerRect_.toFloat(), 5.0f, 1.0f);
    }
    g.setColour (juce::Colour::fromRGB (0x65, 0x65, 0x65));
    for (auto r : sectionRowRects_)
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
    for (auto r : leftRowRects_)
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (bg_);
    for (auto r : rightSectionRects_)
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
}

void TriggerContentComponent::resized()
{
    leftRowRects_.clear();
    sectionRowRects_.clear();
    rightSectionRects_.clear();
    headerRect_ = {};

    auto content = getLocalBounds().reduced (8);
    const int totalW = content.getWidth();
    int leftW = juce::jlimit (330, 390, (int) std::round ((double) totalW * 0.40));
    if (totalW - leftW < 420)
        leftW = juce::jmax (300, totalW - 420);
    auto left = content.removeFromLeft (leftW);
    auto right = content.reduced (6, 0);
    auto statusArea = left.removeFromBottom (24);
    left.removeFromBottom (4);

    headerRect_ = left.removeFromTop (40);
    auto top = headerRect_.reduced (8, 0);
    versionLabel_.setBounds (top.removeFromRight (70));
    const int easyW = juce::jmax (46, easyLabel_.getFont().getStringWidth ("EASY") + 6);
    const int trigW = juce::jmax (90, triggerLabel_.getFont().getStringWidth ("TRIGGER") + 6);
    const int startX = top.getX() + 2;
    const int yOff = 3;
    easyLabel_.setBounds (startX, top.getY() + yOff, easyW, top.getHeight() - yOff);
    triggerLabel_.setBounds (startX + easyW, top.getY() + yOff, trigW, top.getHeight() - yOff);
    left.removeFromTop (4);
    tcLabel_.setBounds (left.removeFromTop (108));
    fpsLabel_.setBounds (left.removeFromTop (22));
    left.removeFromTop (4);

    auto row = [&left, this] (int h = 40)
    {
        auto r = left.removeFromTop (h);
        left.removeFromTop (4);
        leftRowRects_.add (r);
        return r;
    };
    auto layoutParam = [&] (juce::Label& lbl, juce::Component& c, int h = 40)
    {
        auto r = row (h);
        auto l = r.removeFromLeft (112);
        lbl.setBounds (l.reduced (10, 0));
        auto control = r.reduced (0, 3).reduced (2, 0);
        if (&c == &sourceLevelMeter_)
        {
            const int hMeter = 8;
            control = juce::Rectangle<int> (control.getX(), control.getCentreY() - hMeter / 2, control.getWidth(), hMeter);
        }
        c.setBounds (control);
    };

    auto headerRow = [&] (juce::Label& lbl, ExpandCircleButton& btn)
    {
        auto r = row();
        if (leftRowRects_.size() > 0)
            leftRowRects_.remove (leftRowRects_.size() - 1);
        sectionRowRects_.add (r);
        auto bh = r.removeFromLeft (36);
        const int d = 28;
        btn.setBounds (bh.getX() + 3 + (bh.getWidth() - d) / 2, bh.getY() + (bh.getHeight() - d) / 2, d, d);
        lbl.setBounds (r.reduced (6, 0));
    };

    {
        auto r = row();
        if (leftRowRects_.size() > 0)
            leftRowRects_.remove (leftRowRects_.size() - 1);
        sectionRowRects_.add (r);
        auto bh = r.removeFromLeft (36);
        const int d = 28;
        sourceExpandBtn_.setBounds (bh.getX() + 3 + (bh.getWidth() - d) / 2, bh.getY() + (bh.getHeight() - d) / 2, d, d);
        auto labelArea = r.removeFromLeft (112);
        sourceHeader_.setBounds (labelArea);
        sourceCombo_.setBounds (r.reduced (2, 3));
    }
    if (sourceExpanded_)
    {
        const int src = sourceCombo_.getSelectedId();
        if (src == 1)
        {
            layoutParam (sourceDriverLbl_, sourceDriverCombo_);
            layoutParam (sourceDeviceLbl_, sourceDeviceCombo_);
            layoutParam (sourceChannelLbl_, sourceChannelCombo_);
            layoutParam (sourceRateLbl_, sourceRateCombo_);
            layoutParam (sourceLevelLbl_, sourceLevelMeter_);
            layoutParam (sourceGainLbl_, sourceGainSlider_);
        }
        else if (src == 2)
        {
            layoutParam (sourceMtcLbl_, sourceMtcCombo_);
        }
        else if (src == 3)
        {
            layoutParam (sourceArtLbl_, sourceArtCombo_);
        }
        else
        {
            layoutParam (oscAdapterLbl_, oscAdapterCombo_);
            layoutParam (oscIpLbl_, oscIpEditor_);
            layoutParam (oscPortLbl_, oscPortEditor_);
            layoutParam (oscFpsLbl_, oscFpsCombo_);
            layoutParam (oscCmdStrLbl_, oscCmdStrEditor_);
            layoutParam (oscCmdFloatLbl_, oscCmdFloatEditor_);
        }
    }

    headerRow (resolumeHeader_, resolumeExpandBtn_);
    if (resolumeExpanded_)
    {
        layoutParam (resSendIpLbl_, resolumeSendIp_);
        layoutParam (resSendPortLbl_, resolumeSendPort_);
        layoutParam (resListenIpLbl_, resolumeListenIp_);
        layoutParam (resListenPortLbl_, resolumeListenPort_);
        layoutParam (resMaxLayersLbl_, resolumeMaxLayers_);
        layoutParam (resMaxClipsLbl_, resolumeMaxClips_);
    }

    headerRow (ltcOutHeader_, ltcOutExpandBtn_);
    {
        auto hdr = sectionRowRects_.getReference (sectionRowRects_.size() - 1);
        auto r = hdr;
        r.removeFromLeft (36 + 112);
        ltcOutSwitch_.setBounds (r.removeFromRight (54).reduced (0, 6));
        auto dotHost = r.removeFromRight (22);
        const int d = 18;
        ltcThruDot_.setBounds (dotHost.getCentreX() - d / 2, dotHost.getCentreY() - d / 2, d, d);
        ltcThruLbl_.setBounds (r.removeFromRight (42));
    }
    if (ltcOutExpanded_)
    {
        layoutParam (ltcOutDriverLbl_, ltcOutDriverCombo_);
        layoutParam (ltcOutDeviceLbl_, ltcOutDeviceCombo_);
        layoutParam (ltcOutChannelLbl_, ltcOutChannelCombo_);
        layoutParam (ltcOutRateLbl_, ltcOutRateCombo_);
        layoutParam (ltcOutOffsetLbl_, ltcOutOffsetEditor_);
        layoutParam (ltcOutLevelLbl_, ltcOutLevelSlider_);
    }

    const int src = sourceCombo_.getSelectedId();
    sourceLbl_.setVisible (false);
    sourceCombo_.setVisible (true);
    sourceDriverLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceDriverCombo_.setVisible (sourceExpanded_ && src == 1);
    sourceDeviceLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceDeviceCombo_.setVisible (sourceExpanded_ && src == 1);
    sourceChannelLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceChannelCombo_.setVisible (sourceExpanded_ && src == 1);
    sourceRateLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceRateCombo_.setVisible (sourceExpanded_ && src == 1);
    sourceLevelLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceLevelMeter_.setVisible (sourceExpanded_ && src == 1);
    sourceGainLbl_.setVisible (sourceExpanded_ && src == 1);
    sourceGainSlider_.setVisible (sourceExpanded_ && src == 1);
    sourceMtcLbl_.setVisible (sourceExpanded_ && src == 2);
    sourceMtcCombo_.setVisible (sourceExpanded_ && src == 2);
    sourceArtLbl_.setVisible (sourceExpanded_ && src == 3);
    sourceArtCombo_.setVisible (sourceExpanded_ && src == 3);
    oscAdapterLbl_.setVisible (sourceExpanded_ && src == 4);
    oscAdapterCombo_.setVisible (sourceExpanded_ && src == 4);
    oscIpLbl_.setVisible (sourceExpanded_ && src == 4);
    oscIpEditor_.setVisible (sourceExpanded_ && src == 4);
    oscPortLbl_.setVisible (sourceExpanded_ && src == 4);
    oscPortEditor_.setVisible (sourceExpanded_ && src == 4);
    oscFpsLbl_.setVisible (sourceExpanded_ && src == 4);
    oscFpsCombo_.setVisible (sourceExpanded_ && src == 4);
    oscCmdStrLbl_.setVisible (sourceExpanded_ && src == 4);
    oscCmdStrEditor_.setVisible (sourceExpanded_ && src == 4);
    oscCmdFloatLbl_.setVisible (sourceExpanded_ && src == 4);
    oscCmdFloatEditor_.setVisible (sourceExpanded_ && src == 4);

    resSendIpLbl_.setVisible (resolumeExpanded_);
    resSendPortLbl_.setVisible (resolumeExpanded_);
    resListenIpLbl_.setVisible (resolumeExpanded_);
    resListenPortLbl_.setVisible (resolumeExpanded_);
    resMaxLayersLbl_.setVisible (resolumeExpanded_);
    resMaxClipsLbl_.setVisible (resolumeExpanded_);
    resolumeSendIp_.setVisible (resolumeExpanded_);
    resolumeSendPort_.setVisible (resolumeExpanded_);
    resolumeListenIp_.setVisible (resolumeExpanded_);
    resolumeListenPort_.setVisible (resolumeExpanded_);
    resolumeMaxLayers_.setVisible (resolumeExpanded_);
    resolumeMaxClips_.setVisible (resolumeExpanded_);

    ltcOutSwitch_.setVisible (true);
    ltcThruDot_.setVisible (true);
    ltcThruLbl_.setVisible (true);
    ltcOutDriverLbl_.setVisible (ltcOutExpanded_);
    ltcOutDeviceLbl_.setVisible (ltcOutExpanded_);
    ltcOutChannelLbl_.setVisible (ltcOutExpanded_);
    ltcOutRateLbl_.setVisible (ltcOutExpanded_);
    ltcOutOffsetLbl_.setVisible (ltcOutExpanded_);
    ltcOutLevelLbl_.setVisible (ltcOutExpanded_);
    ltcOutDriverCombo_.setVisible (ltcOutExpanded_);
    ltcOutDeviceCombo_.setVisible (ltcOutExpanded_);
    ltcOutChannelCombo_.setVisible (ltcOutExpanded_);
    ltcOutRateCombo_.setVisible (ltcOutExpanded_);
    ltcOutOffsetEditor_.setVisible (ltcOutExpanded_);
    ltcOutLevelSlider_.setVisible (ltcOutExpanded_);

    auto getTriggersRow = left.removeFromBottom (40);
    getTriggersBtn_.setBounds (getTriggersRow.reduced (8, 3));
    getTriggersBtn_.setVisible (true);

    statusLabel_.setBounds (statusArea.reduced (0, 2));

    rightSectionRects_.add (right);
    triggerTable_.setBounds (right.reduced (3));
    updateTableColumnWidths();
    logUi ("resized window=" + getLocalBounds().toString()
           + " leftW=" + juce::String (left.getWidth())
           + " right=" + triggerTable_.getBounds().toString());
}

void TriggerContentComponent::updateTableColumnWidths()
{
    auto& h = triggerTable_.getHeader();
    int available = triggerTable_.getWidth();
    auto& sb = triggerTable_.getVerticalScrollBar();
    if (sb.isVisible())
        available -= sb.getWidth();
    available = juce::jmax (300, available - 2);

    struct Col
    {
        int id;
        int base;
        int min;
    };

    std::array<Col, 9> cols {{
        { 1, 34, 30 },   // arrow
        { 2, 40, 36 },   // in
        { 3, 210, 160 }, // name
        { 4, 100, 90 },  // count
        { 5, 68, 62 },   // range
        { 6, 104, 96 },  // trigger
        { 7, 104, 96 },  // duration
        { 8, 170, 144 }, // end action
        { 9, 48, 44 }    // test
    }};

    int baseSum = 0;
    for (const auto& c : cols)
        baseSum += c.base;
    const double scale = (double) available / (double) baseSum;

    std::array<int, 9> widths {};
    int used = 0;
    for (size_t i = 0; i < cols.size(); ++i)
    {
        int w = juce::jmax (cols[i].min, (int) std::round ((double) cols[i].base * scale));
        widths[i] = w;
        used += w;
    }

    // Keep Test column fixed-looking; absorb delta mostly into Name.
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
}

void TriggerContentComponent::timerCallback()
{
    const auto st = bridgeEngine_.tick();
    hasLiveInputTc_ = st.hasInputTc;
    if (st.hasInputTc)
    {
        liveInputTc_ = st.inputTc;
        liveInputFps_ = st.inputFps;
    }
    const float peak = bridgeEngine_.getLtcInputPeakLevel();
    sourceLevelSmoothed_ = (sourceLevelSmoothed_ * 0.76f) + (peak * 0.24f);
    sourceLevelMeter_.setLevel (sourceLevelSmoothed_);
    updateClipCountdowns();
    if (st.hasInputTc)
    {
        tcLabel_.setText (st.inputTc.toDisplayString (st.inputFps).replaceCharacter ('.', ':'), juce::dontSendNotification);
        fpsLabel_.setText ("TC FPS: " + frameRateToString (st.inputFps), juce::dontSendNotification);
    }
    evaluateAndFireTriggers();
    const int rxCount = (int) clipCollector_.snapshot().size();
    statusLabel_.setText ("SRC " + inputSourceName (sourceCombo_.getSelectedId())
                          + " | LTC OUT " + (ltcOutSwitch_.getState() ? "ON" : "OFF")
                          + " | RX clips: " + juce::String (rxCount),
                          juce::dontSendNotification);
}

int TriggerContentComponent::getNumRows()
{
    return (int) displayRows_.size();
}

void TriggerContentComponent::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) displayRows_.size()))
        return;
    const auto& dr = displayRows_[(size_t) row];
    if (dr.isGroup)
    {
        const bool enabled = layerEnabled_[dr.layer];
        g.setColour (enabled ? juce::Colour::fromRGB (0x5a, 0x5a, 0x5a) : juce::Colour::fromRGB (0x3e, 0x3e, 0x42));
        g.fillRoundedRectangle (juce::Rectangle<float> (1.0f, 1.0f, (float) (width - 2), (float) (height - 2)), 6.0f);
        g.setColour (enabled ? juce::Colour::fromRGB (0x70, 0x70, 0x70) : juce::Colour::fromRGB (0x50, 0x50, 0x58));
        g.drawRoundedRectangle (juce::Rectangle<float> (1.0f, 1.0f, (float) (width - 2), (float) (height - 2)), 6.0f, 1.0f);
        return;
    }
    if (juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
    {
        const auto& clip = triggerRows_[(size_t) dr.clipIndex];
        const bool fired = (currentTriggerKeys_.find ({ clip.layer, clip.clip }) != currentTriggerKeys_.end());
        juce::Colour fill, border;
        if (! clip.include)
        {
            fill = juce::Colour::fromRGB (0x1a, 0x1a, 0x1d);
            border = juce::Colour::fromRGB (0x2a, 0x2a, 0x31);
        }
        else if (fired)
        {
            fill = juce::Colour::fromRGB (0xb0, 0x85, 0x00);
            border = juce::Colour::fromRGB (0xd4, 0xaa, 0x22);
        }
        else if (clip.connected)
        {
            fill = juce::Colour::fromRGB (0x42, 0x82, 0x53);
            border = juce::Colour::fromRGB (0x5a, 0x9a, 0x6a);
        }
        else
        {
            fill = selected ? juce::Colour::fromRGB (0x4b, 0x4b, 0x4b) : row_;
            border = juce::Colour::fromRGB (0x50, 0x50, 0x50);
        }

        auto rr = juce::Rectangle<float> (1.0f, 1.0f, (float) (width - 2), (float) (height - 2));
        g.setColour (fill);
        g.fillRoundedRectangle (rr, 5.0f);
        g.setColour (border);
        g.drawRoundedRectangle (rr, 5.0f, 1.0f);
        return;
    }
    else
    {
        g.setColour (selected ? juce::Colour::fromRGB (0x4b, 0x4b, 0x4b) : row_);
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
            g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
            g.drawEllipse (b, 1.4f);
            g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
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
            juce::String layerName;
            for (const auto& c : triggerRows_)
                if (c.layer == dr.layer && c.layerName.isNotEmpty()) { layerName = c.layerName; break; }
            text = layerName.isNotEmpty() ? layerName : ("Layer " + juce::String (dr.layer));
        }

        g.setColour (enabled ? juce::Colour::fromRGB (0xe0, 0xe0, 0xe0) : juce::Colour::fromRGB (0x9a, 0x9a, 0xa5));
        g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        g.drawText (text, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
        return;
    }

    if (! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return;
    const auto& it = triggerRows_[(size_t) dr.clipIndex];
    switch (columnId)
    {
        case 1:
            return;
        case 2:
        {
            auto b = juce::Rectangle<float> (12.0f, 10.0f, 20.0f, (float) height - 20.0f).withSizeKeepingCentre (20.0f, 20.0f);
            g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
            g.drawEllipse (b, 1.4f);
            g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
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
        default: break;
    }
    juce::Colour textColour = juce::Colours::white.withAlpha (0.9f);
    if (columnId == 4)
    {
        const bool fired = (currentTriggerKeys_.find ({ it.layer, it.clip }) != currentTriggerKeys_.end());
        if (! it.include) textColour = juce::Colour::fromRGB (0x53, 0x53, 0x5d);
        else if (fired) textColour = juce::Colour::fromRGB (0x6a, 0x4e, 0x00);
        else if (it.connected) textColour = juce::Colour::fromRGB (0x1e, 0x4a, 0x2a);
        else textColour = juce::Colour::fromRGB (0x7a, 0x7a, 0x84);
    }
    g.setColour (textColour);
    g.setFont (juce::FontOptions ((columnId == 4 || columnId == 6 || columnId == 7) ? 14.0f : 13.0f));
    g.drawText (text, (columnId == 3 ? 12 : 6), 0, width - 8, height, juce::Justification::centredLeft, true);
}

juce::Component* TriggerContentComponent::refreshComponentForCell (int rowNumber, int columnId, bool, juce::Component* existing)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return nullptr;
    const auto& dr = displayRows_[(size_t) rowNumber];
    if (dr.isGroup || ! juce::isPositiveAndBelow (dr.clipIndex, (int) triggerRows_.size()))
        return nullptr;

    auto& clip = triggerRows_[(size_t) dr.clipIndex];

    if (columnId == 3 || columnId == 5 || columnId == 6 || columnId == 7)
    {
        auto* ed = dynamic_cast<InlineTextCell*> (existing);
        if (ed == nullptr)
            ed = new InlineTextCell();

        const bool fired = (currentTriggerKeys_.find ({ clip.layer, clip.clip }) != currentTriggerKeys_.end());
        juce::Colour textCol = juce::Colour::fromRGB (0xc0, 0xc0, 0xc0);
        if (! clip.include) textCol = juce::Colour::fromRGB (0x47, 0x47, 0x50);
        else if (fired) textCol = juce::Colour::fromRGB (0xff, 0xe0, 0x90);
        else if (clip.connected) textCol = juce::Colour::fromRGB (0xb0, 0xe8, 0xa0);
        ed->setColour (juce::TextEditor::textColourId, textCol);

        if (columnId == 3) ed->setText (clip.name, juce::dontSendNotification);
        if (columnId == 5) ed->setText (juce::String (clip.triggerRangeSec, 1), juce::dontSendNotification);
        if (columnId == 6) ed->setText (clip.triggerTc, juce::dontSendNotification);
        if (columnId == 7) ed->setText (clip.durationTc, juce::dontSendNotification);
        ed->onCommit = [this, rowNumber, columnId] (const juce::String& v)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            if (columnId == 3) c.name = v.trim();
            if (columnId == 5) c.triggerRangeSec = juce::jmax (0.0, v.trim().replaceCharacter (',', '.').getDoubleValue());
            if (columnId == 6) c.triggerTc = v.trim();
            if (columnId == 7) c.durationTc = v.trim();
            triggerTable_.repaint();
        };
        return ed;
    }

    if (columnId == 8)
    {
        auto* cell = dynamic_cast<InlineEndActionCell*> (existing);
        if (cell == nullptr)
            cell = new InlineEndActionCell();
        cell->setState (clip.endActionMode, clip.endActionCol, clip.endActionLayer, clip.endActionClip);
        cell->onChanged = [this, rowNumber] (const juce::String& mode,
                                             const juce::String& col,
                                             const juce::String& layer,
                                             const juce::String& clipValue)
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            c.endActionMode = mode;
            c.endActionCol = col;
            c.endActionLayer = layer;
            c.endActionClip = clipValue;
            triggerTable_.updateContent();
            triggerTable_.repaint();
        };
        return cell;
    }

    if (columnId == 9)
    {
        auto* btn = dynamic_cast<InlineTestButtonCell*> (existing);
        if (btn == nullptr)
            btn = new InlineTestButtonCell();
        btn->onPress = [this, rowNumber]
        {
            if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size())) return;
            auto dr2 = displayRows_[(size_t) rowNumber];
            if (! juce::isPositiveAndBelow (dr2.clipIndex, (int) triggerRows_.size())) return;
            auto& c = triggerRows_[(size_t) dr2.clipIndex];
            sendTestTrigger (c.layer, c.clip);
            for (auto& t : triggerRows_) if (t.layer == c.layer) t.connected = false;
            c.connected = true;
            triggerTable_.repaint();
        };
        return btn;
    }
    return nullptr;
}

void TriggerContentComponent::cellClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) displayRows_.size()))
        return;

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
        rebuildDisplayRows();
        triggerTable_.updateContent();
        triggerTable_.repaint();
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
        triggerTable_.repaint();
        return;
    }
    juce::ignoreUnused (columnId);
}

void TriggerContentComponent::cellDoubleClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    juce::ignoreUnused (rowNumber, columnId);
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
        auto local = cwd.getChildFile ("EasyTrigger").getChildFile ("Fonts");
        auto bridge = cwd.getChildFile ("MTC_Bridge").getChildFile ("Fonts");
        if (local.exists()) fontsDir = local;
        else if (bridge.exists()) fontsDir = bridge;
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
    auto styleCombo = [this] (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, input_);
        c.setColour (juce::ComboBox::outlineColourId, row_);
        c.setColour (juce::ComboBox::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
        c.setColour (juce::ComboBox::arrowColourId, juce::Colour::fromRGB (0x9a, 0xa1, 0xac));
    };
    for (auto* c : { &sourceCombo_, &sourceDriverCombo_, &sourceDeviceCombo_, &sourceChannelCombo_, &sourceRateCombo_,
                     &sourceMtcCombo_, &sourceArtCombo_, &oscAdapterCombo_, &oscFpsCombo_,
                     &ltcOutDriverCombo_, &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutRateCombo_ })
        styleCombo (*c);
    for (auto* e : { &oscIpEditor_, &oscPortEditor_, &oscCmdStrEditor_, &oscCmdFloatEditor_,
                     &resolumeSendIp_, &resolumeSendPort_, &resolumeListenIp_, &resolumeListenPort_, &resolumeMaxLayers_, &resolumeMaxClips_,
                     &ltcOutOffsetEditor_ })
    {
        e->setColour (juce::TextEditor::backgroundColourId, input_);
        e->setColour (juce::TextEditor::outlineColourId, row_);
        e->setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
    }
    for (auto* l : { &sourceHeader_, &resolumeHeader_, &ltcOutHeader_,
                     &sourceLbl_, &sourceDriverLbl_, &sourceDeviceLbl_, &sourceChannelLbl_, &sourceRateLbl_, &sourceLevelLbl_, &sourceGainLbl_, &sourceMtcLbl_, &sourceArtLbl_,
                     &oscAdapterLbl_, &oscIpLbl_, &oscPortLbl_, &oscFpsLbl_, &oscCmdStrLbl_, &oscCmdFloatLbl_,
                     &ltcOutDriverLbl_, &ltcOutDeviceLbl_, &ltcOutChannelLbl_, &ltcOutRateLbl_, &ltcOutOffsetLbl_, &ltcOutLevelLbl_, &ltcThruLbl_,
                     &resSendIpLbl_, &resSendPortLbl_, &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_ })
    {
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xba, 0xc5, 0xd6));
        l->setJustificationType (juce::Justification::centredLeft);
    }
    for (auto* h : { &sourceHeader_, &resolumeHeader_, &ltcOutHeader_ })
    {
        h->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
        h->setFont (juce::FontOptions (14.0f));
        h->setJustificationType (juce::Justification::centredLeft);
    }
    ltcThruLbl_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xd0, 0xd0, 0xd0));
    sourceGainSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    sourceGainSlider_.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (0x20, 0x20, 0x20));
    sourceGainSlider_.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    sourceGainSlider_.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    sourceGainSlider_.setColour (juce::Slider::textBoxTextColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
    sourceGainSlider_.setColour (juce::Slider::textBoxOutlineColourId, row_);
    sourceGainSlider_.setColour (juce::Slider::textBoxBackgroundColourId, input_);
    ltcOutLevelSlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    ltcOutLevelSlider_.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (0x20, 0x20, 0x20));
    ltcOutLevelSlider_.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (0x3d, 0x80, 0x70));
    ltcOutLevelSlider_.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    ltcOutLevelSlider_.setColour (juce::Slider::textBoxTextColourId, juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
    ltcOutLevelSlider_.setColour (juce::Slider::textBoxOutlineColourId, row_);
    ltcOutLevelSlider_.setColour (juce::Slider::textBoxBackgroundColourId, input_);
    statusLabel_.setColour (juce::Label::backgroundColourId, row_);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xff, 0x78, 0x6e));
    getTriggersBtn_.setColour (juce::TextButton::buttonColourId, row_);
    getTriggersBtn_.setColour (juce::TextButton::buttonOnColourId, row_);
    getTriggersBtn_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
    getTriggersBtn_.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
}

void TriggerContentComponent::refreshInputsForSource()
{
    sourceDeviceCombo_.clear();
    sourceMtcCombo_.clear();
    sourceArtCombo_.clear();

    juce::StringArray names;
    if (sourceCombo_.getSelectedId() == 1)
    {
        sourceLtcChoices_ = filteredLtcInputs();
        for (const auto& c : sourceLtcChoices_) names.add (c.displayName);
    }
    else if (sourceCombo_.getSelectedId() == 2)
        names = bridgeEngine_.midiInputs();
    else if (sourceCombo_.getSelectedId() == 3)
        names = bridgeEngine_.artnetInterfaces();
    else
        names.add ("OSC");

    if (sourceCombo_.getSelectedId() == 1)
    {
        for (int i = 0; i < names.size(); ++i)
            sourceDeviceCombo_.addItem (names[i], i + 1);
        if (sourceDeviceCombo_.getNumItems() > 0)
            sourceDeviceCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
        sourceDeviceCombo_.onChange = [this] { startInput(); };
    }
    else if (sourceCombo_.getSelectedId() == 2)
    {
        for (int i = 0; i < names.size(); ++i)
            sourceMtcCombo_.addItem (names[i], i + 1);
        if (sourceMtcCombo_.getNumItems() > 0)
            sourceMtcCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
        sourceMtcCombo_.onChange = [this] { startInput(); };
    }
    else if (sourceCombo_.getSelectedId() == 3)
    {
        for (int i = 0; i < names.size(); ++i)
            sourceArtCombo_.addItem (names[i], i + 1);
        if (sourceArtCombo_.getNumItems() > 0)
            sourceArtCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
        sourceArtCombo_.onChange = [this] { startInput(); };
    }
}

void TriggerContentComponent::refreshLtcOutDevices()
{
    ltcOutChoices_ = bridgeEngine_.scanAudioOutputs();
    ltcOutDeviceCombo_.clear();
    for (int i = 0; i < ltcOutChoices_.size(); ++i)
        ltcOutDeviceCombo_.addItem (ltcOutChoices_[i].displayName, i + 1);
    if (ltcOutDeviceCombo_.getNumItems() > 0)
        ltcOutDeviceCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
}

void TriggerContentComponent::startInput()
{
    bridgeEngine_.stopLtcInput();
    bridgeEngine_.stopMtcInput();
    bridgeEngine_.stopArtnetInput();
    bridgeEngine_.stopOscInput();

    juce::String err;
    const int src = sourceCombo_.getSelectedId();
    if (src == 1)
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::LTC);
        sourceLtcChoices_ = filteredLtcInputs();
        const int idx = sourceDeviceCombo_.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, sourceLtcChoices_.size()))
            bridgeEngine_.startLtcInput (sourceLtcChoices_[(size_t) idx],
                                         juce::jmax (0, sourceChannelCombo_.getSelectedId() - 1),
                                         sourceRateCombo_.getSelectedId() == 2 ? 48000.0 : 44100.0,
                                         512, err);
    }
    else if (src == 2)
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::MTC);
        bridgeEngine_.startMtcInput (sourceMtcCombo_.getSelectedItemIndex(), err);
    }
    else if (src == 3)
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::ArtNet);
        bridgeEngine_.startArtnetInput (sourceArtCombo_.getSelectedItemIndex(), err);
    }
    else
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::OSC);
        FrameRate fps = FrameRate::FPS_25;
        if (oscFpsCombo_.getSelectedId() == 1) fps = FrameRate::FPS_24;
        else if (oscFpsCombo_.getSelectedId() == 3) fps = FrameRate::FPS_2997;
        else if (oscFpsCombo_.getSelectedId() == 4) fps = FrameRate::FPS_30;
        const auto bindIp = oscIpEditor_.getText().trim().isNotEmpty() ? oscIpEditor_.getText().trim() : juce::String ("0.0.0.0");
        const auto strCmd = oscCmdStrEditor_.getText().trim().isNotEmpty() ? oscCmdStrEditor_.getText().trim() : juce::String ("/frames/str");
        const auto floatCmd = oscCmdFloatEditor_.getText().trim().isNotEmpty() ? oscCmdFloatEditor_.getText().trim() : juce::String ("/time");
        bridgeEngine_.startOscInput (juce::jlimit (1, 65535, oscPortEditor_.getText().getIntValue()), bindIp, fps, strCmd, floatCmd, err);
    }

    if (err.isNotEmpty())
        statusLabel_.setText (err, juce::dontSendNotification);
}

void TriggerContentComponent::applyLtcOutput()
{
    juce::String err;
    if (! ltcOutSwitch_.getState())
    {
        bridgeEngine_.setLtcOutputEnabled (false);
        return;
    }

    const int idx = ltcOutDeviceCombo_.getSelectedItemIndex();
    if (juce::isPositiveAndBelow (idx, ltcOutChoices_.size()))
    {
        const double sr = (ltcOutRateCombo_.getSelectedId() == 2 ? 48000.0 : 44100.0);
        bridgeEngine_.startLtcOutput (ltcOutChoices_[idx], juce::jmax (0, ltcOutChannelCombo_.getSelectedId() - 1), sr, 512, err);
        bridgeEngine_.setLtcOutputEnabled (true);
    }
    if (err.isNotEmpty())
        statusLabel_.setText (err, juce::dontSendNotification);
}

void TriggerContentComponent::refreshTriggerRows()
{
    std::map<juce::String, TriggerClip> prevByKey;
    for (const auto& p : triggerRows_)
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
        if (! c.hasOffset)
            continue;
        TriggerClip row;
        row.layer = c.layer;
        row.clip = c.clip;
        row.include = true;
        row.name = c.clipName.isNotEmpty() ? c.clipName : ("Layer " + juce::String (c.layer) + " Clip " + juce::String (c.clip));
        row.layerName = c.layerName;
        row.countdownTc = "00:00:00:00";
        row.triggerRangeSec = 5.0;
        row.durationTc = secondsToTc (c.durationSeconds, FrameRate::FPS_25);
        row.triggerTc = secondsToTc (c.offsetSeconds, FrameRate::FPS_25);
        row.endActionMode = "off";
        row.connected = c.connected;
        row.timecodeHit = false;

        const auto key = juce::String (row.layer) + ":" + juce::String (row.clip);
        if (auto it = prevByKey.find (key); it != prevByKey.end())
        {
            const auto& old = it->second;
            // Keep local trigger configuration, but always refresh live clip data from Resolume.
            row.include = old.include;
            row.triggerRangeSec = old.triggerRangeSec;
            row.endActionMode = old.endActionMode;
            row.endActionCol = old.endActionCol;
            row.endActionLayer = old.endActionLayer;
            row.endActionClip = old.endActionClip;
        }
        triggerRows_.push_back (row);
    }

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

    rebuildDisplayRows();
    triggerTable_.updateContent();
    triggerTable_.repaint();
    logUi ("refreshTriggerRows clips=" + juce::String ((int) clips.size())
           + " rows=" + juce::String ((int) triggerRows_.size()));
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
        t.timecodeHit = std::abs (remain) <= juce::jmax (1, (int) std::round (t.triggerRangeSec * fps));
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

    if (! hasLastInputFrames_)
    {
        lastInputFrames_ = currentFrames;
        hasLastInputFrames_ = true;
        return;
    }

    const int prevFrames = lastInputFrames_;
    lastInputFrames_ = currentFrames;

    struct Candidate
    {
        int index { -1 };
        int score { (std::numeric_limits<int>::max)() };
        bool isCrossOnly { false };
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
        const int winStart = trig - range;
        const int winEnd = trig + range;
        const bool inNow = currentFrames >= winStart && currentFrames <= winEnd;
        const bool wasIn = triggerRangeActive_.count (key) > 0 ? triggerRangeActive_[key] : false;
        newRangeState[key] = inNow;

        bool crossed = false;
        if (prevFrames != currentFrames)
        {
            const int lo = juce::jmin (prevFrames, currentFrames);
            const int hi = juce::jmax (prevFrames, currentFrames);
            crossed = !(hi < winStart || lo > winEnd);
        }

        if (! inNow && ! crossed)
            continue;
        if (inNow && wasIn)
            continue; // do not retrigger while staying in the same window

        auto& best = bestByLayer[t.layer];
        if (best.index < 0)
        {
            best.index = i;
            best.score = std::abs (currentFrames - trig);
            best.isCrossOnly = ! inNow;
            continue;
        }

        const int score = std::abs (currentFrames - trig);
        const bool crossOnly = ! inNow;
        if (best.isCrossOnly && ! crossOnly)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = false;
        }
        else if (best.isCrossOnly == crossOnly && score < best.score)
        {
            best.index = i;
            best.score = score;
            best.isCrossOnly = crossOnly;
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
        sendTestTrigger (t.layer, t.clip);
        for (auto& x : triggerRows_)
            if (x.layer == t.layer)
                x.connected = false;
        t.connected = true;

        // Python parity: keep last fired clip highlighted (orange) per layer until next fire on that layer.
        for (auto it = currentTriggerKeys_.begin(); it != currentTriggerKeys_.end();)
        {
            if (it->first == t.layer) it = currentTriggerKeys_.erase (it);
            else ++it;
        }
        currentTriggerKeys_.insert ({ t.layer, t.clip });
    }
    triggerTable_.repaint();
}

juce::Array<bridge::engine::AudioChoice> TriggerContentComponent::filteredLtcInputs() const
{
    auto all = bridgeEngine_.scanAudioInputs();
    const int drv = sourceDriverCombo_.getSelectedId();
    if (drv <= 1)
        return all;

    juce::String needle;
    if (drv == 2) needle = "ASIO";
    else if (drv == 3) needle = "WASAPI";
    else if (drv == 4) needle = "DirectSound";
    if (needle.isEmpty())
        return all;

    juce::Array<bridge::engine::AudioChoice> filtered;
    for (const auto& c : all)
    {
        const auto txt = c.displayName.toUpperCase();
        if (txt.contains (needle.toUpperCase()))
            filtered.add (c);
    }
    if (filtered.isEmpty())
        return all;
    return filtered;
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

void TriggerContentComponent::sendTestTrigger (int layer, int clip)
{
    if (layer < 1 || clip < 1)
        return;
    juce::OSCSender s;
    const auto ip = resolumeSendIp_.getText().trim().isNotEmpty() ? resolumeSendIp_.getText().trim() : juce::String ("127.0.0.1");
    const int port = juce::jlimit (1, 65535, resolumeSendPort_.getText().trim().getIntValue());
    if (! s.connect (ip, port))
        return;
    const auto addr = "/composition/layers/" + juce::String (layer) + "/clips/" + juce::String (clip) + "/connect";
    juce::OSCMessage on (addr); on.addInt32 (1);
    juce::OSCMessage off (addr); off.addInt32 (0);
    s.send (on);
    s.send (off);
    s.disconnect();
}

void TriggerContentComponent::rebuildDisplayRows()
{
    displayRows_.clear();
    std::map<int, std::vector<int>> byLayer;
    for (int i = 0; i < (int) triggerRows_.size(); ++i)
        byLayer[triggerRows_[(size_t) i].layer].push_back (i);

    for (auto& [layer, indices] : byLayer)
    {
        std::sort (indices.begin(), indices.end(), [this] (int a, int b)
        {
            return triggerRows_[(size_t) a].clip < triggerRows_[(size_t) b].clip;
        });

        if (layerExpanded_.find (layer) == layerExpanded_.end())
            layerExpanded_[layer] = true;
        bool anyIncluded = false;
        for (int idx : indices)
            anyIncluded = anyIncluded || triggerRows_[(size_t) idx].include;
        layerEnabled_[layer] = anyIncluded;

        displayRows_.push_back ({ true, layer, -1 });
        if (layerExpanded_[layer])
            for (int idx : indices)
                displayRows_.push_back ({ false, layer, idx });
    }
}

void TriggerContentComponent::queryResolume()
{
    juce::String err;
    clipCollector_.clear();
    const auto listenIp = resolumeListenIp_.getText().trim().isNotEmpty() ? resolumeListenIp_.getText().trim() : "0.0.0.0";
    if (! clipCollector_.startListening (listenIp, juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()), err))
    {
        statusLabel_.setText (err, juce::dontSendNotification);
        logUi ("queryResolume listen failed: " + err);
        return;
    }
    const auto sendIp = resolumeSendIp_.getText().trim().isNotEmpty() ? resolumeSendIp_.getText().trim() : "127.0.0.1";
    if (! clipCollector_.configureSender (sendIp, juce::jlimit (1, 65535, resolumeSendPort_.getText().getIntValue()), err))
    {
        statusLabel_.setText (err, juce::dontSendNotification);
        logUi ("queryResolume sender failed: " + err);
        return;
    }
    clipCollector_.queryClips (juce::jlimit (1, 64, resolumeMaxLayers_.getText().getIntValue()),
                               juce::jlimit (1, 256, resolumeMaxClips_.getText().getIntValue()));
    statusLabel_.setText ("Resolume query sent", juce::dontSendNotification);
    logUi ("queryResolume sent listenIp=" + listenIp
           + " listenPort=" + resolumeListenPort_.getText().trim()
           + " sendIp=" + sendIp
           + " sendPort=" + resolumeSendPort_.getText().trim()
           + " maxLayers=" + resolumeMaxLayers_.getText().trim()
           + " maxClips=" + resolumeMaxClips_.getText().trim());
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
                            juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (980, 620, 1800, 1400);
    setContentOwned (new TriggerContentComponent(), true);
    centreWithSize (1240, 820);
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
} // namespace trigger
