// Included from TriggerMenuHandlers.cpp inside namespace trigger (anonymous namespace).
// Required context: DarkDialog must be included before this.
#pragma once

class SaveConfigOptionsWindow final : public juce::DocumentWindow
{
public:
    using ApplyHandler = std::function<void (bool includeSettings, bool includeTriggers)>;

    static SaveConfigOptionsWindow* show (bool includeSettings,
                                          bool includeTriggers,
                                          bool saveAs,
                                          ApplyHandler onApply,
                                          juce::Component* relativeTo)
    {
        return new SaveConfigOptionsWindow (includeSettings, includeTriggers, saveAs,
                                            std::move (onApply), relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<SaveConfigOptionsWindow> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

    void applySelection (bool includeSettings, bool includeTriggers)
    {
        if (! includeSettings && ! includeTriggers)
        {
            DarkDialog::show ("Save Config", "Select at least one option.", this);
            return;
        }

        if (onApply_ != nullptr)
            onApply_ (includeSettings, includeTriggers);
        closeButtonPressed();
    }

private:
    ApplyHandler onApply_;

    struct Content final : juce::Component
    {
        SaveConfigOptionsWindow& owner_;
        juce::Label title_;
        juce::ToggleButton settings_;
        juce::ToggleButton triggers_;
        juce::TextButton save_ { "Save" };
        juce::TextButton cancel_ { "Cancel" };

        Content (SaveConfigOptionsWindow& owner, bool includeSettings, bool includeTriggers, bool saveAs)
            : owner_ (owner)
        {
            title_.setText (saveAs ? "Choose what to save as a new file"
                                   : "Choose what to save",
                            juce::dontSendNotification);
            title_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe0, 0xe0, 0xe0));
            title_.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
            title_.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (title_);

            settings_.setButtonText ("Settings");
            triggers_.setButtonText ("Triggers");
            settings_.setToggleState (includeSettings, juce::dontSendNotification);
            triggers_.setToggleState (includeTriggers, juce::dontSendNotification);
            addAndMakeVisible (settings_);
            addAndMakeVisible (triggers_);

            for (auto* b : { &save_, &cancel_ })
            {
                b->setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                b->setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                addAndMakeVisible (*b);
            }
            save_.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2d6a48));
            save_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff245a3c));

            save_.onClick = [this]
            {
                owner_.applySelection (settings_.getToggleState(), triggers_.getToggleState());
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
            settings_.setBounds (area.removeFromTop (26));
            area.removeFromTop (8);
            triggers_.setBounds (area.removeFromTop (26));
            area.removeFromTop (18);
            auto buttons = area.removeFromTop (32);
            cancel_.setBounds (buttons.removeFromRight (100));
            buttons.removeFromRight (10);
            save_.setBounds (buttons.removeFromRight (100));
        }
    };

    SaveConfigOptionsWindow (bool includeSettings,
                             bool includeTriggers,
                             bool saveAs,
                             ApplyHandler onApply,
                             juce::Component* relativeTo)
        : juce::DocumentWindow (saveAs ? "Save Config As" : "Save Config",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton),
          onApply_ (std::move (onApply))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, includeSettings, includeTriggers, saveAs), true);
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
};
