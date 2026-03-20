#include "App.h"
#include "TriggerMainWindow.h"
#include "Version.h"

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
    if (mainWindow_ != nullptr)
    {
        mainWindow_->prepareForShutdown();
        mainWindow_.reset();
    }

    quit();
}

void TriggerApplication::anotherInstanceStarted (const juce::String&)
{
}
} // namespace trigger
