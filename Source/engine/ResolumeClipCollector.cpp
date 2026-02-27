#include "ResolumeClipCollector.h"

#include <regex>

namespace trigger::engine
{
namespace
{
const std::regex kClipParamRe (R"(^/composition/layers/(\d+)/clips/(\d+)/transport/position/behaviour/(offset|duration)$)");
const std::regex kClipConnectRe (R"(^/composition/layers/(\d+)/clips/(\d+)/(?:connect|connected)$)");
const std::regex kClipNameRe (R"(^/composition/layers/(\d+)/clips/(\d+)/name$)");
const std::regex kLayerNameRe (R"(^/composition/layers/(\d+)/name$)");
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
    if (! connect (listenPort))
    {
        errorOut = "Resolume listen failed";
        return false;
    }
    return true;
}

void ResolumeClipCollector::stopListening()
{
    disconnect();
}

bool ResolumeClipCollector::configureSender (const juce::String& sendIp, int sendPort, juce::String& errorOut)
{
    sender_.disconnect();
    senderReady_ = sender_.connect (sendIp, sendPort);
    if (! senderReady_)
    {
        errorOut = "Resolume send failed";
        return false;
    }
    return true;
}

void ResolumeClipCollector::queryClips (int maxLayers, int maxClips)
{
    if (! senderReady_)
        return;

    auto sendQuery = [this] (const juce::String& addr)
    {
        juce::OSCMessage m (addr);
        m.addString ("?");
        sender_.send (m);
    };

    for (int layer = 1; layer <= juce::jlimit (1, 64, maxLayers); ++layer)
    {
        sendQuery ("/composition/layers/" + juce::String (layer) + "/name");
        for (int clip = 1; clip <= juce::jlimit (1, 256, maxClips); ++clip)
        {
            const juce::String base = "/composition/layers/" + juce::String (layer) + "/clips/" + juce::String (clip);
            sendQuery (base + "/name");
            sendQuery (base + "/connect");
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
        info.connected = it.second.connected;
        out.push_back (info);
    }
    return out;
}

void ResolumeClipCollector::clear()
{
    const juce::ScopedLock sl (lock_);
    clips_.clear();
}

void ResolumeClipCollector::oscMessageReceived (const juce::OSCMessage& msg)
{
    std::smatch m;
    const auto addr = msg.getAddressPattern().toString().toStdString();

    if (std::regex_match (addr, m, kLayerNameRe) && m.size() >= 2)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const auto name = (msg.size() > 0 && msg[0].isString()) ? juce::String (msg[0].getString()) : juce::String();
        const juce::ScopedLock sl (lock_);
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
        bool connected = false;
        if (msg.size() > 0)
        {
            if (msg[0].isInt32()) connected = msg[0].getInt32() != 0;
            else if (msg[0].isFloat32()) connected = msg[0].getFloat32() > 0.5f;
        }
        {
            const juce::ScopedLock sl (lock_);
            touchClip (layer, clip);
            clips_[{ layer, clip }].connected = connected;
        }
        if (onChanged) onChanged();
        return;
    }

    if (std::regex_match (addr, m, kClipParamRe) && m.size() >= 4)
    {
        const int layer = juce::String (m[1].str()).getIntValue();
        const int clip = juce::String (m[2].str()).getIntValue();
        const juce::String kind (m[3].str());
        if (msg.size() <= 0)
            return;

        double value = 0.0;
        if (msg[0].isFloat32()) value = (double) msg[0].getFloat32();
        else if (msg[0].isInt32()) value = (double) msg[0].getInt32();
        else return;

        {
            const juce::ScopedLock sl (lock_);
            touchClip (layer, clip);
            if (kind == "offset")
                clips_[{ layer, clip }].offset = value / 1000.0;
            else
                clips_[{ layer, clip }].duration = value / 1000.0;
        }
        if (onChanged) onChanged();
    }
}

void ResolumeClipCollector::touchClip (int layer, int clip)
{
    if (layer < 1 || clip < 1)
        return;
    auto key = std::make_pair (layer, clip);
    if (clips_.find (key) == clips_.end())
        clips_[key] = RawClip{};
}
} // namespace trigger::engine
