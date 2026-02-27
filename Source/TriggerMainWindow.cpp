#include "TriggerMainWindow.h"
#include "core/BridgeVersion.h"
#include <algorithm>

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
}

TriggerContentComponent::TriggerContentComponent()
{
    loadFonts();
    applyTheme();

    easyLabel_.setFont (headerBold_.withHeight (34.0f));
    triggerLabel_.setFont (headerLight_.withHeight (34.0f));
    versionLabel_.setFont (juce::FontOptions (12.0f));
    tcLabel_.setFont (mono_.withHeight (62.0f));

    sourceCombo_.addItem ("LTC", 1);
    sourceCombo_.addItem ("MTC", 2);
    sourceCombo_.addItem ("ArtNet", 3);
    sourceCombo_.addItem ("OSC", 4);
    sourceCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceCombo_.onChange = [this] { refreshInputsForSource(); startInput(); };

    for (int i = 1; i <= 8; ++i)
    {
        sourceChannelCombo_.addItem (juce::String (i), i);
        ltcOutChannelCombo_.addItem (juce::String (i), i);
    }
    sourceChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    ltcOutChannelCombo_.setSelectedId (1, juce::dontSendNotification);
    sourceChannelCombo_.onChange = [this] { startInput(); };

    oscPortEditor_.setText ("9000");
    oscPortEditor_.setInputRestrictions (5, "0123456789");
    oscPortEditor_.onTextChange = [this] { if (sourceCombo_.getSelectedId() == 4) startInput(); };

    ltcOutEnable_.setButtonText ("Out LTC");
    ltcOutEnable_.onClick = [this] { applyLtcOutput(); };

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

    resolumeSendIp_.setText ("127.0.0.1");
    resolumeSendPort_.setText ("7000");
    resolumeListenIp_.setText ("0.0.0.0");
    resolumeListenPort_.setText ("7001");
    resolumeMaxLayers_.setText ("12");
    resolumeMaxClips_.setText ("32");
    getTriggersBtn_.onClick = [this] { queryResolume(); };

    triggerTable_.setModel (this);
    auto& h = triggerTable_.getHeader();
    h.addColumn ("Layer", 1, 55);
    h.addColumn ("Clip", 2, 50);
    h.addColumn ("Name", 3, 260);
    h.addColumn ("Duration", 4, 110);
    h.addColumn ("Trigger", 5, 110);
    h.addColumn ("Conn", 6, 60);

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
    addAndMakeVisible (sourceLbl_);
    addAndMakeVisible (sourceDeviceLbl_);
    addAndMakeVisible (sourceChannelLbl_);
    addAndMakeVisible (oscPortLbl_);
    addAndMakeVisible (ltcOutDriverLbl_);
    addAndMakeVisible (ltcOutDeviceLbl_);
    addAndMakeVisible (ltcOutChannelLbl_);
    addAndMakeVisible (ltcOutRateLbl_);
    addAndMakeVisible (resSendIpLbl_);
    addAndMakeVisible (resSendPortLbl_);
    addAndMakeVisible (resListenIpLbl_);
    addAndMakeVisible (resListenPortLbl_);
    addAndMakeVisible (resMaxLayersLbl_);
    addAndMakeVisible (resMaxClipsLbl_);
    addAndMakeVisible (sourceCombo_);
    addAndMakeVisible (sourceDeviceCombo_);
    addAndMakeVisible (sourceChannelCombo_);
    addAndMakeVisible (oscPortEditor_);
    addAndMakeVisible (ltcOutEnable_);
    addAndMakeVisible (ltcOutDriverCombo_);
    addAndMakeVisible (ltcOutDeviceCombo_);
    addAndMakeVisible (ltcOutChannelCombo_);
    addAndMakeVisible (ltcOutRateCombo_);
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
    g.setColour (juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
    for (auto r : leftRowRects_)
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    g.setColour (juce::Colour::fromRGB (0x2f, 0x2f, 0x2f));
    for (auto r : rightSectionRects_)
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
}

void TriggerContentComponent::resized()
{
    leftRowRects_.clear();
    rightSectionRects_.clear();

    auto b = getLocalBounds().reduced (8);
    auto top = b.removeFromTop (40);
    easyLabel_.setBounds (top.removeFromLeft (90));
    triggerLabel_.setBounds (top.removeFromLeft (120));
    versionLabel_.setBounds (top.removeFromRight (70));

    tcLabel_.setBounds (b.removeFromTop (100).reduced (0, 2));
    fpsLabel_.setBounds (b.removeFromTop (22));
    b.removeFromTop (4);

    auto left = b.removeFromLeft (390);
    auto right = b.reduced (6, 0);

    auto row = [&left, this] (int h = 36)
    {
        auto r = left.removeFromTop (h);
        left.removeFromTop (4);
        leftRowRects_.add (r);
        return r;
    };
    auto layoutParam = [&] (juce::Label& lbl, juce::Component& c, int h = 36)
    {
        auto r = row (h);
        auto l = r.removeFromLeft (128);
        lbl.setBounds (l.reduced (8, 0));
        c.setBounds (r.reduced (3, 3));
    };

    layoutParam (sourceLbl_, sourceCombo_);
    layoutParam (sourceDeviceLbl_, sourceDeviceCombo_);
    layoutParam (sourceChannelLbl_, sourceChannelCombo_);
    layoutParam (oscPortLbl_, oscPortEditor_);

    auto outRow = row();
    ltcOutEnable_.setBounds (outRow.reduced (8, 3));
    layoutParam (ltcOutDriverLbl_, ltcOutDriverCombo_);
    layoutParam (ltcOutDeviceLbl_, ltcOutDeviceCombo_);
    layoutParam (ltcOutChannelLbl_, ltcOutChannelCombo_);
    layoutParam (ltcOutRateLbl_, ltcOutRateCombo_);

    layoutParam (resSendIpLbl_, resolumeSendIp_);
    layoutParam (resSendPortLbl_, resolumeSendPort_);
    layoutParam (resListenIpLbl_, resolumeListenIp_);
    layoutParam (resListenPortLbl_, resolumeListenPort_);
    layoutParam (resMaxLayersLbl_, resolumeMaxLayers_);
    layoutParam (resMaxClipsLbl_, resolumeMaxClips_);

    auto btn = row (40);
    getTriggersBtn_.setBounds (btn.reduced (8, 3));
    statusLabel_.setBounds (left.removeFromBottom (24).reduced (0, 2));

    rightSectionRects_.add (right);
    triggerTable_.setBounds (right.reduced (3));
}

void TriggerContentComponent::timerCallback()
{
    const auto st = bridgeEngine_.tick();
    if (st.hasInputTc)
    {
        tcLabel_.setText (st.inputTc.toDisplayString (st.inputFps).replaceCharacter ('.', ':'), juce::dontSendNotification);
        fpsLabel_.setText ("TC FPS: " + frameRateToString (st.inputFps), juce::dontSendNotification);
    }
    statusLabel_.setText ("SRC " + inputSourceName (sourceCombo_.getSelectedId()) + " | LTC OUT "
                          + (ltcOutEnable_.getToggleState() ? "ON" : "OFF"), juce::dontSendNotification);
}

int TriggerContentComponent::getNumRows()
{
    return (int) triggerRows_.size();
}

void TriggerContentComponent::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    juce::ignoreUnused (row);
    g.setColour (selected ? juce::Colour::fromRGB (0x4b, 0x4b, 0x4b) : row_);
    g.fillRect (0, 0, width, height - 1);
}

