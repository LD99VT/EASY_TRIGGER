// Included from TriggerMainWindow.cpp inside namespace trigger.
// Required context: juce_gui_extra, applyNativeDarkTitleBar.
#pragma once

// ─── Generic dark-themed modal dialog ─────────────────────────────────────────
// Self-owning (heap-allocated, deletes itself on close).
// Usage:  DarkDialog::show ("Title", "Message text", parentComponent);
class DarkDialog final : public juce::DocumentWindow
{
public:
    static void show (const juce::String& title,
                      const juce::String& message,
                      juce::Component*    relativeTo = nullptr,
                      const juce::String& buttonText = "OK")
    {
        new DarkDialog (title, message, buttonText, relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<DarkDialog> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

private:
    DarkDialog (const juce::String& title,
                const juce::String& message,
                const juce::String& buttonText,
                juce::Component*    relativeTo)
        : juce::DocumentWindow (title,
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, message, buttonText), true);
        centreWithSize (400, 160);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 200, rc.getCentreY() - 80, 400, 160);
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
            st &= ~(long) 0x00040000L;  // WS_THICKFRAME
            st &= ~(long) 0x00010000L;  // WS_MAXIMIZEBOX
            ::SetWindowLongPtrW (hwnd, kGwlStyle, st);
            ::SetWindowPos (hwnd, nullptr, 0, 0, 0, 0,
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif
        toFront (true);
    }

    struct Content final : juce::Component
    {
        DarkDialog&      owner_;
        juce::Label      msg_;
        juce::TextButton btn_;

        Content (DarkDialog& o, const juce::String& message, const juce::String& btnText)
            : owner_ (o), btn_ (btnText)
        {
            msg_.setText (message, juce::dontSendNotification);
            msg_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe0, 0xe0, 0xe0));
            msg_.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (msg_);

            btn_.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btn_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btn_.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btn_.setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btn_.onClick = [this] { owner_.closeButtonPressed(); };
            addAndMakeVisible (btn_);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            constexpr int kPad = 20;
            msg_.setBounds (kPad, kPad, getWidth() - kPad * 2, getHeight() - 70);
            btn_.setBounds ((getWidth() - 100) / 2, getHeight() - 42, 100, 32);
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DarkDialog)
};
