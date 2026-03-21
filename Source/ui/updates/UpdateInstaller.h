#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>

namespace trigger
{
class UpdateInstaller : private juce::Thread
{
public:
    UpdateInstaller()
        : juce::Thread ("EasyTriggerUpdateInstaller")
    {
    }

    ~UpdateInstaller() override
    {
        stopThread (10000);
    }

    bool isBusy() const
    {
        return isThreadRunning();
    }

    void downloadAsync (juce::String version,
                        juce::String assetUrl,
                        std::function<void(const juce::String&)> onError,
                        std::function<void(const juce::File&)> onDownloaded)
    {
        if (isThreadRunning())
            return;

        version_ = std::move (version);
        assetUrl_ = std::move (assetUrl);
        onError_ = std::move (onError);
        onDownloaded_ = std::move (onDownloaded);
        startThread (juce::Thread::Priority::normal);
    }

private:
    void run() override
    {
        if (assetUrl_.isEmpty())
        {
            postError ("Update asset URL is missing.");
            return;
        }

        auto targetFile = buildTargetFile();
        targetFile.deleteFile();

        juce::URL url (assetUrl_);
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (15000)
                           .withExtraHeaders ("Accept: application/octet-stream\r\n"
                                              "User-Agent: EasyTrigger/" + version_ + "\r\n");

        std::unique_ptr<juce::InputStream> in = url.createInputStream (options);
        if (threadShouldExit())
            return;

        if (! in)
        {
            postError ("Could not download the update package.");
            return;
        }

        juce::FileOutputStream out (targetFile);
        if (! out.openedOk())
        {
            postError ("Could not write the update package to disk.");
            return;
        }

        juce::HeapBlock<char> buffer (64 * 1024);
        for (;;)
        {
            if (threadShouldExit())
                return;

            const auto bytesRead = in->read (buffer.getData(), 64 * 1024);
            if (bytesRead <= 0)
                break;

            if (! out.write (buffer.getData(), (size_t) bytesRead))
            {
                postError ("Could not save the downloaded update package.");
                return;
            }
        }

        out.flush();
        if (! targetFile.existsAsFile() || targetFile.getSize() <= 0)
        {
            postError ("Downloaded update package is empty.");
            return;
        }

        juce::MessageManager::callAsync ([callback = onDownloaded_, targetFile]
        {
            if (callback != nullptr)
                callback (targetFile);
        });
    }

    juce::File buildTargetFile() const
    {
        auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("EasyTriggerUpdate");
        tempDir.createDirectory();

        juce::String extension = ".bin";
        if (assetUrl_.containsIgnoreCase (".exe")) extension = ".exe";
        else if (assetUrl_.containsIgnoreCase (".dmg")) extension = ".dmg";
        else if (assetUrl_.containsIgnoreCase (".zip")) extension = ".zip";

        return tempDir.getChildFile ("EasyTrigger_Update_" + juce::File::createLegalFileName (version_) + extension);
    }

    void postError (const juce::String& message)
    {
        juce::MessageManager::callAsync ([callback = onError_, message]
        {
            if (callback != nullptr)
                callback (message);
        });
    }

    juce::String version_;
    juce::String assetUrl_;
    std::function<void(const juce::String&)> onError_;
    std::function<void(const juce::File&)> onDownloaded_;
};
} // namespace trigger