void TriggerContentComponent::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool selected)
{
    juce::ignoreUnused (selected);
    if (! juce::isPositiveAndBelow (row, (int) triggerRows_.size()))
        return;
    const auto& it = triggerRows_[(size_t) row];

    juce::String text;
    switch (columnId)
    {
        case 1: text = juce::String (it.layer); break;
        case 2: text = juce::String (it.clip); break;
        case 3: text = it.name; break;
        case 4: text = it.durationTc; break;
        case 5: text = it.triggerTc; break;
        case 6: text = it.connected ? "ON" : "--"; break;
        default: break;
    }

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (text, 6, 0, width - 8, height, juce::Justification::centredLeft, true);
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

    const auto base = juce::File::getCurrentWorkingDirectory().getChildFile ("EasyTrigger").getChildFile ("Fonts");
    headerBold_ = loadFont (base.getChildFile ("Thunder-SemiBoldLC.ttf"));
    headerLight_ = loadFont (base.getChildFile ("Thunder-LightLC.ttf"));
    mono_ = loadFont (base.getChildFile ("JetBrainsMonoNL-Bold.ttf"));
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
        c.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    };
    for (auto* c : { &sourceCombo_, &sourceDeviceCombo_, &sourceChannelCombo_, &ltcOutDriverCombo_, &ltcOutDeviceCombo_, &ltcOutChannelCombo_, &ltcOutRateCombo_ })
        styleCombo (*c);
    for (auto* e : { &oscPortEditor_, &resolumeSendIp_, &resolumeSendPort_, &resolumeListenIp_, &resolumeListenPort_, &resolumeMaxLayers_, &resolumeMaxClips_ })
    {
        e->setColour (juce::TextEditor::backgroundColourId, input_);
        e->setColour (juce::TextEditor::outlineColourId, row_);
        e->setColour (juce::TextEditor::textColourId, juce::Colours::white);
    }
    for (auto* l : { &sourceLbl_, &sourceDeviceLbl_, &sourceChannelLbl_, &oscPortLbl_,
                     &ltcOutDriverLbl_, &ltcOutDeviceLbl_, &ltcOutChannelLbl_, &ltcOutRateLbl_,
                     &resSendIpLbl_, &resSendPortLbl_, &resListenIpLbl_, &resListenPortLbl_, &resMaxLayersLbl_, &resMaxClipsLbl_ })
    {
        l->setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xba, 0xc5, 0xd6));
        l->setJustificationType (juce::Justification::centredLeft);
    }
    statusLabel_.setColour (juce::Label::backgroundColourId, row_);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xff, 0x78, 0x6e));
}

