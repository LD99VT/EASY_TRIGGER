#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

namespace trigger
{
struct OscLogEntry
{
    enum class Dir { input, output };
    Dir           dir { Dir::input };
    juce::int64   timestampMs { 0 };
    juce::String  ip;
    int           port { 0 };
    juce::String  address;
    juce::String  type;
    juce::String  value;
};

class OscLog
{
public:
    static constexpr int kMaxEntries = 300;

    void push (OscLogEntry e)
    {
        const juce::ScopedLock sl (lock_);
        entries_.push_back (std::move (e));
        if ((int) entries_.size() > kMaxEntries)
            entries_.erase (entries_.begin());
    }

    std::vector<OscLogEntry> snapshot() const
    {
        const juce::ScopedLock sl (lock_);
        return entries_;
    }

    void clear()
    {
        const juce::ScopedLock sl (lock_);
        entries_.clear();
    }

private:
    std::vector<OscLogEntry> entries_;
    mutable juce::CriticalSection lock_;
};
} // namespace trigger
