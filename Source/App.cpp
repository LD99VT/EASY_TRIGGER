#include "App.h"
#include "MainWindow.h"
#include "core/BridgeVersion.h"

namespace bridge
{
const juce::String BridgeApplication::getApplicationName()
{
    return "Easy Bridge";
}

const juce::String BridgeApplication::getApplicationVersion()
{
    return version::kAppVersion;
}

void BridgeApplication::initialise (const juce::String&)
{
    mainWindow_ = std::make_unique<MainWindow>();
}

void BridgeApplication::shutdown()
{
    mainWindow_.reset();
}

void BridgeApplication::systemRequestedQuit()
{
    quit();
}

void BridgeApplication::anotherInstanceStarted (const juce::String&)
{
}
} // namespace bridge
