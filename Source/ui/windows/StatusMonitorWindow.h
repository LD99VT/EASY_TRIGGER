#pragma once

#include <functional>

#include <juce_gui_extra/juce_gui_extra.h>

#include "../monitoring/OscLog.h"
#include "NativeWindowUtils.h"

namespace trigger
{
class StatusMonitorWindow final : public juce::DocumentWindow,
                                  private juce::Timer
{
public:
    using Getter = std::function<void (juce::Array<juce::String>&, juce::Array<juce::String>&)>;

    StatusMonitorWindow (Getter getter, OscLog* oscLog, juce::Component* relativeTo)
        : juce::DocumentWindow ("Trigger Status Monitor",
                                juce::Colour::fromRGB (0x1e, 0x1e, 0x1e),
                                juce::DocumentWindow::closeButton
                                | juce::DocumentWindow::minimiseButton),
          getter_ (std::move (getter)),
          oscLog_ (oscLog)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setContentOwned (new Content (*this, oscLog_), true);
        setResizeLimits (600, 480, 4000, 4000);
        centreWithSize (820, 720);
        if (relativeTo != nullptr)
        {
            const auto rc = relativeTo->getScreenBounds();
            setBounds (rc.getCentreX() - 410, rc.getCentreY() - 360, 820, 720);
        }
        setVisible (true);
#if JUCE_WINDOWS
        applyNativeDarkTitleBar (*this);
        if (auto* hwnd = (HWND) getWindowHandle())
        {
            ::SendMessageW (hwnd, WM_SETICON, 0, 0);
            ::SendMessageW (hwnd, WM_SETICON, 1, 0);
        }
#endif
        toFront (true);
        startTimerHz (5);
    }

    void closeButtonPressed() override { delete this; }

    void timerCallback() override
    {
        if (auto* c = dynamic_cast<Content*> (getContentComponent()))
            c->refresh();
    }

    void getValues (juce::Array<juce::String>& keys, juce::Array<juce::String>& vals)
    {
        getter_ (keys, vals);
    }

private:
    Getter getter_;
    OscLog* oscLog_ = nullptr;

    static constexpr int kRows = 22;
    static constexpr int kLeftRows = 7;

    struct Content final : juce::Component, juce::ListBoxModel
    {
        StatusMonitorWindow& win_;
        OscLog* oscLog_ = nullptr;

        juce::Label leftHeader_  { {}, "Timecode / Playback" };
        juce::Label rightHeader_ { {}, "OSC / Network" };
        juce::Label keyLbls_[kRows];
        juce::Label valLbls_[kRows];
        juce::TextButton ok_ { "OK" };
        juce::Label oscLogHeaderLbl_ { {}, "OSC Console" };
        juce::TextButton clearLogBtn_ { "Clear" };
        juce::Label colDir_  { {}, "DIR" };
        juce::Label colTime_ { {}, "TIME" };
        juce::Label colIp_   { {}, "IP : PORT" };
        juce::Label colAddr_ { {}, "ADDRESS" };
        juce::Label colType_ { {}, "TYPE" };
        juce::Label colVal_  { {}, "VALUE" };
        juce::ListBox logList_;
        std::vector<OscLogEntry> logSnapshot_;

        static constexpr int kColDir  = 42;
        static constexpr int kColTime = 92;
        static constexpr int kColIp   = 134;
        static constexpr int kColAddr = 274;
        static constexpr int kColType = 48;

