#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "MainWindow.h"
#include <memory>

namespace bridge
{
class BridgeApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted (const juce::String& commandLine) override;

private:
    std::unique_ptr<MainWindow> mainWindow_;
};
} // namespace bridge
