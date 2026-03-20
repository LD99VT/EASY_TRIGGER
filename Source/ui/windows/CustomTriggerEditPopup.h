// Included from TriggerMainWindow.cpp inside namespace trigger.
#pragma once

// ─── Custom Trigger source editor popup ─────────────────────────────────────
// Small window with two radio-style toggles (Grp/Col | Lyr/Clp) and 4 fields.
class CustomTriggerEditPopup final : public juce::DocumentWindow
{
public:
    using OnAccept = std::function<void (const juce::String& type,
                                         const juce::String& col,
                                         const juce::String& layer,
                                         const juce::String& clip)>;

    static CustomTriggerEditPopup* show (const juce::String& windowTitle,
                                         const juce::String& type,
                                         const juce::String& col,
                                         const juce::String& layer,
                                         const juce::String& clip,
                                         OnAccept fn,
                                         juce::Component* relativeTo)
    {
        return new CustomTriggerEditPopup (windowTitle, type, col, layer, clip,
                                          std::move (fn), relativeTo);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<CustomTriggerEditPopup> (this)]
            { if (auto* w = safe.getComponent()) delete w; });
    }

private:
    OnAccept onAccept_;

    // ── small labelled text field -----------------------------------------
    struct FieldRow final : juce::Component
    {
        juce::Label      lbl_;
        juce::TextEditor ed_;
        juce::TextButton btnPlus_  { "+" };
        juce::TextButton btnMinus_ { "-" };

        FieldRow (const juce::String& labelText, const juce::String& value)
        {
            lbl_.setText (labelText, juce::dontSendNotification);
            lbl_.setFont (juce::FontOptions (13.0f));
            lbl_.setColour (juce::Label::textColourId, juce::Colour (0xffbcbcbc));
            lbl_.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (lbl_);

            ed_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff242424));
            ed_.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0x00000000));
            ed_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0x00000000));
            ed_.setColour (juce::TextEditor::textColourId,       juce::Colour (0xffcacaca));
            ed_.setBorder (juce::BorderSize<int> (0));
            ed_.setIndents (6, 0);
            ed_.setJustification (juce::Justification::centredLeft);
            ed_.setFont (juce::FontOptions (13.0f));
            ed_.setInputRestrictions (4, "0123456789");
            ed_.onReturnKey = [this] { ed_.giveAwayKeyboardFocus(); };
            ed_.setText (value, juce::dontSendNotification);
            addAndMakeVisible (ed_);

            for (auto* b : { &btnPlus_, &btnMinus_ })
                addAndMakeVisible (*b);
            btnPlus_.onClick  = [this] { step (+1); };
            btnMinus_.onClick = [this] { step (-1); };
        }

        void step (int delta)
        {
            int v = ed_.getText().getIntValue() + delta;
            if (v < 1) v = 1;
            ed_.setText (juce::String (v), juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            lbl_.setBounds (r.removeFromLeft (64));
            constexpr int kBtnSz = 32, kGap = 2;
            btnMinus_.setBounds (r.removeFromRight (kBtnSz));
            r.removeFromRight (kGap);
            btnPlus_.setBounds (r.removeFromRight (kBtnSz));
            r.removeFromRight (kGap);
            ed_.setBounds (r.reduced (0, 3));
        }

        juce::String getValue() const { return ed_.getText().trim(); }
        void setValue (const juce::String& v) { ed_.setText (v, juce::dontSendNotification); }
        void setActive (bool on)
        {
            ed_.setEnabled (on);
            ed_.setAlpha (on ? 1.0f : 0.32f);
            lbl_.setAlpha (on ? 1.0f : 0.32f);
            btnPlus_.setEnabled (on);
            btnMinus_.setEnabled (on);
            btnPlus_.setAlpha  (on ? 1.0f : 0.32f);
            btnMinus_.setAlpha (on ? 1.0f : 0.32f);
        }
    };

    // ── radio toggle pill ----------------------------------------------------
    struct RadioPill final : juce::Component
    {
        std::function<void(int)> onSelect;  // 0 = gc, 1 = lc

        RadioPill()
        {
            for (int i = 0; i < 2; ++i)
            {
                btns_[i].setClickingTogglesState (false);
                btns_[i].setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2e2e2e));
                btns_[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e2e2e));
                btns_[i].setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff9a9a9a));
                btns_[i].setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffe0e0e0));
                addAndMakeVisible (btns_[i]);
            }
            btns_[0].setButtonText ("Group / Column");
            btns_[1].setButtonText ("Layer / Clip");
            btns_[0].onClick = [this] { setSelected (0); if (onSelect) onSelect (0); };
            btns_[1].onClick = [this] { setSelected (1); if (onSelect) onSelect (1); };
        }

        void setSelected (int idx)
        {
            sel_ = idx;
            for (int i = 0; i < 2; ++i)
            {
                const bool active = (i == sel_);
                btns_[i].setColour (juce::TextButton::buttonColourId,
                    active ? juce::Colour (0xff4a4a4a) : juce::Colour (0xff2e2e2e));
                btns_[i].setColour (juce::TextButton::textColourOffId,
                    active ? juce::Colour (0xffe8e8e8) : juce::Colour (0xff9a9a9a));
                btns_[i].repaint();
            }
        }

        int getSelected() const { return sel_; }

        void resized() override
        {
            auto r = getLocalBounds();
            const int hw = r.getWidth() / 2;
            btns_[0].setBounds (r.removeFromLeft (hw).reduced (1, 0));
            btns_[1].setBounds (r.reduced (1, 0));
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colour (0xff2b2b2b));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
        }

    private:
        juce::TextButton btns_[2];
        int sel_ { 0 };
    };

    // ── Content ─────────────────────────────────────────────────────────────
    struct Content final : juce::Component
    {
        CustomTriggerEditPopup& win_;

        RadioPill            radio_;
        FieldRow             rowGrp_;
        FieldRow             rowCol_;
        FieldRow             rowLyr_;
        FieldRow             rowClp_;
        juce::TextButton     btnApply_  { "Apply" };
        juce::TextButton     btnReset_  { "Reset" };
        juce::TextButton     btnCancel_ { "Cancel" };

        Content (CustomTriggerEditPopup& w,
                 const juce::String& type,
                 const juce::String& col,
                 const juce::String& layer,
                 const juce::String& clip)
            : win_    (w),
              rowGrp_ ("Group",  type == "gc" ? layer : ""),
              rowCol_ ("Column", (type == "gc" || type == "col") ? col : ""),
              rowLyr_ ("Layer",  type == "lc" ? layer : ""),
              rowClp_ ("Clip",   type == "lc" ? clip  : "")
        {
            addAndMakeVisible (radio_);
            addAndMakeVisible (rowGrp_);
            addAndMakeVisible (rowCol_);
            addAndMakeVisible (rowLyr_);
            addAndMakeVisible (rowClp_);

            const int sel = (type == "lc") ? 1 : 0;
            radio_.setSelected (sel);
            applyMode (sel);

            radio_.onSelect = [this] (int idx) { applyMode (idx); };

            auto styleBtn = [&] (juce::TextButton& b)
            {
                b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3e3e3e));
                b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe0e0e0));
                b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffe0e0e0));
                addAndMakeVisible (b);
            };
            styleBtn (btnApply_);
            styleBtn (btnReset_);
            styleBtn (btnCancel_);
            btnApply_.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2d6a48));
            btnApply_.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff245a3c));

            btnApply_.onClick = [this]
            {
                const int mode = radio_.getSelected();
                juce::String type2, col2, layer2, clip2;
                if (mode == 1)  // lc
                {
                    type2  = "lc";
                    layer2 = rowLyr_.getValue();
                    clip2  = rowClp_.getValue();
                }
                else  // gc or col
                {
                    const juce::String grp = rowGrp_.getValue();
                    col2 = rowCol_.getValue();
                    if (grp.isNotEmpty()) { type2 = "gc"; layer2 = grp; }
                    else                 { type2 = "col"; }
                }
                if (win_.onAccept_)
                    win_.onAccept_ (type2, col2, layer2, clip2);
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
            btnReset_.onClick = [this]
            {
                rowGrp_.setValue ({});
                rowCol_.setValue ({});
                rowLyr_.setValue ({});
                rowClp_.setValue ({});
                radio_.setSelected (0);
                applyMode (0);
            };
            btnCancel_.onClick = [this]
            {
                juce::MessageManager::callAsync ([w = &win_] { delete w; });
            };
        }

        void applyMode (int idx)
        {
            rowGrp_.setActive (idx == 0);
            rowCol_.setActive (idx == 0);
            rowLyr_.setActive (idx == 1);
            rowClp_.setActive (idx == 1);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff171717));
            // separator between radio and fields
            g.setColour (juce::Colour (0xff2b2b2b));
            g.fillRect (12, 42, getWidth() - 24, 1);
            // separator above buttons
            g.fillRect (0, getHeight() - 50, getWidth(), 1);
        }

        void resized() override
        {
            constexpr int kPad  = 14;
            constexpr int kRowH = 32;
            constexpr int gap   = 8;

            radio_.setBounds (kPad, 8, getWidth() - kPad * 2, 26);

            int y = 50;
            rowGrp_.setBounds (kPad, y, getWidth() - kPad * 2, kRowH); y += kRowH + 2;
            rowCol_.setBounds (kPad, y, getWidth() - kPad * 2, kRowH); y += kRowH + 2;
            rowLyr_.setBounds (kPad, y, getWidth() - kPad * 2, kRowH); y += kRowH + 2;
            rowClp_.setBounds (kPad, y, getWidth() - kPad * 2, kRowH);

            const int btnY = getHeight() - 38;
            btnReset_.setBounds  (kPad,                        btnY, 76, 28);
            btnApply_.setBounds  (getWidth() - kPad - 80 - 88, btnY, 80, 28);
            btnCancel_.setBounds (getWidth() - kPad - 80,      btnY, 80, 28);
        }
    };

    CustomTriggerEditPopup (const juce::String& windowTitle,
                             const juce::String& type,
                             const juce::String& col,
                             const juce::String& layer,
                             const juce::String& clip,
                             OnAccept fn,
                             juce::Component* relativeTo)
        : juce::DocumentWindow (windowTitle,
                                juce::Colour (0xff1e1e1e),
                                juce::DocumentWindow::closeButton),
          onAccept_ (std::move (fn))
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentOwned (new Content (*this, type, col, layer, clip), true);

        // height: radio(8+26) + sep(1+8) + 4 rows*(32+2) + sep(1) + buttons(50) = 234
        constexpr int kW = 330, kH = 248;
        centreWithSize (kW, kH);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getTopLevelComponent()->getScreenBounds();
            setBounds (rc.getCentreX() - kW / 2, rc.getCentreY() - kH / 2, kW, kH);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
#endif
        toFront (true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomTriggerEditPopup)
};
