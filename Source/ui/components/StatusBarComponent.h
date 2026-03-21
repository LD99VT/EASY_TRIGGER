#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "../style/TriggerColours.h"

namespace trigger
{
class StatusBarComponent final : public juce::Component
{
public:
    StatusBarComponent()
    {
        setOpaque (true);

        for (auto* label : { &leftLabel_, &centerLabel_, &rightLabel_ })
        {
            label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            label->setColour (juce::Label::textColourId, kStatusWarn);
            label->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*label);
        }

        leftLabel_.setJustificationType (juce::Justification::centredLeft);
        centerLabel_.setJustificationType (juce::Justification::centred);
        rightLabel_.setJustificationType (juce::Justification::centredRight);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kRow);
        g.setColour (kRowOutline);
        g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        leftRect_ = bounds;
        centerRect_ = {};
        rightRect_ = {};

        if (! bounds.isEmpty())
        {
            const int rightW = juce::jmax (220, bounds.getWidth() / 4);
            const int centerW = juce::jmax (320, bounds.getWidth() / 3);
            const int centerX = bounds.getCentreX() - centerW / 2;

            centerRect_ = juce::Rectangle<int> (centerX,
                                                bounds.getY(),
                                                juce::jmin (centerW, bounds.getWidth()),
                                                bounds.getHeight());

            rightRect_ = juce::Rectangle<int> (bounds.getRight() - rightW,
                                               bounds.getY(),
                                               rightW,
                                               bounds.getHeight());

            leftRect_ = juce::Rectangle<int> (bounds.getX(),
                                              bounds.getY(),
                                              juce::jmax (0, centerRect_.getX() - bounds.getX()),
                                              bounds.getHeight());
        }

        leftLabel_.setBounds (leftRect_.reduced (8, 0));
        centerLabel_.setBounds (centerRect_.reduced (8, 0));
        rightLabel_.setBounds (rightRect_.reduced (8, 0));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onClick != nullptr)
            onClick();
    }

    void setLeftStatus (const juce::String& text, juce::Colour colour)
    {
        leftLabel_.setText (text, juce::dontSendNotification);
        leftLabel_.setColour (juce::Label::textColourId, colour);
    }

    void setCenterStatus (const juce::String& text, juce::Colour colour)
    {
        centerLabel_.setText (text, juce::dontSendNotification);
        centerLabel_.setColour (juce::Label::textColourId, colour);
    }

    void setRightStatus (const juce::String& text, juce::Colour colour)
    {
        rightLabel_.setText (text, juce::dontSendNotification);
        rightLabel_.setColour (juce::Label::textColourId, colour);
    }

    juce::String getLeftText() const   { return leftLabel_.getText(); }
    juce::String getCenterText() const { return centerLabel_.getText(); }
    juce::String getRightText() const  { return rightLabel_.getText(); }

    std::function<void()> onClick;

private:
    juce::Label leftLabel_;
    juce::Label centerLabel_;
    juce::Label rightLabel_;
    juce::Rectangle<int> leftRect_;
    juce::Rectangle<int> centerRect_;
    juce::Rectangle<int> rightRect_;
};
} // namespace trigger
