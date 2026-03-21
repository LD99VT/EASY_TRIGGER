#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace trigger
{
class UpdatePromptWindow final : public juce::DocumentWindow
{
public:
    static void show (const juce::String& currentVersion,
                      const juce::String& latestVersion,
                      const juce::String& releaseNotes,
                      std::function<void()> onUpdate,
                      juce::Component* relativeTo = nullptr)
    {
        new UpdatePromptWindow (currentVersion, latestVersion, releaseNotes, std::move (onUpdate), relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<UpdatePromptWindow> (this)]
        {
            if (auto* w = safe.getComponent())
                delete w;
        });
    }

private:
    UpdatePromptWindow (const juce::String& currentVersion,
                        const juce::String& latestVersion,
                        const juce::String& releaseNotes,
                        std::function<void()> onUpdate,
                        juce::Component* relativeTo)
        : juce::DocumentWindow ("Update Available",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, currentVersion, latestVersion, releaseNotes, std::move (onUpdate)), true);
        centreWithSize (520, 320);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 260, rc.getCentreY() - 160, 520, 320);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
#endif
        toFront (true);
    }

    struct Content final : juce::Component
    {
        UpdatePromptWindow& owner_;
        juce::Label title_;
        juce::Label body_;
        juce::TextEditor notes_;
        juce::TextButton updateBtn_ { "Update" };
        juce::TextButton cancelBtn_ { "Cancel" };
        std::function<void()> onUpdate_;

        Content (UpdatePromptWindow& owner,
                 const juce::String& currentVersion,
                 const juce::String& latestVersion,
                 const juce::String& releaseNotes,
                 std::function<void()> onUpdate)
            : owner_ (owner), onUpdate_ (std::move (onUpdate))
        {
            title_.setText ("Easy Trigger " + latestVersion + " is available", juce::dontSendNotification);
            title_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xe6, 0xe6, 0xe6));
            title_.setFont (juce::FontOptions (18.0f).withStyle ("Bold"));
            addAndMakeVisible (title_);

            body_.setText ("Current version: " + currentVersion + "\nNew version: " + latestVersion,
                           juce::dontSendNotification);
            body_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0xb8, 0xbd, 0xc7));
            body_.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (body_);

            notes_.setMultiLine (true);
            notes_.setReadOnly (true);
            notes_.setScrollbarsShown (true);
            notes_.setText (releaseNotes.isNotEmpty() ? releaseNotes : "Release notes unavailable.");
            notes_.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (0x12, 0x12, 0x12));
            notes_.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xda, 0xda, 0xda));
            notes_.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGB (0x34, 0x34, 0x34));
            addAndMakeVisible (notes_);

            for (auto* btn : { &updateBtn_, &cancelBtn_ })
            {
                btn->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                btn->setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
                addAndMakeVisible (*btn);
            }

            updateBtn_.onClick = [this]
            {
                if (onUpdate_ != nullptr)
                    onUpdate_();
                owner_.closeButtonPressed();
            };
            cancelBtn_.onClick = [this] { owner_.closeButtonPressed(); };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));
        }

        void resized() override
        {
            constexpr int kPad = 18;
            auto area = getLocalBounds().reduced (kPad);
            title_.setBounds (area.removeFromTop (28));
            area.removeFromTop (8);
            body_.setBounds (area.removeFromTop (40));
            area.removeFromTop (8);
            auto buttons = area.removeFromBottom (34);
            cancelBtn_.setBounds (buttons.removeFromRight (110));
            buttons.removeFromRight (8);
            updateBtn_.setBounds (buttons.removeFromRight (110));
            area.removeFromBottom (8);
            notes_.setBounds (area);
        }
    };
};
} // namespace trigger
