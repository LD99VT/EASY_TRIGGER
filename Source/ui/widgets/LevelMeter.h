#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trigger
{
class LevelMeter final : public juce::Component
{
public:
    LevelMeter() { setOpaque (false); }
    void setMeterColour (juce::Colour c) { meterColour_ = c; }
    void setLevel (float l)
    {
        l = juce::jlimit (0.0f, 2.0f, l);
        if (std::abs (level_ - l) > 0.001f)
        {
            level_ = l;
            repaint();
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto intBounds = getLocalBounds();
        constexpr float corner = 2.0f;

        g.setColour (juce::Colour::fromRGB (0x2a, 0x2a, 0x2a));
        g.fillRoundedRectangle (intBounds.toFloat(), corner);

        auto bounds = intBounds.toFloat().reduced (1.0f);
        if (level_ > 0.001f)
        {
            const float display = juce::jmin (1.0f, level_);
            const auto fill = bounds.withWidth (bounds.getWidth() * display);

            juce::Colour bar;
            if (level_ < 0.6f)       bar = meterColour_.withAlpha (0.7f);
            else if (level_ < 0.85f) bar = juce::Colour (0xFFFFAB00).withAlpha (0.8f);
            else                     bar = juce::Colour (0xFFC62828).withAlpha (0.9f);

            g.setColour (bar);
            g.fillRoundedRectangle (fill, corner);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (fill.withHeight (fill.getHeight() * 0.4f), corner);

            if (level_ > 1.0f)
            {
                g.setColour (juce::Colour (0xFFC62828).withAlpha (0.3f));
                g.fillRoundedRectangle (bounds, corner);
            }
        }

        g.setColour (juce::Colour (0xFF2A2D35));
        g.drawRoundedRectangle (bounds, corner, 0.5f);
        g.setColour (juce::Colour (0xFF2A2D35).withAlpha (0.6f));
        for (auto tp : { 0.25f, 0.5f, 0.75f })
        {
            const float x = bounds.getX() + bounds.getWidth() * tp;
            g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 0.5f);
        }
    }
private:
    float level_ { 0.0f };
    juce::Colour meterColour_ { juce::Colour (0xFF3D8070) };
};
} // namespace trigger
