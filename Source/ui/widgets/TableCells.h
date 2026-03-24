// Included from TriggerMainWindow.cpp inside namespace trigger (anonymous namespace).
// Required context: StyleHelpers.h (paintFlatBtn, kFlatBtn*), clipRowFont().
#pragma once

#include "../windows/CustomTriggerEditPopup.h"

class InlineTextCell final : public juce::TextEditor
{
public:
    std::function<void(const juce::String&)> onCommit;
    InlineTextCell()
    {
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        setBorder (juce::BorderSize<int> (0));
        setIndents (4, 0);
        setJustification (juce::Justification::centredLeft);
        onReturnKey = [this] { if (onCommit) onCommit (getText()); };
        onFocusLost = [this] { if (onCommit) onCommit (getText()); };
    }
};

class InlineButtonCell final : public juce::TextButton
{
public:
    std::function<void()> onPress;
    InlineButtonCell()
    {
        onClick = [this] { if (onPress) onPress(); };
    }
};

class InlineTestButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;

    InlineTestButtonCell() : juce::Button ("test")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (30.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());

        paintFlatBtn (g, sq, isHovered, isDown);

        juce::Path tri;
        const auto cx = sq.getCentreX();
        const auto cy = sq.getCentreY();
        const float w = 7.0f;
        const float h = 9.0f;
        tri.startNewSubPath (cx - w * 0.5f, cy - h * 0.5f);
        tri.lineTo (cx - w * 0.5f, cy + h * 0.5f);
        tri.lineTo (cx + w * 0.6f, cy);
        tri.closeSubPath();
        g.setColour (kFlatBtnIcon);
        g.fillPath (tri);
    }
};

class InlineSendTargetCell final : public juce::Component
{
public:
    std::function<void(int)> onSelectionChanged;

    InlineSendTargetCell()
    {
        addAndMakeVisible (combo_);
        styleCombo (combo_);
        combo_.setColour (juce::ComboBox::backgroundColourId, kFlatBtnBg);
        combo_.setColour (juce::ComboBox::outlineColourId,    kFlatBtnBg);
        combo_.onChange = [this]
        {
            if (isUpdating_)
                return;
            if (onSelectionChanged != nullptr)
                onSelectionChanged (combo_.getSelectedId());
        };
    }

    void setChoices (int maxTargets, int selectedIndex)
    {
        isUpdating_ = true;
        combo_.clear (juce::dontSendNotification);
        combo_.addItem ("All", 1);
        for (int i = 1; i <= maxTargets; ++i)
            combo_.addItem ("Send " + juce::String (i), i + 1);
        combo_.setSelectedId (selectedIndex + 1, juce::dontSendNotification);
        isUpdating_ = false;
    }

    void resized() override
    {
        combo_.setBounds (getLocalBounds().reduced (0, 4));
    }

private:
    juce::ComboBox combo_;
    bool isUpdating_ { false };
};

class RangeModeButton final : public juce::Button
{
public:
    std::function<void()> onPress;

    RangeModeButton() : juce::Button ("range-mode")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setMode (const juce::String& mode, const juce::String& tooltipText)
    {
        mode_ = mode;
        setTooltip (tooltipText);
        repaint();
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f, 3.0f);
        paintFlatBtn (g, b, isHovered, isDown);
        g.setColour (kFlatBtnIcon);
        g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));

        juce::String label = "MID";
        if (mode_ == "pre")  label = "PRE";
        if (mode_ == "post") label = "POST";
        g.drawFittedText (label, b.toNearestInt(), juce::Justification::centred, 1);
    }

private:
    juce::String mode_ { "mid" };
};

class InlineRangeCell final : public juce::Component
{
public:
    std::function<void(const juce::String&)> onRangeCommit;
    std::function<void(const juce::String&)> onModeChanged;

    InlineRangeCell()
    {
        addAndMakeVisible (modeBtn_);
        addAndMakeVisible (editor_);

        editor_.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor_.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor_.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (0xca, 0xca, 0xca));
        editor_.setBorder (juce::BorderSize<int> (0));
        editor_.setIndents (4, 0);
        editor_.setJustification (juce::Justification::centredLeft);
        editor_.onReturnKey = [this] { if (onRangeCommit) onRangeCommit (editor_.getText()); };
        editor_.onFocusLost = [this] { if (onRangeCommit) onRangeCommit (editor_.getText()); };

