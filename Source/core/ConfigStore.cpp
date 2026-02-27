#include "ConfigStore.h"

namespace bridge::core
{
std::optional<juce::var> ConfigStore::loadJsonFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    const auto content = file.loadFileAsString();
    auto parsed = juce::JSON::parse (content);
    if (parsed.isVoid())
        return std::nullopt;
    return parsed;
}

bool ConfigStore::saveJsonFile (const juce::File& file, const juce::var& data)
{
    const auto parent = file.getParentDirectory();
    if (! parent.exists() && ! parent.createDirectory())
        return false;

    const auto json = juce::JSON::toString (data, true);
    return file.replaceWithText (json);
}
} // namespace bridge::core

