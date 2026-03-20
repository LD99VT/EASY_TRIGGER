// Included from TriggerMainWindow.cpp inside namespace trigger.
// Required context: juce_gui_extra, applyNativeDarkTitleBar.
#pragma once

// ─── Colour picker modal window ───────────────────────────────────────────────
class ColourPickerWindow final : public juce::DocumentWindow
{
public:
    using OnAccept = std::function<void (juce::Colour)>;

    static ColourPickerWindow* show (const juce::String& title,
                                     juce::Colour initial,
                                     OnAccept fn,
                                     juce::Component* relativeTo)
    {
        return new ColourPickerWindow (title, initial, std::move (fn), relativeTo);
    }

    void closeButtonPressed() override { delete this; }

private:
    OnAccept onAccept_;

    struct Content final : juce::Component
    {
        ColourPickerWindow& win_;
        juce::ColourSelector selector_;
        juce::TextButton     btnOk_     { "OK" };
        juce::TextButton     btnCancel_ { "Cancel" };

        Content (ColourPickerWindow& w, juce::Colour initial)
            : win_ (w),
              selector_ (juce::ColourSelector::showColourAtTop
                       | juce::ColourSelector::showSliders
                       | juce::ColourSelector::showColourspace)
        {
            selector_.setCurrentColour (initial);
            selector_.setColour (juce::ColourSelector::backgroundColourId,
                                 juce::Colour (0xff1a1a1a));
            addAndMakeVisible (selector_);

            auto style = [&] (juce::TextButton& b)
            {
                b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe0e0e0));
                b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffe0e0e0));
                addAndMakeVisible (b);
            };
            style (btnOk_);
            style (btnCancel_);
            btnOk_.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2d6a48));
            btnOk_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff245a3c));

            btnOk_.onClick = [this]
            {
                const auto col = selector_.getCurrentColour();
                if (win_.onAccept_) win_.onAccept_ (col);
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
            btnCancel_.onClick = [this]
            {
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff1a1a1a));
            g.setColour (juce::Colour (0xff2a2a2a));
            g.fillRect (0, getHeight() - 54, getWidth(), 1);
        }

        void resized() override
        {
            auto area    = getLocalBounds();
            auto btnArea = area.removeFromBottom (54).reduced (14, 12);
            selector_.setBounds (area);
            btnCancel_.setBounds (btnArea.removeFromRight (90));
            btnArea.removeFromRight (8);
            btnOk_.setBounds (btnArea.removeFromRight (90));
        }
    };

    ColourPickerWindow (const juce::String& title, juce::Colour initial,
                        OnAccept fn, juce::Component* relativeTo)
        : juce::DocumentWindow (title,
                                juce::Colour (0xff1e1e1e),
                                juce::DocumentWindow::closeButton),
          onAccept_ (std::move (fn))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, initial), true);
        centreWithSize (296, 390);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 148, rc.getCentreY() - 195, 296, 390);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourPickerWindow)
};
