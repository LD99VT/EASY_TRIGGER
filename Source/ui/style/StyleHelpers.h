#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "TriggerColours.h"

namespace trigger
{
// ─── Shared control styling functions ─────────────────────────────────────────

inline void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId, kInput);
    c.setColour (juce::ComboBox::outlineColourId,    kRow);
    c.setColour (juce::ComboBox::textColourId,       kTextMuted);
}

inline void styleEditor (juce::TextEditor& e)
{
    e.setColour (juce::TextEditor::backgroundColourId,     kInput);
    e.setColour (juce::TextEditor::outlineColourId,        kRow);
    e.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB (0x56, 0x5f, 0x6b));
    e.setColour (juce::TextEditor::textColourId,           kTextMuted);
    e.setJustification (juce::Justification::centredLeft);
    e.setIndents (8, 2);
}

inline void styleSlider (juce::Slider& s, bool dbStyle)
{
    s.setColour (juce::Slider::backgroundColourId,        juce::Colour::fromRGB (0x20, 0x20, 0x20));
    s.setColour (juce::Slider::trackColourId,             dbStyle ? kTeal : juce::Colour::fromRGB (0x1f, 0x3b, 0x45));
    s.setColour (juce::Slider::thumbColourId,             juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId,       kTextMuted);
    s.setColour (juce::Slider::textBoxOutlineColourId,    kRow);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kInput);
}

inline void styleFlatMenuButton (juce::TextButton& b)
{
    b.getProperties().set ("flatMenuButton", true);
    b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    b.setColour (juce::TextButton::buttonOnColourId, kMenuHover);
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

inline void styleRowButton (juce::TextButton& b, juce::Colour fill = kRow)
{
    b.setColour (juce::TextButton::buttonColourId, fill);
    b.setColour (juce::TextButton::buttonOnColourId, fill);
    b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

inline void styleSectionLabel (juce::Label& l)
{
    l.setColour (juce::Label::backgroundColourId, kRow);
    l.setColour (juce::Label::textColourId, kTextSecondary);
    l.setFont (juce::FontOptions (kSectionHeaderFontSize));
}

inline void styleValueLabel (juce::Label& l)
{
    l.setColour (juce::Label::textColourId, kTextMuted);
}

// ─── Flat button unified style ─────────────────────────────────────────────────
inline const juce::Colour kFlatBtnBg   { 0xff383838 };
inline const juce::Colour kFlatBtnHover{ 0xff505050 };
inline const juce::Colour kFlatBtnDown { 0xff252525 };
inline const juce::Colour kFlatBtnIcon { 0xffe8e8e8 };

// Paint helper — call from any paintButton() override.
// Fills background with unified colors, no border.
inline void paintFlatBtn (juce::Graphics& g,
                           juce::Rectangle<float> b,
                           bool hovered, bool down,
                           float radius = 5.0f)
{
    g.setColour (down ? kFlatBtnDown : (hovered ? kFlatBtnHover : kFlatBtnBg));
    g.fillRoundedRectangle (b, radius);
}

// LookAndFeel for juce::TextButton — flat, no border, unified colors.
struct FlatButtonLAF final : juce::LookAndFeel_V4
{
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                                const juce::Colour&, bool hovered, bool down) override
    {
        paintFlatBtn (g, btn.getLocalBounds().toFloat().reduced (0.5f), hovered, down);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                          bool /*hovered*/, bool /*down*/) override
    {
        g.setColour (kFlatBtnIcon);
        g.setFont (juce::FontOptions (13.0f).withStyle ("Medium"));
        g.drawFittedText (btn.getButtonText(),
                          btn.getLocalBounds(), juce::Justification::centred, 1);
    }
};

// ─── Flat icon text button (+ / -)  ─────────────────────────────────────────
struct FlatIconButton final : juce::Button
{
    explicit FlatIconButton (const juce::String& text)
        : juce::Button (text), label_ (text)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? kInput.darker (0.15f) : (hovered ? kInput.brighter (0.18f) : kInput));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (kFlatBtnIcon);
        g.setFont (juce::FontOptions (14.0f).withStyle ("Medium"));
        g.drawFittedText (label_, getLocalBounds(), juce::Justification::centred, 1);
    }
private:
    juce::String label_;
};

} // namespace trigger
