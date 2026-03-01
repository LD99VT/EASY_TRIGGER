#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "inputs/ArtnetInput.h"
#include "inputs/LtcInput.h"
#include "inputs/MtcInput.h"
#include "OscInput.h"
#include "outputs/ArtnetOutput.h"
#include "outputs/LtcOutput.h"
#include "outputs/MtcOutput.h"

namespace bridge::engine
{
enum class InputSource
{
    LTC,
    MTC,
    ArtNet,
    OSC
};

struct AudioChoice
{
    juce::String typeName;
    juce::String deviceName;
    juce::String displayName;
};

struct RuntimeStatus
{
    bool hasInputTc { false };
    Timecode inputTc {};
    FrameRate inputFps { FrameRate::FPS_25 };
    juce::String inputStatus { "STOPPED" };
    juce::String ltcOutStatus { "OFF" };
    juce::String mtcOutStatus { "OFF" };
    juce::String artnetOutStatus { "OFF" };
    juce::String errorText;
};

class BridgeEngine
{
public:
    BridgeEngine();
    ~BridgeEngine();

    juce::Array<AudioChoice> scanAudioInputs() const;
    juce::Array<AudioChoice> scanAudioOutputs() const;

    juce::StringArray midiInputs();
    juce::StringArray midiOutputs();
    juce::StringArray artnetInterfaces();

    bool startLtcInput (const AudioChoice& choice, int channel, double sampleRate, int bufferSize, juce::String& errorOut);
    void stopLtcInput();
    bool startMtcInput (int deviceIndex, juce::String& errorOut);
    void stopMtcInput();
    bool startArtnetInput (int interfaceIndex, const juce::String& bindIp, juce::String& errorOut);
    void stopArtnetInput();
    bool startOscInput (int port, const juce::String& bindIp, FrameRate fps, const juce::String& addrStr, const juce::String& addrFloat, juce::String& errorOut);
    void stopOscInput();

    bool startLtcOutput (const AudioChoice& choice, int channel, double sampleRate, int bufferSize, juce::String& errorOut);
    void stopLtcOutput();
    bool startMtcOutput (int deviceIndex, juce::String& errorOut);
    void stopMtcOutput();
    bool startArtnetOutput (int interfaceIndex, juce::String& errorOut);
    void stopArtnetOutput();
    void setLtcOutputEnabled (bool enabled);
    void setMtcOutputEnabled (bool enabled);
    void setArtnetOutputEnabled (bool enabled);
    void setLtcInputGain (float linearGain);
    void setLtcOutputGain (float linearGain);
    float getLtcInputPeakLevel() const;

    void setInputSource (InputSource source);
    void setOffsets (int ltcFrames, int mtcFrames, int artnetFrames);
    RuntimeStatus tick();

private:
    static FrameRate sanitizeFps (FrameRate fps);
    static Timecode applyOffsetSafe (const Timecode& tc, int frames, FrameRate fps);

    InputSource source_ { InputSource::LTC };
    int ltcOffset_ { 0 };
    int mtcOffset_ { 0 };
    int artnetOffset_ { 0 };

    juce::String lastError_;

    // Keep current LTC input selection for conflict guard with LTC output.
    juce::String ltcInType_;
    juce::String ltcInDevice_;
    int ltcInChannel_ { -1 };
    double ltcInSampleRate_ { 0.0 };
    int ltcInBufferSize_ { 0 };
    int mtcInDeviceIndex_ { -1 };
    int artnetInInterfaceIndex_ { -1 };
    juce::String artnetInBindIp_ { "0.0.0.0" };
    int oscInPort_ { 0 };
    juce::String oscInBindIp_;
    juce::String oscInAddrStr_;
    juce::String oscInAddrFloat_;
    FrameRate oscInFps_ { FrameRate::FPS_25 };

    LtcInput ltcInput_;
    MtcInput mtcInput_;
    ArtnetInput artnetInput_;
    OscInput oscInput_;

    LtcOutput ltcOutput_;
    MtcOutput mtcOutput_;
    ArtnetOutput artnetOutput_;

    bool ltcOutEnabled_ { false };
    bool mtcOutEnabled_ { false };
    bool artnetOutEnabled_ { false };

    // Cache active output configs to avoid expensive reopen when unchanged.
    juce::String ltcOutType_;
    juce::String ltcOutDevice_;
    int ltcOutChannel_ { 0 };
    double ltcOutSampleRate_ { 0.0 };
    int ltcOutBufferSize_ { 0 };
    int mtcOutDeviceIndex_ { -1 };
    int artnetOutInterfaceIndex_ { -1 };
};
} // namespace bridge::engine
