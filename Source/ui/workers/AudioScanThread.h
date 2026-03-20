// Included from TriggerMainWindow.cpp, outside the anonymous namespace.
// Required context: TriggerContentComponent forward-declared, AudioDeviceEntry,
//                   bridge::engine::AudioChoice, onAudioScanComplete().
#pragma once

class TriggerContentComponent::AudioScanThread final : public juce::Thread
{
public:
    explicit AudioScanThread (TriggerContentComponent* owner)
        : juce::Thread ("TriggerAudioScan"),
          safeOwner_ (owner)
    {}

    void run() override
    {
        juce::Array<bridge::engine::AudioChoice> inputs, outputs;
        if (tempManager_ == nullptr)
            return;

        for (auto* type : tempManager_->getAvailableDeviceTypes())
        {
            if (threadShouldExit() || type == nullptr)
                return;

            const auto typeName = type->getTypeName();
            type->scanForDevices();

            for (const auto& name : type->getDeviceNames (true))
            {
                bridge::engine::AudioChoice choice;
                choice.typeName = typeName;
                choice.deviceName = name;
                choice.displayName = AudioDeviceEntry::makeDisplayName (typeName, name);
                inputs.add (choice);
            }

            for (const auto& name : type->getDeviceNames (false))
            {
                bridge::engine::AudioChoice choice;
                choice.typeName = typeName;
                choice.deviceName = name;
                choice.displayName = AudioDeviceEntry::makeDisplayName (typeName, name);
                outputs.add (choice);
            }
        }

        if (threadShouldExit())
            return;

        auto safe = safeOwner_;
        juce::MessageManager::callAsync ([safe, inputs, outputs]()
        {
            if (auto* owner = safe.getComponent())
                owner->onAudioScanComplete (inputs, outputs);
        });
    }

    std::unique_ptr<juce::AudioDeviceManager> tempManager_;

private:
    juce::Component::SafePointer<TriggerContentComponent> safeOwner_;
};