void TriggerContentComponent::refreshInputsForSource()
{
    sourceDeviceCombo_.clear();
    sourceDeviceCombo_.setEnabled (true);
    sourceChannelCombo_.setEnabled (sourceCombo_.getSelectedId() == 1);
    oscPortEditor_.setVisible (sourceCombo_.getSelectedId() == 4);

    juce::StringArray names;
    if (sourceCombo_.getSelectedId() == 1)
        for (const auto& c : bridgeEngine_.scanAudioInputs()) names.add (c.displayName);
    else if (sourceCombo_.getSelectedId() == 2)
        names = bridgeEngine_.midiInputs();
    else if (sourceCombo_.getSelectedId() == 3)
        names = bridgeEngine_.artnetInterfaces();
    else
    {
        sourceDeviceCombo_.setEnabled (false);
        names.add ("OSC listen");
    }

    for (int i = 0; i < names.size(); ++i)
        sourceDeviceCombo_.addItem (names[i], i + 1);
    if (sourceDeviceCombo_.getNumItems() > 0)
        sourceDeviceCombo_.setSelectedItemIndex (0, juce::dontSendNotification);
    sourceDeviceCombo_.onChange = [this] { startInput(); };
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
        auto inputs = bridgeEngine_.scanAudioInputs();
        const int idx = sourceDeviceCombo_.getSelectedItemIndex();
        if (juce::isPositiveAndBelow (idx, inputs.size()))
            bridgeEngine_.startLtcInput (inputs[idx], juce::jmax (0, sourceChannelCombo_.getSelectedId() - 1), 44100.0, 512, err);
    }
    else if (src == 2)
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::MTC);
        bridgeEngine_.startMtcInput (sourceDeviceCombo_.getSelectedItemIndex(), err);
    }
    else if (src == 3)
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::ArtNet);
        bridgeEngine_.startArtnetInput (sourceDeviceCombo_.getSelectedItemIndex(), err);
    }
    else
    {
        bridgeEngine_.setInputSource (bridge::engine::InputSource::OSC);
        bridgeEngine_.startOscInput (juce::jlimit (1, 65535, oscPortEditor_.getText().getIntValue()), "0.0.0.0", FrameRate::FPS_25, "/frames/str", "/time", err);
    }

    if (err.isNotEmpty())
        statusLabel_.setText (err, juce::dontSendNotification);
}

void TriggerContentComponent::applyLtcOutput()
{
    juce::String err;
    if (! ltcOutEnable_.getToggleState())
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
    triggerRows_.clear();
    auto clips = clipCollector_.snapshot();
    std::sort (clips.begin(), clips.end(), [] (const auto& a, const auto& b)
    {
        if (a.layer != b.layer) return a.layer < b.layer;
        return a.clip < b.clip;
    });

    for (const auto& c : clips)
    {
        TriggerRow row;
        row.layer = c.layer;
        row.clip = c.clip;
        row.name = c.clipName.isNotEmpty() ? c.clipName : ("Layer " + juce::String (c.layer) + " Clip " + juce::String (c.clip));
        row.durationTc = secondsToTc (c.durationSeconds, FrameRate::FPS_25);
        row.triggerTc = secondsToTc (c.offsetSeconds, FrameRate::FPS_25);
        row.connected = c.connected;
        triggerRows_.push_back (row);
    }
    triggerTable_.updateContent();
    triggerTable_.repaint();
}

void TriggerContentComponent::queryResolume()
{
    juce::String err;
    clipCollector_.clear();
    const auto listenIp = resolumeListenIp_.getText().trim().isNotEmpty() ? resolumeListenIp_.getText().trim() : "0.0.0.0";
    if (! clipCollector_.startListening (listenIp, juce::jlimit (1, 65535, resolumeListenPort_.getText().getIntValue()), err))
    {
        statusLabel_.setText (err, juce::dontSendNotification);
        return;
    }
    const auto sendIp = resolumeSendIp_.getText().trim().isNotEmpty() ? resolumeSendIp_.getText().trim() : "127.0.0.1";
    if (! clipCollector_.configureSender (sendIp, juce::jlimit (1, 65535, resolumeSendPort_.getText().getIntValue()), err))
    {
        statusLabel_.setText (err, juce::dontSendNotification);
        return;
    }
    clipCollector_.queryClips (juce::jlimit (1, 64, resolumeMaxLayers_.getText().getIntValue()),
                               juce::jlimit (1, 256, resolumeMaxClips_.getText().getIntValue()));
    statusLabel_.setText ("Resolume query sent", juce::dontSendNotification);
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