        modeBtn_.onPress = [this]
        {
            if (mode_ == "pre")      mode_ = "mid";
            else if (mode_ == "mid") mode_ = "post";
            else                     mode_ = "pre";

            updateButton();
            if (onModeChanged)
                onModeChanged (mode_);
        };
    }

    void setState (double rangeValue, const juce::String& mode)
    {
        editor_.setText (juce::String (rangeValue, 1), juce::dontSendNotification);
        mode_ = mode.trim().toLowerCase();
        if (mode_ != "pre" && mode_ != "post")
            mode_ = "mid";
        updateButton();
    }

    void setTextAppearance (juce::Colour colour, const juce::Font& font)
    {
        editor_.applyColourToAllText (colour, true);
        editor_.applyFontToAllText (font, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2, 1);
        modeBtn_.setBounds (r.removeFromLeft (44));
        r.removeFromLeft (4);
        editor_.setBounds (r);
    }

private:
    void updateButton()
    {
        juce::String modeName = "MID";
        juce::String tip = "Range mode: MID (window centered on trigger time)";
        if (mode_ == "pre")
        {
            modeName = "PRE";
            tip = "Range mode: PRE (window ends at trigger time)";
        }
        else if (mode_ == "post")
        {
            modeName = "POST";
            tip = "Range mode: POST (window starts at trigger time)";
        }

        juce::ignoreUnused (modeName);
        modeBtn_.setMode (mode_, tip);
    }

    RangeModeButton modeBtn_;
    juce::TextEditor editor_;
    juce::String mode_ { "mid" };
};

class InlineDeleteButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;
    InlineDeleteButtonCell() : juce::Button ("del")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat();
        const float side = juce::jmin (28.0f, juce::jmin (b.getWidth() - 6.0f, b.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (b.getCentre());
        paintFlatBtn (g, sq, isHovered, isDown);
        const float cx = sq.getCentreX();
        const float cy = sq.getCentreY();
        g.setColour (kFlatBtnIcon);
        g.fillRect (juce::Rectangle<float> (cx - 5.5f, cy - 1.0f, 11.0f, 2.0f));
    }
};

class InlineAddButtonCell final : public juce::Button
{
public:
    std::function<void()> onPress;
    InlineAddButtonCell() : juce::Button ("add")
    {
        onClick = [this] { if (onPress) onPress(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool isHovered, bool isDown) override
    {
        auto b = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
        const juce::Font font (juce::FontOptions (13.0f).withStyle ("Medium"));
        const auto textW = font.getStringWidthFloat ("+ Add");
        const auto buttonW = juce::jmin (b.getWidth(), juce::jmax (52.0f, textW + 18.0f));
        b = juce::Rectangle<float> (0.0f, 0.0f, buttonW, b.getHeight())
                .withPosition (6.0f, b.getY());  // left-aligned
        paintFlatBtn (g, b, isHovered, isDown);
        g.setColour (kFlatBtnIcon);
        g.setFont (font);
        g.drawFittedText ("+ Add", b.toNearestInt(), juce::Justification::centred, 1);
    }
};

class InlineDragHandleCell final : public juce::Component
{
public:
    std::function<void()> onDragStart;
    std::function<void(juce::Point<int>)> onDragMove;
    std::function<void(juce::Point<int>)> onDragEnd;

    InlineDragHandleCell()
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        juce::ignoreUnused (g);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragging_ = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging_)
        {
            dragging_ = true;
            if (onDragStart) onDragStart();
        }

        if (onDragMove) onDragMove (e.getScreenPosition());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging_)
            if (onDragEnd) onDragEnd (e.getScreenPosition());

        dragging_ = false;
    }

private:
    bool dragging_ { false };
};

class InlineExpandDragCell final : public juce::Component
{
public:
    std::function<void()> onToggle;
    std::function<void()> onDragStart;
    std::function<void(juce::Point<int>)> onDragMove;
    std::function<void(juce::Point<int>)> onDragEnd;

    void setExpanded (bool expanded) { expanded_ = expanded; repaint(); }