        explicit Content (StatusMonitorWindow& w, OscLog* log)
            : win_ (w), oscLog_ (log)
        {
            const juce::Colour keyCol = juce::Colour::fromRGB (0x84, 0x84, 0x84);
            const juce::Colour valCol = juce::Colour::fromRGB (0xe0, 0xe0, 0xe0);
            const juce::Colour headerCol = juce::Colour::fromRGB (0xac, 0xac, 0xac);

            for (auto* lbl : { &leftHeader_, &rightHeader_ })
            {
                lbl->setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
                lbl->setColour (juce::Label::textColourId, headerCol);
                lbl->setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (*lbl);
            }

            for (int i = 0; i < kRows; ++i)
            {
                keyLbls_[i].setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
                keyLbls_[i].setColour (juce::Label::textColourId, keyCol);
                keyLbls_[i].setJustificationType (juce::Justification::centredRight);
                addAndMakeVisible (keyLbls_[i]);

                valLbls_[i].setFont (juce::FontOptions (12.5f));
                valLbls_[i].setColour (juce::Label::textColourId, valCol);
                valLbls_[i].setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (valLbls_[i]);
            }

            for (auto* b : { &ok_ })
            {
                b->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0x4a, 0x4a));
                b->setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
                b->setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (0xe4, 0xe4, 0xe4));
            }
            ok_.onClick = [this] { juce::MessageManager::callAsync ([w = &win_] { delete w; }); };
            addAndMakeVisible (ok_);

            oscLogHeaderLbl_.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
            oscLogHeaderLbl_.setColour (juce::Label::textColourId, juce::Colour::fromRGB (0x88, 0x88, 0x88));
            addAndMakeVisible (oscLogHeaderLbl_);

            clearLogBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
            clearLogBtn_.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x3a, 0x3a, 0x3a));
            clearLogBtn_.setColour (juce::TextButton::textColourOffId, juce::Colour::fromRGB (0xb0, 0xb0, 0xb0));
            clearLogBtn_.setColour (juce::TextButton::textColourOnId, juce::Colour::fromRGB (0xb0, 0xb0, 0xb0));
            clearLogBtn_.onClick = [this]
            {
                if (oscLog_ != nullptr) oscLog_->clear();
                logSnapshot_.clear();
                logList_.updateContent();
            };
            addAndMakeVisible (clearLogBtn_);

            const juce::Colour hdrCol = juce::Colour::fromRGB (0x58, 0x58, 0x58);
            for (auto* lbl : { &colDir_, &colTime_, &colIp_, &colAddr_, &colType_, &colVal_ })
            {
                lbl->setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
                lbl->setColour (juce::Label::textColourId, hdrCol);
                lbl->setJustificationType (juce::Justification::centredLeft);
                addAndMakeVisible (*lbl);
            }
            colDir_.setJustificationType (juce::Justification::centred);
            colType_.setJustificationType (juce::Justification::centred);

            logList_.setModel (this);
            logList_.setRowHeight (22);
            logList_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff0f0f0f));
            logList_.setOutlineThickness (1);
            logList_.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff333333));
            addAndMakeVisible (logList_);

            refresh();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (0x17, 0x17, 0x17));

            constexpr int kPad = 14;
            constexpr int kTopHeaderH = 22;
            constexpr int kRowH = 24;
            const int topSectionBottom = kPad + kTopHeaderH + 10 + juce::jmax (kLeftRows, kRows - kLeftRows) * kRowH + 10;
            const int sepY = topSectionBottom + 4;
            g.setColour (juce::Colour (0xff2e2e2e));
            g.fillRect (kPad, sepY, getWidth() - kPad * 2, 1);

            const int midX = getWidth() / 2;
            g.fillRect (midX, kPad, 1, topSectionBottom - kPad - 2);
        }

        void resized() override
        {
            constexpr int kPad = 14;
            constexpr int kRowH = 24;
            constexpr int kGap = 10;
            constexpr int kTopHeaderH = 22;
            constexpr int kColumnGap = 18;
            const int columnW = (getWidth() - kPad * 2 - kColumnGap) / 2;
            const int keyW = 116;
            const int valW = columnW - keyW - kGap;
            const int leftX = kPad;
            const int rightX = kPad + columnW + kColumnGap;

            leftHeader_.setBounds (leftX, kPad, columnW, kTopHeaderH);
            rightHeader_.setBounds (rightX, kPad, columnW, kTopHeaderH);

            for (int i = 0; i < kLeftRows; ++i)
            {
                const int y = kPad + kTopHeaderH + 10 + i * kRowH;
                keyLbls_[i].setBounds (leftX, y, keyW, kRowH);
                valLbls_[i].setBounds (leftX + keyW + kGap, y, valW, kRowH);
            }

            for (int i = kLeftRows; i < kRows; ++i)
            {
                const int y = kPad + kTopHeaderH + 10 + (i - kLeftRows) * kRowH;
                keyLbls_[i].setBounds (rightX, y, keyW, kRowH);
                valLbls_[i].setBounds (rightX + keyW + kGap, y, valW, kRowH);
            }

            const int statsEndY = kPad + kTopHeaderH + 10 + juce::jmax (kLeftRows, kRows - kLeftRows) * kRowH + 10;
            const int oscHdrY = statsEndY + 9;

            oscLogHeaderLbl_.setBounds (kPad, oscHdrY, 120, 22);
            clearLogBtn_.setBounds (getWidth() - kPad - 64, oscHdrY, 64, 22);

            const int colLblY = oscHdrY + 25;
            int cx = kPad;
            colDir_.setBounds (cx, colLblY, kColDir, 18); cx += kColDir;
            colTime_.setBounds (cx, colLblY, kColTime, 18); cx += kColTime;
            colIp_.setBounds (cx, colLblY, kColIp, 18); cx += kColIp;
            colAddr_.setBounds (cx, colLblY, kColAddr, 18); cx += kColAddr;
            colType_.setBounds (cx, colLblY, kColType, 18); cx += kColType;
            colVal_.setBounds (cx, colLblY, getWidth() - cx - kPad, 18);

            const int logY = colLblY + 20;
            const int okY = getHeight() - kPad - 32;
            const int minLogH = 180;
            const int logH = juce::jmax (minLogH, okY - logY - 6);
            logList_.setBounds (kPad, logY, getWidth() - kPad * 2, logH);
            ok_.setBounds ((getWidth() - 100) / 2, okY, 100, 32);
        }

        int getNumRows() override { return (int) logSnapshot_.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (row < 0 || row >= (int) logSnapshot_.size())
                return;

            const auto& e = logSnapshot_[row];
            const bool isIn = (e.dir == OscLogEntry::Dir::input);

            const juce::Colour bg = (row % 2 == 0)
                ? (isIn ? juce::Colour (0xff0e1e1c) : juce::Colour (0xff1a100a))
                : (isIn ? juce::Colour (0xff122422) : juce::Colour (0xff1e140e));
            g.fillAll (bg);

            const juce::Colour dirCol = isIn ? juce::Colour (0xff4cbf98) : juce::Colour (0xffcc8844);
            const juce::Colour mainCol = isIn ? juce::Colour (0xff5cd0a8) : juce::Colour (0xffddaa66);
            const juce::Colour dimCol { 0xff888888 };
            const juce::Colour valCol { 0xffe0ddd8 };

            g.setFont (juce::FontOptions (13.0f));
            constexpr int kPad = 14;
            const int ty = (h - 16) / 2;

            int cx = kPad;
            auto drawCol = [&] (const juce::String& text, int colW, juce::Colour col,
                                juce::Justification just = juce::Justification::centredLeft)
            {
                g.setColour (col);
                g.drawText (text, cx + 2, ty, colW - 4, 16, just, true);
                cx += colW;
            };

            drawCol (isIn ? "IN" : "OUT", kColDir, dirCol, juce::Justification::centred);

            const juce::int64 ts = e.timestampMs;
            const int hh = (int) ((ts / 3'600'000) % 24);
            const int mm = (int) ((ts / 60'000) % 60);
            const int ss = (int) ((ts / 1'000) % 60);
            const int ms = (int) (ts % 1000);
            const auto timeStr = (hh < 10 ? juce::String ("0") : juce::String()) + juce::String (hh) + ":"
                               + (mm < 10 ? juce::String ("0") : juce::String()) + juce::String (mm) + ":"
                               + (ss < 10 ? juce::String ("0") : juce::String()) + juce::String (ss) + "."
                               + (ms < 100 ? (ms < 10 ? juce::String ("00") : juce::String ("0")) : juce::String()) + juce::String (ms);
            drawCol (timeStr, kColTime, dimCol);
            drawCol (e.ip + ":" + juce::String (e.port), kColIp, dimCol);
            drawCol (e.address, kColAddr, mainCol);
            drawCol (e.type, kColType, dimCol, juce::Justification::centred);
            drawCol (e.value, w - cx - kPad, valCol);
        }

        void refresh()
        {
            juce::Array<juce::String> keys, vals;
            win_.getValues (keys, vals);
            for (int i = 0; i < kRows; ++i)
            {
                const auto key = i < keys.size() ? keys[i] : juce::String();
                const auto val = i < vals.size() ? vals[i] : juce::String();
                keyLbls_[i].setText (key, juce::dontSendNotification);
                valLbls_[i].setText (val, juce::dontSendNotification);
            }

            if (oscLog_ != nullptr)
            {
                const auto newSnap = oscLog_->snapshot();
                const bool newEntries = (newSnap.size() != logSnapshot_.size());
                logSnapshot_ = newSnap;
                logList_.updateContent();
                if (newEntries && ! logSnapshot_.empty())
                    logList_.scrollToEnsureRowIsOnscreen ((int) logSnapshot_.size() - 1);
            }
        }
    };
};
} // namespace trigger
