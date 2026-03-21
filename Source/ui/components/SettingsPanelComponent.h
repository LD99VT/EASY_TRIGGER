#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace trigger
{
class SettingsPanelComponent final : public juce::Component
{
public:
    SettingsPanelComponent (juce::TextButton& getClipsButton, juce::TextButton& createCustomButton)
        : getClipsButton_ (getClipsButton),
          createCustomButton_ (createCustomButton)
    {
        addAndMakeVisible (viewport_);
        viewport_.setViewedComponent (&content_, false);
        viewport_.setScrollBarsShown (true, false);
        viewport_.setScrollBarThickness (8);

        addAndMakeVisible (getClipsButton_);
        addAndMakeVisible (createCustomButton_);
    }

    juce::Viewport& viewport() noexcept { return viewport_; }
    juce::Component& content() noexcept { return content_; }
    const juce::Component& content() const noexcept { return content_; }

    int getScrollBarThickness() const noexcept { return viewport_.getScrollBarThickness(); }
    juce::Point<int> getViewPosition() const noexcept { return viewport_.getViewPosition(); }

    juce::Rectangle<int> getViewportBoundsInParent() const
    {
        return viewport_.getBounds() + getPosition();
    }

    void setContentSize (int width, int height)
    {
        content_.setSize (width, height);
    }

    void layoutPanel (juce::Rectangle<int> bounds)
    {
        auto footerArea = bounds.removeFromBottom (88);
        auto createCustomRow = footerArea.removeFromBottom (40);
        footerArea.removeFromBottom (4);
        auto getTriggersRow = footerArea.removeFromBottom (40);
        footerArea.removeFromBottom (4);

        viewport_.setBounds (bounds);
        getClipsButton_.setBounds (getTriggersRow);
        createCustomButton_.setBounds (createCustomRow);
    }

    void bringButtonsToFront()
    {
        getClipsButton_.toFront (false);
        createCustomButton_.toFront (false);
        viewport_.toBack();
    }

private:
    juce::Viewport viewport_;
    juce::Component content_;
    juce::TextButton& getClipsButton_;
    juce::TextButton& createCustomButton_;
};
} // namespace trigger
