#include "ResolumeClipCollector.h"

#include <regex>
#include <atomic>

namespace trigger::engine
{
namespace
{
const std::regex kClipParamRe (R"(^/composition/layers/(\d+)/clips/(\d+)/transport/position/behaviour/(offset|duration)$)");
const std::regex kClipConnectRe (R"(^/composition/layers/(\d+)/clips/(\d+)/(?:connect|connected|transport/position/behaviour/connect|transport/position/behaviour/connected)$)");
const std::regex kClipNameRe (R"(^/composition/layers/(\d+)/clips/(\d+)/name$)");
const std::regex kLayerNameRe (R"(^/composition/layers/(\d+)/name$)");

double parseNumericArg (const juce::OSCMessage& msg)
{
    if (msg.size() <= 0)
        return 0.0;
    if (msg[0].isFloat32()) return (double) msg[0].getFloat32();
    if (msg[0].isInt32())   return (double) msg[0].getInt32();
    if (msg[0].isString())  return msg[0].getString().getDoubleValue();
    return 0.0;
}

double resolumeOffsetToSeconds (double raw)
{
    return raw / 1000.0;
}

double resolumeDurationToSeconds (double raw)
{
    const auto v = juce::jmax (0.0, raw);
    if (v >= 0.0 && v <= 1.0)
        return v * 604800.0;
    if (v > 604800.0)
        return v / 1000.0;
    return v;
}

void logResolume (const juce::String& line)
{
    static juce::CriticalSection cs;
    const juce::ScopedLock sl (cs);
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto file = exeDir.getChildFile ("easytrigger_resolume.log");
    const auto text = juce::Time::getCurrentTime().toString (true, true) + " | " + line + "\n";
    file.appendText (text, false, false, "\n");
}
}

ResolumeClipCollector::ResolumeClipCollector()
{
    addListener (this);
}

ResolumeClipCollector::~ResolumeClipCollector()
{
    stopListening();
    removeListener (this);
}

bool ResolumeClipCollector::startListening (const juce::String& listenIp, int listenPort, juce::String& errorOut)
{
    juce::ignoreUnused (listenIp);
    stopListening();
    logResolume ("LISTEN start ip=" + listenIp + " port=" + juce::String (listenPort));
    if (! connect (listenPort))
    {
        errorOut = "Resolume listen failed";
        logResolume ("LISTEN FAILED");
        return false;
    }
    logResolume ("LISTEN OK");
    return true;
}

void ResolumeClipCollector::stopListening()
{
    disconnect();
}

bool ResolumeClipCollector::configureSender (const juce::String& sendIp, int sendPort, juce::String& errorOut)
{
    sender_.disconnect();
    logResolume ("SEND connect ip=" + sendIp + " port=" + juce::String (sendPort));
    senderReady_ = sender_.connect (sendIp, sendPort);
    if (! senderReady_)
    {
        errorOut = "Resolume send failed";
        logResolume ("SEND CONNECT FAILED");
        return false;
    }
    logResolume ("SEND CONNECT OK");
    return true;
}

void ResolumeClipCollector::queryClips (int maxLayers, int maxClips)
{
    if (! senderReady_)
    {
        logResolume ("QUERY skipped: sender not ready");
        return;
    }

    auto sendQuery = [this] (const juce::String& addr)
    {
        // Compatibility: some Resolume setups reply to "?" query argument, some to empty messages.
        juce::OSCMessage q (addr);
        q.addString ("?");
        juce::OSCMessage plain (addr);
        const bool okQ = sender_.send (q);
        const bool okPlain = sender_.send (plain);
        logResolume ("OUT " + addr + " q=" + juce::String (okQ ? "1" : "0") + " plain=" + juce::String (okPlain ? "1" : "0"));
    };

    for (int layer = 1; layer <= juce::jlimit (1, 64, maxLayers); ++layer)
    {
        sendQuery ("/composition/layers/" + juce::String (layer) + "/name");
        for (int clip = 1; clip <= juce::jlimit (1, 256, maxClips); ++clip)
        {
            const juce::String base = "/composition/layers/" + juce::String (layer) + "/clips/" + juce::String (clip);
            sendQuery (base + "/name");
            sendQuery (base + "/connect");
            sendQuery (base + "/connected");
            sendQuery (base + "/transport/position/behaviour/offset");
            sendQuery (base + "/transport/position/behaviour/duration");
        }
    }
}

std::vector<ClipTriggerInfo> ResolumeClipCollector::snapshot() const
{
    const juce::ScopedLock sl (lock_);
    std::vector<ClipTriggerInfo> out;
    out.reserve (clips_.size());
    for (const auto& it : clips_)
    {
        ClipTriggerInfo info;
        info.layer = it.first.first;
        info.clip = it.first.second;
        info.layerName = it.second.layerName;
        info.clipName = it.second.clipName;
        info.offsetSeconds = it.second.offset;
        info.durationSeconds = it.second.duration;
        info.hasOffset = it.second.hasOffset;
        info.connected = it.second.connected;
        out.push_back (info);
    }
    return out;
}

void ResolumeClipCollector::clear()
{
    const juce::ScopedLock sl (lock_);
    clips_.clear();
    layerNames_.clear();
}

void ResolumeClipCollector::oscMessageReceived (const juce::OSCMessage& msg)
{
    handleMessage (msg);
}

void ResolumeClipCollector::oscBundleReceived (const juce::OSCBundle& bundle)
{
    logResolume ("IN BUNDLE size=" + juce::String ((int) std::distance (bundle.begin(), bundle.end())));
    for (const auto& element : bundle)
    {
        if (element.isMessage())
        {
            handleMessage (element.getMessage());
        }
        else if (element.isBundle())
        {
            oscBundleReceived (element.getBundle());
        }
    }
}

void ResolumeClipCollector::handleMessage (const juce::OSCMessage& msg)
{
    static std::atomic<int> sInCount { 0 };
    std::smatch m;
    const auto addr = msg.getAddressPattern().toString().toStdString();
    const int in = ++sInCount;
    if (in <= 300)
        logResolume ("IN " + juce::String (msg.getAddressPattern().toString()));

    if (std::regex_match (addr, m, kLayerNameRe) && m.size() >= 2)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const auto name = (msg.size() > 0 && msg[0].isString()) ? juce::String (msg[0].getString()) : juce::String();
        const juce::ScopedLock sl (lock_);
        layerNames_[layer] = name;
        for (auto& [key, raw] : clips_)
            if (key.first == layer)
                raw.layerName = name;
        if (onChanged) onChanged();
        return;
    }

