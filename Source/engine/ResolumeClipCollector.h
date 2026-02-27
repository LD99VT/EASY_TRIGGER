#pragma once

#include <juce_osc/juce_osc.h>
#include <juce_core/juce_core.h>
#include <map>
#include <unordered_map>

namespace trigger::engine
{
struct ClipTriggerInfo
{
    int layer { 0 };
    int clip { 0 };
    juce::String layerName;
    juce::String clipName;
    double offsetSeconds { 0.0 };
    double durationSeconds { 0.0 };
    bool hasOffset { false };
    bool connected { false };
};

class ResolumeClipCollector final : private juce::OSCReceiver,
                                    private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    ResolumeClipCollector();
    ~ResolumeClipCollector() override;

    bool startListening (const juce::String& listenIp, int listenPort, juce::String& errorOut);
    void stopListening();

    bool configureSender (const juce::String& sendIp, int sendPort, juce::String& errorOut);
    void queryClips (int maxLayers, int maxClips);

    std::vector<ClipTriggerInfo> snapshot() const;
    void clear();

    std::function<void()> onChanged;

private:
    void oscMessageReceived (const juce::OSCMessage& msg) override;
    void oscBundleReceived (const juce::OSCBundle& bundle) override;
    void handleMessage (const juce::OSCMessage& msg);
    void touchClip (int layer, int clip);

    juce::OSCSender sender_;
    bool senderReady_ { false };

    struct RawClip
    {
        juce::String layerName;
        juce::String clipName;
        double offset { 0.0 };
        double duration { 0.0 };
        bool hasOffset { false };
        bool connected { false };
    };
    std::map<std::pair<int, int>, RawClip> clips_;
    std::unordered_map<int, juce::String> layerNames_;
    mutable juce::CriticalSection lock_;
};
} // namespace trigger::engine
