#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trigger
{
class ExpandCircleButton final : public juce::Component
{
public:
    std::function<void()> onClick;
    void setExpanded (bool expanded) { expanded_ = expanded; repaint(); }
    bool isExpanded() const { return expanded_; }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (0x48, 0x48, 0x48));
        g.fillEllipse (b);
        g.setColour (juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
        g.drawEllipse (b, 1.0f);

        juce::Path p;
        const auto cx = b.getCentreX();
        const auto cy = b.getCentreY();
        const float s = 5.6f;
        if (expanded_)
        {
            p.startNewSubPath (cx - s, cy - 2.0f);
            p.lineTo (cx + s, cy - 2.0f);
            p.lineTo (cx, cy + s);
        }
        else
        {
            p.startNewSubPath (cx - 2.0f, cy - s);
            p.lineTo (cx - 2.0f, cy + s);
            p.lineTo (cx + s, cy);
        }
        p.closeSubPath();
        g.setColour (juce::Colour::fromRGB (0xf2, 0xf2, 0xf2));
        g.fillPath (p);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
private:
    bool expanded_ { true };
};

class HelpCircleButton final : public juce::Component
{
public:
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
        g.fillEllipse (b);
        g.setColour (juce::Colour::fromRGB (0x66, 0x66, 0x66));
        g.drawEllipse (b, 1.5f);
        g.setColour (juce::Colour::fromRGB (0x99, 0x99, 0x99));
        auto f = juce::FontOptions (13.0f).withStyle ("Bold");
        g.setFont (juce::Font (f));
        g.drawFittedText ("?", getLocalBounds().translated (0, -1), juce::Justification::centred, 1);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
};
} // namespace trigger
