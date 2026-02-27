#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <optional>

namespace bridge::core
{
class ConfigStore
{
public:
    static std::optional<juce::var> loadJsonFile (const juce::File& file);
    static bool saveJsonFile (const juce::File& file, const juce::var& data);
};
} // namespace bridge::core

