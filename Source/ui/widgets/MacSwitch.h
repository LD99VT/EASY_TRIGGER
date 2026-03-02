#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trigger
{
class MacSwitch final : public juce::Component
{
public:
    std::function<void(bool)> onToggle;
    void setState (bool s) { state_ = s; repaint(); }
    bool getState() const { return state_; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (state_ ? juce::Colour::fromRGB (0x4c, 0xbd, 0x54) : juce::Colour::fromRGB (0x3f, 0x3f, 0x3f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (state_ ? juce::Colour::fromRGB (0x56, 0xc8, 0x5f) : juce::Colour::fromRGB (0x4b, 0x4b, 0x4b));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
        const float d = b.getHeight() - 4.0f;
        const float x = state_ ? (b.getRight() - d - 2.0f) : (b.getX() + 2.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x, b.getY() + 2.0f, d, d);
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
