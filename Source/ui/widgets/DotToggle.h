#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../style/TriggerColours.h"

namespace trigger
{
class DotToggle final : public juce::Component
{
public:
    std::function<void(bool)> onToggle;
    void setState (bool s) { state_ = s; repaint(); }
    bool getState() const { return state_; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kArrowCollapsed);
        g.drawEllipse (b, 1.4f);
        g.setColour (kBg);
        g.fillEllipse (b.reduced (1.6f));
        if (state_)
        {
            g.setColour (kTextSecondary);
            g.fillEllipse (b.reduced (4.8f));
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        state_ = ! state_;
        repaint();
        if (onToggle) onToggle (state_);
    }
private:
    bool state_ { false };
};
} // namespace trigger
