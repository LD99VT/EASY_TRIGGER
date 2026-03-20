#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "TriggerColours.h"

namespace trigger
{
class TriggerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    int getDefaultScrollbarWidth() override { return 8; }

    void drawPopupMenuBackground (juce::Graphics& g, int, int) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    }

    void drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                             int width,
                                             int height,
                                             const juce::PopupMenu::Options&) override
    {
        drawPopupMenuBackground (g, width, height);
    }

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override
    {
        juce::ignoreUnused (icon, shortcutKeyText);

        if (isSeparator)
        {
            auto r = area.reduced (10, area.getHeight() / 2);
            g.setColour (kControlOutline);
            g.drawLine ((float) r.getX(), (float) r.getY(), (float) r.getRight(), (float) r.getY(), 1.0f);
            return;
        }

        auto r = area.reduced (2, 0);
        g.setColour (isHighlighted ? findColour (juce::PopupMenu::highlightedBackgroundColourId)
                                   : findColour (juce::PopupMenu::backgroundColourId));
        g.fillRect (r);

        juce::Colour colour = textColour != nullptr ? *textColour
                                                    : findColour (isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                                                                : juce::PopupMenu::textColourId);
        if (! isActive)
            colour = colour.withAlpha (0.45f);

        if (isTicked)
        {
            auto dot = juce::Rectangle<float> ((float) r.getX() + 8.0f, (float) r.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (colour);
            g.fillEllipse (dot);
        }

        g.setColour (colour);
        g.setFont (getPopupMenuFont());
        g.drawText (text, r.withTrimmedLeft (24).withTrimmedRight (hasSubMenu ? 18 : 8), juce::Justification::centredLeft, true);

        if (hasSubMenu)
        {
            juce::Path p;
            const float cx = (float) r.getRight() - 9.0f;
            const float cy = (float) r.getCentreY();
            p.startNewSubPath (cx - 3.0f, cy - 4.0f);
            p.lineTo (cx + 1.5f, cy);
            p.lineTo (cx - 3.0f, cy + 4.0f);
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
    }

    void drawPopupMenuItemWithOptions (juce::Graphics& g,
                                       const juce::Rectangle<int>& area,
                                       bool isHighlighted,
                                       const juce::PopupMenu::Item& item,
                                       const juce::PopupMenu::Options&) override
    {
        drawPopupMenuItem (g,
                           area,
                           item.isSeparator,
                           item.isEnabled,
                           isHighlighted,
                           item.isTicked,
                           item.subMenu != nullptr,
                           item.text,
                           item.shortcutKeyDescription,
                           item.image.get(),
                           item.colour.isTransparent() ? nullptr : &item.colour);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        g.setColour (findColour (juce::PopupMenu::backgroundColourId));
        g.fillRect (area);
        g.setColour (findColour (juce::PopupMenu::headerTextColourId));
        g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        g.drawText (sectionName, area.withTrimmedLeft (10), juce::Justification::centredLeft, true);
    }

    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g,
                                                const juce::Rectangle<int>& area,
                                                const juce::String& sectionName,
                                                const juce::PopupMenu::Options&) override
    {
        drawPopupMenuSectionHeader (g, area, sectionName);
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton,
                               bool isButtonDown) override
    {
        juce::ignoreUnused (backgroundColour);
        auto bounds = button.getLocalBounds().toFloat();
        auto c = button.findColour (juce::TextButton::buttonColourId);
        const auto text = button.getButtonText().trim();
        const bool isFlatMenuButton = button.getProperties().contains ("flatMenuButton");
        const bool isPlusMinus = (text == "+" || text == "-");
        if (isPlusMinus)
        {
            const float side = juce::jmin (28.0f, juce::jmin (bounds.getWidth() - 6.0f, bounds.getHeight() - 6.0f));
            bounds = juce::Rectangle<float> (0, 0, side, side).withCentre (bounds.getCentre());
            c = kControlFill;
            if (isButtonDown)
                c = c.darker (0.15f);
            else if (isMouseOverButton)
                c = c.brighter (0.12f);
            g.setColour (c);
            g.fillRoundedRectangle (bounds, 5.0f);
            return;
        }

        if (isFlatMenuButton)
        {
            if (isMouseOverButton || isButtonDown)
            {
                const auto fill = isButtonDown ? kMenuPressed : kMenuHover;
                g.setColour (fill);
                g.fillRoundedRectangle (bounds.reduced (0.5f), 5.0f);
            }
            return;
        }

        bounds = bounds.reduced (0.5f);
        if (isButtonDown)
            c = c.darker (0.15f);
        else if (isMouseOverButton)
            c = c.brighter (0.08f);

        g.setColour (c);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (kRowOutline);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool isMouseOverButton,
                         bool isButtonDown) override
    {
        const auto text = button.getButtonText().trim();
        const bool isPlusMinus = (text == "+" || text == "-");
        if (! isPlusMinus)
        {
            juce::LookAndFeel_V4::drawButtonText (g, button, isMouseOverButton, isButtonDown);
            return;
        }

        auto bounds = button.getLocalBounds().toFloat();
        const float side = juce::jmin (28.0f, juce::jmin (bounds.getWidth() - 6.0f, bounds.getHeight() - 6.0f));
        auto sq = juce::Rectangle<float> (0, 0, side, side).withCentre (bounds.getCentre());
        const float cx = sq.getCentreX();
        const float cy = sq.getCentreY();
        auto icon = kControlIcon;
        if (isMouseOverButton || isButtonDown)
            icon = icon.brighter (0.4f);

        g.setColour (icon);
        g.fillRect (juce::Rectangle<float> (cx - 5.5f, cy - 1.0f, 11.0f, 2.0f));
        if (text == "+")
            g.fillRect (juce::Rectangle<float> (cx - 1.0f, cy - 5.5f, 2.0f, 11.0f));
    }

    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool,
                       int,
                       int,
                       int,
                       int,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        juce::Path p;
        const float cx = (float) width - 14.0f;
        const float cy = (float) height * 0.5f;
        p.startNewSubPath (cx - 5.0f, cy - 2.0f);
        p.lineTo (cx, cy + 3.0f);
        p.lineTo (cx + 5.0f, cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (box.getLocalBounds().reduced (8, 1));
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        const auto c = editor.hasKeyboardFocus (true)
                           ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                           : editor.findColour (juce::TextEditor::outlineColourId);
        g.setColour (c);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        const auto bg = label.findColour (juce::Label::backgroundColourId);
        if (bg.getAlpha() > 0)
        {
            g.setColour (bg);
            g.fillRoundedRectangle (label.getLocalBounds().toFloat(), 4.0f);
        }

        if (! label.isBeingEdited())
        {
            const auto tc = label.findColour (juce::Label::textColourId);
            if (tc.getAlpha() > 0)
            {
                const auto textArea = label.getBorderSize().subtractedFrom (label.getLocalBounds());
                g.setFont (label.getFont());
                g.setColour (tc);
                g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                                  juce::jmax (1, (int) ((float) textArea.getHeight() / label.getFont().getHeight())),
                                  label.getMinimumHorizontalScale());
            }
        }

        const auto oc = label.findColour (label.isBeingEdited() ? juce::Label::outlineWhenEditingColourId
                                                                 : juce::Label::outlineColourId);
        if (oc.getAlpha() > 0)
        {
            g.setColour (oc);
            g.drawRoundedRectangle (label.getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
        }
    }
};
} // namespace trigger
