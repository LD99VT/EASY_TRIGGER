#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <atomic>

namespace trigger
{
class UpdateChecker : private juce::Thread
{
public:
    explicit UpdateChecker (juce::String latestReleaseApiUrl)
        : juce::Thread ("EasyTriggerUpdateChecker"),
          latestReleaseApiUrl_ (std::move (latestReleaseApiUrl))
    {
    }

    ~UpdateChecker() override
    {
        stopThread (10000);
    }

    void checkAsync (const juce::String& currentVersion)
    {
        if (isThreadRunning())
            return;

        currentVer_ = currentVersion;
        resultReady_.store (false, std::memory_order_relaxed);
        updateAvailable_.store (false, std::memory_order_relaxed);
        checkFailed_.store (false, std::memory_order_relaxed);
        latestVer_ = {};
        releaseUrl_ = {};
        preferredAssetUrl_ = {};
        releaseNotes_ = {};

        startThread (juce::Thread::Priority::low);
    }

    bool hasResult() const { return resultReady_.load (std::memory_order_acquire); }
    bool isUpdateAvailable() const { return updateAvailable_.load (std::memory_order_relaxed); }
    bool didCheckFail() const { return checkFailed_.load (std::memory_order_relaxed); }

    juce::String getLatestVersion() const { jassert (hasResult()); return latestVer_; }
    juce::String getReleaseUrl() const { jassert (hasResult()); return releaseUrl_; }
    juce::String getPreferredAssetUrl() const { jassert (hasResult()); return preferredAssetUrl_; }
    juce::String getReleaseNotes() const { jassert (hasResult()); return releaseNotes_; }

private:
    void run() override
    {
        juce::URL url (latestReleaseApiUrl_);

        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (8000)
                           .withExtraHeaders ("Accept: application/vnd.github+json\r\n"
                                              "User-Agent: EasyTrigger/" + currentVer_ + "\r\n");

        std::unique_ptr<juce::InputStream> stream = url.createInputStream (options);
        if (threadShouldExit())
            return;

        if (! stream)
        {
            checkFailed_.store (true, std::memory_order_relaxed);
            resultReady_.store (true, std::memory_order_release);
            return;
        }

        const auto response = stream->readEntireStreamAsString();
        stream.reset();

        if (threadShouldExit())
            return;

        auto json = juce::JSON::parse (response);
        if (! json.isObject())
        {
            checkFailed_.store (true, std::memory_order_relaxed);
            resultReady_.store (true, std::memory_order_release);
            return;
        }

        const auto tagName = json.getProperty ("tag_name", {}).toString();
        const auto htmlUrl = json.getProperty ("html_url", {}).toString();
        const auto body = json.getProperty ("body", {}).toString();

        if (tagName.isEmpty())
        {
            checkFailed_.store (true, std::memory_order_relaxed);
            resultReady_.store (true, std::memory_order_release);
            return;
        }

        const auto remoteVersion = tagName.trimCharactersAtStart ("vV");
        latestVer_ = remoteVersion;
        releaseUrl_ = htmlUrl.isNotEmpty() ? htmlUrl : "https://github.com/LD99VT/EASY_TRIGGER/releases/latest";
        preferredAssetUrl_ = choosePreferredAssetUrl (json);
        releaseNotes_ = body;

        updateAvailable_.store (isNewer (remoteVersion, currentVer_), std::memory_order_relaxed);
        checkFailed_.store (false, std::memory_order_relaxed);
        resultReady_.store (true, std::memory_order_release);
    }

    static bool isNewer (const juce::String& remote, const juce::String& local)
    {
        auto splitVersion = [] (const juce::String& v)
        {
            juce::Array<int> parts;
            juce::StringArray tokens;
            tokens.addTokens (v, ".", "");
            for (auto& t : tokens)
                parts.add (t.getIntValue());
            return parts;
        };

        const auto r = splitVersion (remote);
        const auto l = splitVersion (local);
        const int count = juce::jmax (r.size(), l.size());

        for (int i = 0; i < count; ++i)
        {
            const int rv = i < r.size() ? r[i] : 0;
            const int lv = i < l.size() ? l[i] : 0;
            if (rv > lv) return true;
            if (rv < lv) return false;
        }
        return false;
    }

    static juce::String choosePreferredAssetUrl (const juce::var& releaseJson)
    {
        const auto* object = releaseJson.getDynamicObject();
        if (object == nullptr)
            return {};

        const auto assets = object->getProperty ("assets");
        if (! assets.isArray())
            return {};

#if JUCE_WINDOWS
        constexpr auto preferredSuffix = ".exe";
#elif JUCE_MAC
        constexpr auto preferredSuffix = ".dmg";
#else
        constexpr auto preferredSuffix = "";
#endif

        juce::String fallbackUrl;
        for (const auto& asset : *assets.getArray())
        {
            const auto* assetObject = asset.getDynamicObject();
            if (assetObject == nullptr)
                continue;

            const auto name = assetObject->getProperty ("name").toString();
            const auto url = assetObject->getProperty ("browser_download_url").toString();
            if (url.isEmpty())
                continue;

            if (fallbackUrl.isEmpty())
                fallbackUrl = url;

            if (juce::String (preferredSuffix).isNotEmpty()
                && name.endsWithIgnoreCase (preferredSuffix))
                return url;
        }

        return fallbackUrl;
    }

    juce::String latestReleaseApiUrl_;
    juce::String currentVer_;
    std::atomic<bool> resultReady_ { false };
    std::atomic<bool> updateAvailable_ { false };
    std::atomic<bool> checkFailed_ { false };
    juce::String latestVer_;
    juce::String releaseUrl_;
    juce::String preferredAssetUrl_;
    juce::String releaseNotes_;
};
} // namespace trigger