    InlineExpandDragCell()
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto arrowB = juce::Rectangle<float> (4.0f, 6.0f, 28.0f, (float) getHeight() - 12.0f).withSizeKeepingCentre (28.0f, 28.0f);
        g.setColour (juce::Colour::fromRGB (0x48, 0x48, 0x48));
        g.fillEllipse (arrowB);
        g.setColour (juce::Colour::fromRGB (0x5a, 0x5a, 0x5a));
        g.drawEllipse (arrowB, 1.0f);

        juce::Path p;
        const auto cx = arrowB.getCentreX();
        const auto cy = arrowB.getCentreY();
        const float s = 5.8f;
        if (expanded_)
        {
            p.startNewSubPath (cx - s, cy - 2.0f);
            p.lineTo (cx + s, cy - 2.0f);
            p.lineTo (cx, cy + s);
        }
        else
        {
            p.startNewSubPath (cx - 2.0f, cy - s);
            p.lineTo (cx - 2.0f, cy + s);
            p.lineTo (cx + s, cy);
        }
        p.closeSubPath();
        g.setColour (expanded_ ? juce::Colour::fromRGB (0xf2, 0xf2, 0xf2) : juce::Colour::fromRGB (0x8b, 0x8b, 0x8b));
        g.fillPath (p);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        dragging_ = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging_)
        {
            dragging_ = true;
            if (onDragStart) onDragStart();
        }

        if (onDragMove) onDragMove (e.getScreenPosition());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging_)
        {
            if (onDragEnd) onDragEnd (e.getScreenPosition());
        }
        else if (onToggle != nullptr)
        {
            onToggle();
        }

        dragging_ = false;
    }

private:
    bool expanded_ { false };
    bool dragging_ { false };
};


// Small "Set" button that uses the same font as the table row (14px)
struct SetButton final : juce::Button
{
    SetButton() : juce::Button ("set") {}
    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        paintFlatBtn (g, getLocalBounds().toFloat().reduced (0.5f), hovered, down);
        g.setColour (kFlatBtnIcon);
        g.setFont (juce::FontOptions (kTableRowFontSize));
        g.drawFittedText ("Set", getLocalBounds(), juce::Justification::centred, 1);
    }
};

inline juce::Component::SafePointer<CustomTriggerEditPopup>& activeCustomEditorPopup()
{
    static juce::Component::SafePointer<CustomTriggerEditPopup> popup;
    return popup;
}

// Custom trigger source selector: Col toggle (Col / L/C) + inline number fields
class InlineCustomTypeCell final : public juce::Component
{
public:
    std::function<void(const juce::String&, const juce::String&, const juce::String&, const juce::String&)> onChanged;

    InlineCustomTypeCell()
    {
        addAndMakeVisible (setBtn_);
        addAndMakeVisible (summaryLbl_);

        summaryLbl_.setFont (clipRowFont());
        summaryLbl_.setColour (juce::Label::textColourId, juce::Colour (0xff9a9a9a));
        summaryLbl_.setJustificationType (juce::Justification::centredLeft);

        setBtn_.onClick = [this] { openPopup(); };
    }

    void setState (const juce::String& type, const juce::String& col,
                   const juce::String& layer, const juce::String& clip,
                   const juce::String& clipName = {})
    {
        type_     = type;
        col_      = col;
        layer_    = layer;
        clip_     = clip;
        clipName_ = clipName;
        updateSummary();
    }

    void setFired (bool fired)
    {
        fired_ = fired;
        updateSummary();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2, 3);
        setBtn_.setBounds (r.removeFromLeft (36));
        r.removeFromLeft (4);
        summaryLbl_.setBounds (r);
    }

