#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
        g.setColour (juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
        g.drawEllipse (b, 1.4f);
        g.setColour (juce::Colour::fromRGB (0x24, 0x24, 0x24));
        g.fillEllipse (b.reduced (1.6f));
        if (state_)
        {
            g.setColour (juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
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
