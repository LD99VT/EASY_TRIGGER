// Included from TriggerMainWindow.cpp inside namespace trigger (anonymous namespace).
// Required context: DarkDialog must be included before this.
#pragma once

class GetClipsOptionsWindow final : public juce::DocumentWindow
{
public:
    using ApplyHandler = std::function<void (bool, bool)>;

    GetClipsOptionsWindow (bool includeWithOffset,
                           bool includeWithoutOffset,
                           ApplyHandler onApply,
                           juce::Component* relativeTo)
        : juce::DocumentWindow ("Get Clips",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton),
          onApply_ (std::move (onApply))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, includeWithOffset, includeWithoutOffset), true);
        centreWithSize (360, 190);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 180, rc.getCentreY() - 95, 360, 190);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
        if (auto* hwnd = (HWND) getWindowHandle())
        {
            ::SendMessageW (hwnd, WM_SETICON, 0, 0);
            ::SendMessageW (hwnd, WM_SETICON, 1, 0);
            constexpr long kGwlStyle = -16;
            long st = (long) ::GetWindowLongPtrW (hwnd, kGwlStyle);
            st &= ~(long) 0x00040000L;
            st &= ~(long) 0x00010000L;
            ::SetWindowLongPtrW (hwnd, kGwlStyle, st);
            ::SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif
        toFront (true);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<GetClipsOptionsWindow> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

    void applySelection (bool includeWithOffset, bool includeWithoutOffset)
    {
        if (! includeWithOffset && ! includeWithoutOffset)
        {
            DarkDialog::show ("Get Clips", "Select at least one option.", this);
            return;
        }

        if (onApply_ != nullptr)
            onApply_ (includeWithOffset, includeWithoutOffset);
        closeButtonPressed();
    }

private:
    ApplyHandler onApply_;

    struct Content final : juce::Component
    {
        GetClipsOptionsWindow& owner_;
        juce::Label title_ { {}, "Select which clips to import" };
        juce::ToggleButton withOffset_;
        juce::ToggleButton withoutOffset_;
        juce::TextButton apply_ { "Apply" };
        juce::TextButton cancel_ { "Cancel" };

        Content (GetClipsOptionsWindow& owner, bool includeWithOffset, bool includeWithoutOffset)
            : owner_ (owner)
        {
            title_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe0, 0xe0, 0xe0));
            title_.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
            title_.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (title_);

            withOffset_.setButtonText ("Get Clips with timecode");
            withoutOffset_.setButtonText ("Get Clips without timecode");
            withOffset_.setToggleState (includeWithOffset, juce::dontSendNotification);
            withoutOffset_.setToggleState (includeWithoutOffset, juce::dontSendNotification);
            addAndMakeVisible (withOffset_);
            addAndMakeVisible (withoutOffset_);

            for (auto* b : { &apply_, &cancel_ })
            {
                b->setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                b->setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                addAndMakeVisible (*b);
            }

            apply_.onClick = [this]
            {
                owner_.applySelection (withOffset_.getToggleState(),
                                       withoutOffset_.getToggleState());
            };
            cancel_.onClick = [this] { owner_.closeButtonPressed(); };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (16);
            title_.setBounds (area.removeFromTop (24));
            area.removeFromTop (10);
            withOffset_.setBounds (area.removeFromTop (26));
            area.removeFromTop (8);
            withoutOffset_.setBounds (area.removeFromTop (26));
            area.removeFromTop (18);
            auto buttons = area.removeFromTop (32);
            cancel_.setBounds (buttons.removeFromRight (100));
            buttons.removeFromRight (10);
            apply_.setBounds (buttons.removeFromRight (100));
        }
    };
};


