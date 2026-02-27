#include "App.h"
#include "TriggerMainWindow.h"
#include "core/BridgeVersion.h"

namespace trigger
{
const juce::String TriggerApplication::getApplicationName()
{
    return "Easy Trigger";
}

const juce::String TriggerApplication::getApplicationVersion()
{
    return bridge::version::kAppVersion;
}

void TriggerApplication::initialise (const juce::String&)
{
    mainWindow_ = std::make_unique<MainWindow>();
}

void TriggerApplication::shutdown()
{
    mainWindow_.reset();
}

void TriggerApplication::systemRequestedQuit()
{
    quit();
}

void TriggerApplication::anotherInstanceStarted (const juce::String&)
{
}
} // namespace trigger
