// Included from TriggerMainWindow.cpp inside namespace trigger.
// Required context: juce_gui_extra, ColourPickerWindow, applyNativeDarkTitleBar.
#pragma once

// ─── Preferences window ───────────────────────────────────────────────────────
class PreferencesWindow final : public juce::DocumentWindow
{
public:
    using ApplyFn = std::function<void (juce::Colour fired,
                                        juce::Colour tc,
                                        juce::Colour plain,
                                        juce::Colour custom)>;

    static PreferencesWindow* show (juce::Colour fired, juce::Colour tc,
                                    juce::Colour plain, juce::Colour custom,
                                    ApplyFn fn, juce::Component* relativeTo)
    {
        return new PreferencesWindow (fired, tc, plain, custom,
                                      std::move (fn), relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<PreferencesWindow> (this)]
            {
                if (auto* w = safe.getComponent()) delete w;
            });
    }

private:
    ApplyFn onApply_;

    // ── Square colour swatch — opens ColourPickerWindow on click ──────────────
    struct ColourSwatch final : juce::Component
    {
        juce::Colour                                      col_;
        juce::Component::SafePointer<ColourPickerWindow>  picker_;

        explicit ColourSwatch (juce::Colour c) : col_ (c) {}

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (col_);
            g.fillRoundedRectangle (r, 5.0f);
            if (isMouseOver())
            {
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (r, 5.0f);
            }
            g.setColour (isMouseOver() ? juce::Colour (0xff888888) : juce::Colour (0xff505050));
            g.drawRoundedRectangle (r, 5.0f, 1.2f);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { repaint(); }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (picker_ != nullptr) { picker_->toFront (true); return; }
            picker_ = ColourPickerWindow::show (
                "Choose Colour", col_,
                [safe = juce::Component::SafePointer<ColourSwatch> (this)] (juce::Colour c)
                {
                    if (safe != nullptr) { safe->col_ = c; safe->repaint(); }
                },
                getTopLevelComponent());
        }
    };

    // ── Content ───────────────────────────────────────────────────────────────
    struct Content final : juce::Component
    {
        PreferencesWindow& win_;

        juce::Label      sectionLbl_ { {}, "CLIP COLOURS" };
        juce::Label      lbl0_, lbl1_, lbl2_, lbl3_;
        ColourSwatch     sw0_, sw1_, sw2_, sw3_;
        juce::TextButton btnReset_ { "Reset" };
        juce::TextButton btnApply_   { "Apply" };
        juce::TextButton btnClose_   { "Close" };

        Content (PreferencesWindow& w,
                 juce::Colour fired, juce::Colour tc,
                 juce::Colour plain, juce::Colour custom)
            : win_ (w),
              sw0_ (fired), sw1_ (tc), sw2_ (plain), sw3_ (custom)
        {
            sectionLbl_.setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
            sectionLbl_.setColour (juce::Label::textColourId, juce::Colour (0xff555555));
            sectionLbl_.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (sectionLbl_);

            struct Row { juce::Label& lbl; const char* text; ColourSwatch& sw; };
            const Row rows[] = {
                { lbl0_, "Trigger fired:",      sw0_ },
                { lbl1_, "Connected (TC):",     sw1_ },
                { lbl2_, "Connected (plain):",  sw2_ },
                { lbl3_, "Connected (custom):", sw3_ },
            };
            for (auto& r : rows)
            {
                r.lbl.setText (r.text, juce::dontSendNotification);
                r.lbl.setFont (juce::FontOptions (13.5f));
                r.lbl.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcbc));
                r.lbl.setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (r.lbl);
                addAndMakeVisible (r.sw);
            }

            auto styleBtn = [&] (juce::TextButton& b)
            {
                b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe0e0e0));
                b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffe0e0e0));
                addAndMakeVisible (b);
            };
            styleBtn (btnReset_);
            styleBtn (btnApply_);
            styleBtn (btnClose_);
            btnApply_.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2d6a48));
            btnApply_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff245a3c));

            btnReset_.onClick = [this]
            {
                sw0_.col_ = kDefaultClipFired;           sw0_.repaint();
                sw1_.col_ = kDefaultClipConnectedTc;     sw1_.repaint();
                sw2_.col_ = kDefaultClipConnectedPlain;  sw2_.repaint();
                sw3_.col_ = kDefaultClipConnectedCustom; sw3_.repaint();
            };

            btnApply_.onClick = [this]
            {
                if (win_.onApply_)
                    win_.onApply_ (sw0_.col_, sw1_.col_, sw2_.col_, sw3_.col_);
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
            btnClose_.onClick = [this]
            {
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff171717));
            // hairline under section label
            g.setColour (juce::Colour (0xff2b2b2b));
            g.fillRect (18, 34, getWidth() - 36, 1);
            // hairline above buttons
            g.fillRect (0, getHeight() - 50, getWidth(), 1);
        }

        void resized() override
        {
            constexpr int kPad   = 18;
            constexpr int kSwW   = 48;    // rectangular swatch width
            constexpr int kSwH   = 20;    // rectangular swatch height
            constexpr int kRowH  = 30;
            const int     swX    = getWidth() - kPad - kSwW;

            sectionLbl_.setBounds (kPad, 8, getWidth() - kPad * 2, 20);

            juce::Label*  lbls[] = { &lbl0_, &lbl1_, &lbl2_, &lbl3_ };
            ColourSwatch* sws[]  = { &sw0_,  &sw1_,  &sw2_,  &sw3_  };
            for (int i = 0; i < 4; ++i)
            {
                const int y = 40 + i * kRowH;
                lbls[i]->setBounds (kPad, y + (kRowH - 18) / 2, swX - kPad - 10, 18);
                sws[i]->setBounds  (swX,  y + (kRowH - kSwH) / 2, kSwW, kSwH);
            }

            const int btnY = getHeight() - 40;
            btnClose_.setBounds (getWidth() - kPad - 86,       btnY, 86, 28);
            btnApply_.setBounds (getWidth() - kPad - 86 - 98,  btnY, 86, 28);
            btnReset_.setBounds (kPad,                         btnY, 76, 28);
        }
    };

    PreferencesWindow (juce::Colour fired, juce::Colour tc,
                       juce::Colour plain, juce::Colour custom,
                       ApplyFn fn, juce::Component* relativeTo)
        : juce::DocumentWindow (u8"Easy Trigger \u2014 Preferences",
                                juce::Colour (0xff1e1e1e),
                                juce::DocumentWindow::closeButton),
          onApply_ (std::move (fn))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, fired, tc, plain, custom), true);
        // height: section(40) + 4 rows×30(120) + gap(14) + separator(1) + gap(9) + btn(28) + pad(14) = 226
        constexpr int kW = 360, kH = 226;
        centreWithSize (kW, kH);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - kW / 2, rc.getCentreY() - kH / 2, kW, kH);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreferencesWindow)
};
