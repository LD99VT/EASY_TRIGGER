// Included from TriggerMainWindow.cpp inside namespace trigger.
// Required context: juce_gui_extra, Version.h (build-generated), applyNativeDarkTitleBar.
#pragma once

// ─── About dialog ─────────────────────────────────────────────────────────────
class AboutDialog final : public juce::DocumentWindow
{
public:
    static void show (juce::Component* relativeTo = nullptr,
                      juce::Font fontBold  = {},
                      juce::Font fontLight = {})
    {
        new AboutDialog (relativeTo, fontBold, fontLight);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<AboutDialog> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

private:
    AboutDialog (juce::Component* relativeTo, juce::Font fontBold, juce::Font fontLight)
        : juce::DocumentWindow ("About Easy Trigger",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, fontBold, fontLight), true);
        centreWithSize (440, 340);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 220, rc.getCentreY() - 170, 440, 340);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
#endif
        toFront (true);
    }

    struct Content final : juce::Component
    {
        AboutDialog& owner_;
        juce::Font   fontBold_;
        juce::Font   fontLight_;
        juce::TextButton btnClose_  { "Close" };
        juce::TextButton btnGithub_ { "GitHub" };

        Content (AboutDialog& o, juce::Font fontBold, juce::Font fontLight)
            : owner_ (o),
              fontBold_  (fontBold.getHeight()  > 0.0f ? fontBold  : juce::Font (juce::FontOptions (30.0f, juce::Font::bold))),
              fontLight_ (fontLight.getHeight() > 0.0f ? fontLight : juce::Font (juce::FontOptions (30.0f)))
        {
            btnClose_.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btnClose_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
            btnClose_.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btnClose_.setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btnClose_.onClick = [this] { owner_.closeButtonPressed(); };
            addAndMakeVisible (btnClose_);

            btnGithub_.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x2e, 0x6e, 0x4e));
            btnGithub_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x28, 0x60, 0x44));
            btnGithub_.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btnGithub_.setColour (juce::TextButton::textColourOnId,   juce::Colour::fromRGB (0xe1, 0xe6, 0xef));
            btnGithub_.onClick = []
            {
                juce::URL ("https://github.com/LD99VT/EASY_TRIGGER").launchInDefaultBrowser();
            };
            addAndMakeVisible (btnGithub_);
        }

        void paint (juce::Graphics& g) override
        {
            const auto w = getWidth();
            const auto h = getHeight();
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));

            // Separator line
            g.setColour (juce::Colour::fromRGB (0x30, 0x30, 0x30));
            g.drawHorizontalLine (h - 60, 20.0f, (float) w - 20);

            // "EASY " — bold, light grey
            const float titleH = 36.0f;
            const juce::Font fBold  = fontBold_.withHeight (titleH);
            const juce::Font fLight = fontLight_.withHeight (titleH);
            const juce::String easyStr = "EASY ";
            const juce::String trigStr = "TRIGGER";
            const float easyW  = fBold.getStringWidthFloat (easyStr);
            const float trigW  = fLight.getStringWidthFloat (trigStr);
            const float totalW = easyW + trigW;
            const float startX = ((float) w - totalW) * 0.5f;
            const int   titleY = 22;

            g.setFont (fBold);
            g.setColour (juce::Colour::fromRGB (0xce, 0xce, 0xce));
            g.drawText (easyStr, (int) startX, titleY, (int) easyW + 2, (int) titleH, juce::Justification::centredLeft);

            g.setFont (fLight);
            g.setColour (juce::Colour::fromRGB (0x8a, 0x8a, 0x8a));
            g.drawText (trigStr, (int) (startX + easyW), titleY, (int) trigW + 2, (int) titleH, juce::Justification::centredLeft);

            // Version
            g.setFont (juce::FontOptions (13.0f));
            g.setColour (juce::Colour::fromRGB (0x8a, 0x8a, 0x8a));
            g.drawText ("v" + juce::String (bridge::version::kAppVersion),
                        0, 64, w, 18, juce::Justification::centred);

            // Description
            g.setFont (juce::FontOptions (13.0f));
            g.setColour (juce::Colour::fromRGB (0xc0, 0xc0, 0xc0));
            const juce::String line1 =
                "Timecode-driven clip trigger for Resolume Avenue / Arena.";
            const juce::String line2 =
                u8"In: LTC \u2022 MTC \u2022 ArtNet \u2022 OSC  \u2192  Col / Layer\u2013Clip / Group\u2013Col";
            g.drawFittedText (line1, 24, 92,  w - 48, 20, juce::Justification::centred, 1);
            g.drawFittedText (line2, 24, 114, w - 48, 20, juce::Justification::centred, 1);

            // Tech stack
            g.setFont (juce::FontOptions (12.0f));
            g.setColour (juce::Colour::fromRGB (0x80, 0x80, 0x80));
            g.drawText (u8"Built with JUCE  \u2022  Steinberg ASIO SDK",
                        0, 142, w, 18, juce::Justification::centred);

            // License note
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (juce::Colour::fromRGB (0x55, 0x55, 0x55));
            g.drawText (u8"Open Source  \u2014  MIT License",
                        0, 164, w, 16, juce::Justification::centred);
            g.drawText (u8"\u00A9 2024\u20132026 LD99VT",
                        0, 181, w, 16, juce::Justification::centred);
        }

        void resized() override
        {
            const int w = getWidth();
            const int h = getHeight();
            btnGithub_.setBounds (w / 2 - 110, h - 44, 100, 30);
            btnClose_.setBounds  (w / 2 + 10,  h - 44, 100, 30);
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutDialog)
};