    if (std::regex_match (addr, m, kClipNameRe) && m.size() >= 3)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const int clip = juce::String (m[2].str()).getIntValue();
        const auto name = (msg.size() > 0 && msg[0].isString()) ? juce::String (msg[0].getString()) : juce::String();
        {
            const juce::ScopedLock sl (lock_);
            touchClip (layer, clip);
            clips_[{ layer, clip }].clipName = name;
        }
        if (onChanged) onChanged();
        return;
    }

    if (std::regex_match (addr, m, kClipConnectRe) && m.size() >= 3)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const int clip = juce::String (m[2].str()).getIntValue();
        const auto value = parseNumericArg (msg);
        const bool isConnectedEndpoint = (addr.find ("/connected") != std::string::npos);
        if (isConnectedEndpoint)
        {
            const bool connected = ((int) std::round (value) == 3);
            {
                const juce::ScopedLock sl (lock_);
                touchClip (layer, clip);
                clips_[{ layer, clip }].connected = connected;
            }
            if (onChanged) onChanged();
        }
        return;
    }

    if (std::regex_match (addr, m, kClipParamRe) && m.size() >= 4)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const int clip = juce::String (m[2].str()).getIntValue();
        const juce::String kind (m[3].str());
        const double value = parseNumericArg (msg);

        {
            const juce::ScopedLock sl (lock_);
            touchClip (layer, clip);
            if (kind == "offset")
            {
                clips_[{ layer, clip }].offset = resolumeOffsetToSeconds (value);
                clips_[{ layer, clip }].hasOffset = true;
            }
            else
                clips_[{ layer, clip }].duration = resolumeDurationToSeconds (value);
        }
        if (onChanged) onChanged();
        return;
    }

    if (in <= 300)
        logResolume ("IN unhandled");
}

void ResolumeClipCollector::touchClip (int layer, int clip)
{
    if (layer < 1 || clip < 1)
        return;
    auto key = std::make_pair (layer, clip);
    if (clips_.find (key) == clips_.end())
    {
        RawClip c;
        auto it = layerNames_.find (layer);
        if (it != layerNames_.end())
            c.layerName = it->second;
        clips_[key] = c;
    }
}
} // namespace trigger::engine