private:
    void updateSummary()
    {
        juce::String s;
        if (type_ == "gc")
            s = "Grp:" + layer_ + "  Col:" + col_;
        else if (type_ == "lc")
        {
            s = "Lyr:" + layer_;
            if (clip_.isNotEmpty()) s += "  Clp:" + clip_;
        }
        else if (col_.isNotEmpty())
            s = "Col:" + col_;
        summaryLbl_.setText (s.isEmpty() ? "(not set)" : s, juce::dontSendNotification);
        summaryLbl_.setColour (juce::Label::textColourId,
            fired_ ? kTextAmberDark
                   : (s.isEmpty() ? juce::Colour (0xff9e9e9e) : juce::Colour (0xffbcbcbc)));
    }

    void openPopup()
    {
        auto& activePopup = activeCustomEditorPopup();
        if (activePopup != nullptr) { activePopup->toFront (true); return; }
        popup_ = CustomTriggerEditPopup::show (
            clipName_.isNotEmpty() ? (juce::String (u8"Custom Trigger \u2014 ") + clipName_) : juce::String (u8"Custom Trigger \u2014 Source"),
            type_, col_, layer_, clip_,
            [safe = juce::Component::SafePointer<InlineCustomTypeCell> (this)]
            (const juce::String& t, const juce::String& c,
             const juce::String& l, const juce::String& cl)
            {
                if (safe == nullptr) return;
                safe->type_  = t; safe->col_ = c;
                safe->layer_ = l; safe->clip_ = cl;
                safe->updateSummary();
                if (safe->onChanged) safe->onChanged (t, c, l, cl);
            },
            getTopLevelComponent());
        activePopup = popup_;
    }

    SetButton    setBtn_;
    juce::Label  summaryLbl_;
    juce::String type_, col_, layer_, clip_, clipName_;
    juce::Component::SafePointer<CustomTriggerEditPopup> popup_;
    bool         fired_ { false };
};

class InlineEndActionCell final : public juce::Component
{
public:
    std::function<void(const juce::String&, const juce::String&, const juce::String&, const juce::String&)> onChanged;

    InlineEndActionCell()
    {
        addAndMakeVisible (setBtn_);
        addAndMakeVisible (infoLbl_);

        infoLbl_.setFont (clipRowFont());
        infoLbl_.setColour (juce::Label::textColourId, juce::Colour (0xff9e9e9e));
        infoLbl_.setJustificationType (juce::Justification::centredLeft);
        infoLbl_.setText ("(not set)", juce::dontSendNotification);

        setBtn_.onClick = [this] { openPopup(); };
    }

    void setState (const juce::String& mode, const juce::String& col,
                   const juce::String& layer, const juce::String& clip,
                   const juce::String& clipName = {})
    {
        type_     = mode;
        col_      = col;
        layer_    = layer;
        clip_     = clip;
        clipName_ = clipName;
        updateSummary();
    }

    void setFired (bool fired)
    {
        fired_ = fired;
        updateSummary();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2, 3);
        setBtn_.setBounds (r.removeFromLeft (36));
        r.removeFromLeft (4);
        infoLbl_.setBounds (r);
    }

private:
    void updateSummary()
    {
        juce::String s;
        if (type_ == "gc")
            s = "Grp:" + layer_ + "  Col:" + col_;
        else if (type_ == "lc")
        {
            s = "Lyr:" + layer_;
            if (clip_.isNotEmpty()) s += "  Clp:" + clip_;
        }
        else if (type_ == "col" && col_.isNotEmpty())
            s = "Col:" + col_;
        infoLbl_.setText (s.isEmpty() ? "(not set)" : s, juce::dontSendNotification);
        infoLbl_.setColour (juce::Label::textColourId,
            fired_ ? kTextAmberDark
                   : (s.isEmpty() ? juce::Colour (0xff9e9e9e) : juce::Colour (0xffbcbcbc)));
    }

    void openPopup()
    {
        auto& activePopup = activeCustomEditorPopup();
        if (activePopup != nullptr) { activePopup->toFront (true); return; }
        popup_ = CustomTriggerEditPopup::show (
            clipName_.isNotEmpty() ? (juce::String (u8"End Action \u2014 ") + clipName_) : juce::String (u8"End Action \u2014 Source"),
            type_, col_, layer_, clip_,
            [safe = juce::Component::SafePointer<InlineEndActionCell> (this)]
            (const juce::String& t, const juce::String& c,
             const juce::String& l, const juce::String& cl)
            {
                if (safe == nullptr) return;
                safe->type_  = t; safe->col_ = c;
                safe->layer_ = l; safe->clip_ = cl;
                safe->updateSummary();
                if (safe->onChanged) safe->onChanged (t, c, l, cl);
            },
            getTopLevelComponent());
        activePopup = popup_;
    }

    SetButton        setBtn_;
    juce::Label      infoLbl_;
    juce::String     type_, col_, layer_, clip_, clipName_;
    bool             fired_ { false };
    juce::Component::SafePointer<CustomTriggerEditPopup> popup_;
};
