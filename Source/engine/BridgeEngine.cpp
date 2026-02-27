#include "BridgeEngine.h"

#include <cmath>
#include <memory>

namespace bridge::engine
{
namespace
{
juce::Array<AudioChoice> scanAudioDevices (bool wantInputs)
{
    juce::Array<AudioChoice> result;
    juce::AudioDeviceManager tempMgr;
    tempMgr.initialise (128, 128, nullptr, false);

    for (auto* type : tempMgr.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;

        type->scanForDevices();
        const auto names = type->getDeviceNames (wantInputs);
        for (const auto& name : names)
        {
            AudioChoice c;
            c.typeName = type->getTypeName();
            c.deviceName = name;
            c.displayName = AudioDeviceEntry::makeDisplayName (c.typeName, c.deviceName);
            result.add (c);
        }
    }
    return result;
}
}

BridgeEngine::BridgeEngine() = default;
BridgeEngine::~BridgeEngine()
{
    stopLtcOutput();
    stopMtcOutput();
    stopArtnetOutput();
    stopLtcInput();
    stopMtcInput();
    stopArtnetInput();
    stopOscInput();
}

juce::Array<AudioChoice> BridgeEngine::scanAudioInputs() const
{
    return scanAudioDevices (true);
}

juce::Array<AudioChoice> BridgeEngine::scanAudioOutputs() const
{
    return scanAudioDevices (false);
}

juce::StringArray BridgeEngine::midiInputs()
{
    mtcInput_.refreshDeviceList();
    return mtcInput_.getDeviceNames();
}

juce::StringArray BridgeEngine::midiOutputs()
{
    mtcOutput_.refreshDeviceList();
    return mtcOutput_.getDeviceNames();
}

juce::StringArray BridgeEngine::artnetInterfaces()
{
    artnetInput_.refreshNetworkInterfaces();
    return artnetInput_.getInterfaceNames();
}

bool BridgeEngine::startLtcInput (const AudioChoice& choice, int channel, double sampleRate, int bufferSize, juce::String& errorOut)
{
    const int ch = juce::jmax (0, channel);
    const bool sameConfig =
        ltcInput_.getIsRunning()
        && choice.typeName == ltcInType_
        && choice.deviceName == ltcInDevice_
        && ch == ltcInChannel_
        && std::abs (sampleRate - ltcInSampleRate_) < 0.5
        && bufferSize == ltcInBufferSize_;

    if (sameConfig)
    {
        errorOut.clear();
        return true;
    }

    bool ok = ltcInput_.start (choice.typeName, choice.deviceName, ch, -1, sampleRate, bufferSize);
    if (! ok && (sampleRate > 0.0 || bufferSize > 0))
        ok = ltcInput_.start (choice.typeName, choice.deviceName, ch, -1, 0.0, 0);
    if (! ok)
    {
        errorOut = "Failed to start LTC input";
        return false;
    }

    ltcInType_ = choice.typeName;
    ltcInDevice_ = choice.deviceName;
    ltcInChannel_ = ch;
    ltcInSampleRate_ = sampleRate;
    ltcInBufferSize_ = bufferSize;
    errorOut.clear();
    return true;
}

void BridgeEngine::stopLtcInput()
{
    ltcInput_.stop();
    ltcInType_.clear();
    ltcInDevice_.clear();
    ltcInChannel_ = -1;
    ltcInSampleRate_ = 0.0;
    ltcInBufferSize_ = 0;
}

bool BridgeEngine::startMtcInput (int deviceIndex, juce::String& errorOut)
{
    const bool sameConfig = mtcInput_.getIsRunning() && deviceIndex == mtcInDeviceIndex_;
    if (! sameConfig)
        mtcInput_.refreshDeviceList();
    if (! sameConfig && ! mtcInput_.start (deviceIndex))
    {
        errorOut = "Failed to start MTC input";
        return false;
    }
    mtcInDeviceIndex_ = deviceIndex;
    errorOut.clear();
    return true;
}

void BridgeEngine::stopMtcInput()
{
    mtcInput_.stop();
    mtcInDeviceIndex_ = -1;
}

bool BridgeEngine::startArtnetInput (int interfaceIndex, const juce::String& bindIp, juce::String& errorOut)
{
    const auto bind = bindIp.trim().isNotEmpty() ? bindIp.trim() : juce::String ("0.0.0.0");
    const bool sameConfig =
        artnetInput_.getIsRunning()
        && interfaceIndex == artnetInInterfaceIndex_
        && bind == artnetInBindIp_;
    if (! sameConfig)
        artnetInput_.refreshNetworkInterfaces();
    if (! sameConfig && ! artnetInput_.start (interfaceIndex, 6454, bind))
    {
        errorOut = "Failed to start ArtNet input";
        return false;
    }
    artnetInInterfaceIndex_ = interfaceIndex;
    artnetInBindIp_ = bind;
    errorOut.clear();
    return true;
}

void BridgeEngine::stopArtnetInput()
{
    artnetInput_.stop();
    artnetInInterfaceIndex_ = -1;
    artnetInBindIp_ = "0.0.0.0";
}

bool BridgeEngine::startOscInput (int port, const juce::String& bindIp, FrameRate fps, const juce::String& addrStr, const juce::String& addrFloat, juce::String& errorOut)
{
    const bool sameConfig =
        oscInput_.getIsRunning()
        && port == oscInPort_
        && bindIp == oscInBindIp_
        && fps == oscInFps_
        && addrStr == oscInAddrStr_
        && addrFloat == oscInAddrFloat_;
    if (sameConfig)
    {
        errorOut.clear();
        return true;
    }

    if (oscInput_.getIsRunning())
        oscInput_.stop();
    if (! oscInput_.start (port, bindIp, fps, addrStr, addrFloat, errorOut))
        return false;
    oscInPort_ = port;
    oscInBindIp_ = bindIp;
    oscInFps_ = fps;
    oscInAddrStr_ = addrStr;
    oscInAddrFloat_ = addrFloat;
    errorOut.clear();
    return true;
}

void BridgeEngine::stopOscInput()
{
    oscInput_.stop();
    oscInPort_ = 0;
    oscInBindIp_.clear();
    oscInFps_ = FrameRate::FPS_25;
    oscInAddrStr_.clear();
    oscInAddrFloat_.clear();
}

bool BridgeEngine::startLtcOutput (const AudioChoice& choice, int channel, double sampleRate, int bufferSize, juce::String& errorOut)
{
    const int ch = juce::jmax (-1, channel);

    // Guard: same LTC device+channel for IN and OUT is unstable on some drivers.
    if (ltcInput_.getIsRunning()
        && choice.typeName == ltcInType_
        && choice.deviceName == ltcInDevice_
        && ch >= 0
        && ch == ltcInChannel_)
    {
        errorOut = "LTC IN/OUT conflict: same device and channel";
        return false;
    }

    const bool sameConfig =
        ltcOutput_.getIsRunning()
        && choice.typeName == ltcOutType_
        && choice.deviceName == ltcOutDevice_
        && ch == ltcOutChannel_
        && std::abs (sampleRate - ltcOutSampleRate_) < 0.5
        && bufferSize == ltcOutBufferSize_;

    bool started = true;
    if (! sameConfig)
    {
        started = ltcOutput_.start (choice.typeName, choice.deviceName, ch, sampleRate, bufferSize);
        if (! started && (sampleRate > 0.0 || bufferSize > 0))
            started = ltcOutput_.start (choice.typeName, choice.deviceName, ch, 0.0, 0);
    }
    if (! started)
    {
        errorOut = "Failed to start LTC output";
        return false;
    }
    ltcOutType_ = choice.typeName;
    ltcOutDevice_ = choice.deviceName;
    ltcOutChannel_ = ch;
    ltcOutSampleRate_ = sampleRate;
    ltcOutBufferSize_ = bufferSize;
    ltcOutEnabled_ = true;
    // Keep stream open; actual signal is resumed instantly when input TC appears or output is enabled.
    ltcOutput_.setPaused (true);
    errorOut.clear();
    return true;
}

void BridgeEngine::stopLtcOutput()
{
    ltcOutEnabled_ = false;
    ltcOutType_.clear();
    ltcOutDevice_.clear();
    ltcOutChannel_ = 0;
    ltcOutSampleRate_ = 0.0;
    ltcOutBufferSize_ = 0;
    ltcOutput_.stop();
}

bool BridgeEngine::startMtcOutput (int deviceIndex, juce::String& errorOut)
{
    mtcOutput_.refreshDeviceList();
    const bool sameConfig = mtcOutput_.getIsRunning() && deviceIndex == mtcOutDeviceIndex_;
    if (! sameConfig && ! mtcOutput_.start (deviceIndex))
    {
        errorOut = "Failed to start MTC output";
        return false;
    }
    mtcOutDeviceIndex_ = deviceIndex;
    mtcOutEnabled_ = true;
    mtcOutput_.setPaused (true);
    errorOut.clear();
    return true;
}

void BridgeEngine::stopMtcOutput()
{
    mtcOutEnabled_ = false;
    mtcOutDeviceIndex_ = -1;
    mtcOutput_.stop();
}

bool BridgeEngine::startArtnetOutput (int interfaceIndex, juce::String& errorOut)
{
    artnetOutput_.refreshNetworkInterfaces();
    const bool sameConfig = artnetOutput_.getIsRunning() && interfaceIndex == artnetOutInterfaceIndex_;
    if (! sameConfig && ! artnetOutput_.start (interfaceIndex, 6454))
    {
        errorOut = "Failed to start ArtNet output";
        return false;
    }
    artnetOutInterfaceIndex_ = interfaceIndex;
    artnetOutEnabled_ = true;
    artnetOutput_.setPaused (true);
    errorOut.clear();
    return true;
}

void BridgeEngine::stopArtnetOutput()
{
    artnetOutEnabled_ = false;
    artnetOutInterfaceIndex_ = -1;
    artnetOutput_.stop();
}

void BridgeEngine::setLtcOutputEnabled (bool enabled)
{
    ltcOutEnabled_ = enabled;
    if (ltcOutput_.getIsRunning())
        ltcOutput_.setPaused (! enabled);
}

void BridgeEngine::setMtcOutputEnabled (bool enabled)
{
    mtcOutEnabled_ = enabled;
    if (mtcOutput_.getIsRunning())
        mtcOutput_.setPaused (! enabled);
}

void BridgeEngine::setArtnetOutputEnabled (bool enabled)
{
    artnetOutEnabled_ = enabled;
    if (artnetOutput_.getIsRunning())
        artnetOutput_.setPaused (! enabled);
}

void BridgeEngine::setLtcInputGain (float linearGain)
{
    ltcInput_.setInputGain (juce::jlimit (0.0f, 2.0f, linearGain));
}

void BridgeEngine::setLtcOutputGain (float linearGain)
{
    ltcOutput_.setOutputGain (juce::jlimit (0.0f, 2.0f, linearGain));
}

float BridgeEngine::getLtcInputPeakLevel() const
{
    return juce::jlimit (0.0f, 1.5f, ltcInput_.getLtcPeakLevel());
}

void BridgeEngine::setInputSource (InputSource source)
{
    source_ = source;
}

void BridgeEngine::setOffsets (int ltcFrames, int mtcFrames, int artnetFrames)
{
    ltcOffset_ = juce::jlimit (-30, 30, ltcFrames);
    mtcOffset_ = juce::jlimit (-30, 30, mtcFrames);
    artnetOffset_ = juce::jlimit (-30, 30, artnetFrames);
}

FrameRate BridgeEngine::sanitizeFps (FrameRate fps)
{
    switch (fps)
    {
        case FrameRate::FPS_2398:
        case FrameRate::FPS_24:
        case FrameRate::FPS_25:
        case FrameRate::FPS_2997:
        case FrameRate::FPS_30:
            return fps;
        default:
            return FrameRate::FPS_25;
    }
}

Timecode BridgeEngine::applyOffsetSafe (const Timecode& tc, int frames, FrameRate fps)
{
    return offsetTimecode (tc, juce::jlimit (-30, 30, frames), sanitizeFps (fps));
}

RuntimeStatus BridgeEngine::tick()
{
    RuntimeStatus st;
    st.ltcOutStatus = (! ltcOutEnabled_) ? "OFF" : (ltcOutput_.getIsRunning() ? (ltcOutput_.isPaused() ? "ARMED" : "ON") : "OFF");
    st.mtcOutStatus = (! mtcOutEnabled_) ? "OFF" : (mtcOutput_.getIsRunning() ? (mtcOutput_.isPaused() ? "ARMED" : "ON") : "OFF");
    st.artnetOutStatus = (! artnetOutEnabled_) ? "OFF" : (artnetOutput_.getIsRunning() ? (artnetOutput_.isPaused() ? "ARMED" : "ON") : "OFF");

    Timecode tc;
    FrameRate fps = FrameRate::FPS_25;
    bool hasInput = false;

    switch (source_)
    {
        case InputSource::LTC:
            hasInput = ltcInput_.getIsRunning() && ltcInput_.isReceiving();
            if (hasInput)
            {
                tc = ltcInput_.getCurrentTimecode();
                fps = sanitizeFps (ltcInput_.getDetectedFrameRate());
                st.inputStatus = "LTC LOCKED";
            }
            else
            {
                st.inputStatus = ltcInput_.getIsRunning() ? "LTC NO SIGNAL" : "LTC STOPPED";
            }
            break;
        case InputSource::MTC:
            hasInput = mtcInput_.getIsRunning() && mtcInput_.isReceiving();
            if (hasInput)
            {
                tc = mtcInput_.getCurrentTimecode();
                fps = sanitizeFps (mtcInput_.getDetectedFrameRate());
                st.inputStatus = "MTC LOCKED";
            }
            else
            {
                st.inputStatus = mtcInput_.getIsRunning() ? "MTC NO SIGNAL" : "MTC STOPPED";
            }
            break;
        case InputSource::ArtNet:
            hasInput = artnetInput_.getIsRunning() && artnetInput_.isReceiving();
            if (hasInput)
            {
                tc = artnetInput_.getCurrentTimecode();
                fps = sanitizeFps (artnetInput_.getDetectedFrameRate());
                st.inputStatus = "ARTNET LOCKED";
            }
            else
            {
                st.inputStatus = artnetInput_.getIsRunning() ? "ARTNET NO SIGNAL" : "ARTNET STOPPED";
            }
            break;
        case InputSource::OSC:
            hasInput = oscInput_.getIsRunning() && oscInput_.isReceiving();
            if (hasInput)
            {
                tc = oscInput_.getCurrentTimecode();
                fps = sanitizeFps (oscInput_.getDetectedFrameRate());
                st.inputStatus = "OSC LOCKED";
            }
            else
            {
                st.inputStatus = oscInput_.getIsRunning() ? "OSC NO SIGNAL" : "OSC STOPPED";
            }
            break;
    }

    st.hasInputTc = hasInput;
    st.inputTc = tc;
    st.inputFps = fps;
    st.errorText = lastError_;

    if (! hasInput)
    {
        // Fast reaction on TC loss: pause outputs without closing/reopening devices.
        if (ltcOutput_.getIsRunning())    ltcOutput_.setPaused (true);
        if (mtcOutput_.getIsRunning())    mtcOutput_.setPaused (true);
        if (artnetOutput_.getIsRunning()) artnetOutput_.setPaused (true);
        return st;
    }

    if (ltcOutEnabled_ && ltcOutput_.getIsRunning())
    {
        ltcOutput_.setPaused (false);
        ltcOutput_.setFrameRate (fps);
        ltcOutput_.setTimecode (applyOffsetSafe (tc, ltcOffset_, fps));
    }
    else if (ltcOutput_.getIsRunning())
    {
        ltcOutput_.setPaused (true);
    }
    if (mtcOutEnabled_ && mtcOutput_.getIsRunning())
    {
        mtcOutput_.setPaused (false);
        mtcOutput_.setFrameRate (fps);
        mtcOutput_.setTimecode (applyOffsetSafe (tc, mtcOffset_, fps));
    }
    else if (mtcOutput_.getIsRunning())
    {
        mtcOutput_.setPaused (true);
    }
    if (artnetOutEnabled_ && artnetOutput_.getIsRunning())
    {
        artnetOutput_.setPaused (false);
        artnetOutput_.setFrameRate (fps);
        artnetOutput_.setTimecode (applyOffsetSafe (tc, artnetOffset_, fps));
    }
    else if (artnetOutput_.getIsRunning())
    {
        artnetOutput_.setPaused (true);
    }

    return st;
}
} // namespace bridge::engine
