#include "OscInput.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace bridge::engine
{
OscInput::OscInput()
    : juce::Thread ("OSC Input")
{
}

OscInput::~OscInput()
{
    stop();
}

bool OscInput::start (int port, juce::String bindIp, FrameRate fps, juce::String addrStr, juce::String addrFloat, OscValueType floatValueType, double floatMaxSeconds, juce::String& errorOut)
{
    stop();

    bindIp_ = bindIp.trim();
    if (bindIp_.isEmpty())
        bindIp_ = "0.0.0.0";

    fps_.store (fps, std::memory_order_relaxed);
    // Use the addresses exactly as configured in the UI. If the user clears
    // a field, it stays empty and that message type is effectively disabled.
    addrStr_ = addrStr.trim();
    addrFloat_ = addrFloat.trim();

    floatValueType_.store ((int) floatValueType, std::memory_order_relaxed);
    floatMaxSeconds_.store (floatMaxSeconds, std::memory_order_relaxed);
    lastPacketTsMs_.store (0.0, std::memory_order_relaxed);
    lastStringTsMs_.store (0.0, std::memory_order_relaxed);
    bindFellBack_.store (false, std::memory_order_relaxed);

    socket_ = std::make_unique<juce::DatagramSocket> (false);
    bool bound = false;
    bool fellBack = false;

    if (bindIp_ != "0.0.0.0")
        bound = socket_->bindToPort (port, bindIp_);
    if (! bound)
    {
        bound = socket_->bindToPort (port);
        if (bound)
        {
            fellBack = (bindIp_ != "0.0.0.0");
            bindIp_ = "0.0.0.0";
        }
    }

    if (! bound)
    {
        socket_.reset();
        errorOut = "Failed to bind OSC " + bindIp_ + ":" + juce::String (port);
        return false;
    }

    bindFellBack_.store (fellBack, std::memory_order_relaxed);
    running_.store (true, std::memory_order_relaxed);
    startThread();
    errorOut.clear();
    return true;
}

void OscInput::stop()
{
    running_.store (false, std::memory_order_relaxed);
    if (socket_ != nullptr)
        socket_->shutdown();
    if (isThreadRunning())
        stopThread (1000);
    socket_.reset();
    bindFellBack_.store (false, std::memory_order_relaxed);
}

bool OscInput::getIsRunning() const
{
    return running_.load (std::memory_order_relaxed);
}

bool OscInput::isReceiving() const
{
    const auto last = lastPacketTsMs_.load (std::memory_order_relaxed);
    if (last <= 0.0)
        return false;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    return (now - last) < kSourceTimeoutMs;
}

Timecode OscInput::getCurrentTimecode() const
{
    return unpackTimecode (packedTc_.load (std::memory_order_relaxed));
}

FrameRate OscInput::getDetectedFrameRate() const
{
    return fps_.load (std::memory_order_relaxed);
}

void OscInput::run()
{
    uint8_t packet[8192];

    while (! threadShouldExit() && running_.load (std::memory_order_relaxed) && socket_ != nullptr)
    {
        if (! socket_->waitUntilReady (true, 100))
            continue;

        const int bytesRead = socket_->read (packet, (int) sizeof (packet), false);
        if (bytesRead <= 0)
            continue;

        parsePacket (packet, bytesRead, 0);
    }
}

bool OscInput::readBE32 (const uint8_t* data, int size, int& offset, uint32_t& out)
{
    if (offset + 4 > size)
        return false;
    out = ((uint32_t) data[offset] << 24)
        | ((uint32_t) data[offset + 1] << 16)
        | ((uint32_t) data[offset + 2] << 8)
        | (uint32_t) data[offset + 3];
    offset += 4;
    return true;
}

bool OscInput::readBE64 (const uint8_t* data, int size, int& offset, uint64_t& out)
{
    if (offset + 8 > size)
        return false;
    out = ((uint64_t) data[offset] << 56)
        | ((uint64_t) data[offset + 1] << 48)
        | ((uint64_t) data[offset + 2] << 40)
        | ((uint64_t) data[offset + 3] << 32)
        | ((uint64_t) data[offset + 4] << 24)
        | ((uint64_t) data[offset + 5] << 16)
        | ((uint64_t) data[offset + 6] << 8)
        | (uint64_t) data[offset + 7];
    offset += 8;
    return true;
}

bool OscInput::readPaddedString (const uint8_t* data, int size, int& offset, juce::String& out)
{
    if (offset >= size)
        return false;

    int zeroPos = offset;
    while (zeroPos < size && data[zeroPos] != 0)
        ++zeroPos;
    if (zeroPos >= size)
        return false;

    out = juce::String::fromUTF8 ((const char*) (data + offset), zeroPos - offset);
    zeroPos += 1;
    while ((zeroPos % 4) != 0)
        ++zeroPos;
    if (zeroPos > size)
        return false;
    offset = zeroPos;
    return true;
}

bool OscInput::skipBlob (const uint8_t* data, int size, int& offset)
{
    uint32_t len = 0;
    if (! readBE32 (data, size, offset, len))
        return false;
    if ((int) len < 0 || offset + (int) len > size)
        return false;
    offset += (int) len;
    while ((offset % 4) != 0)
        ++offset;
    return offset <= size;
}

bool OscInput::parsePacket (const uint8_t* data, int size, int depth)
{
    if (depth > 6 || size <= 0)
        return false;

    if (size >= 8 && std::memcmp (data, "#bundle", 7) == 0)
    {
        int offset = 8;
        uint64_t timetag = 0;
        if (! readBE64 (data, size, offset, timetag))
            return false;

        bool any = false;
        while (offset + 4 <= size)
        {
            uint32_t elemSize = 0;
            if (! readBE32 (data, size, offset, elemSize))
                break;
            if (elemSize == 0 || offset + (int) elemSize > size)
                break;
            any = parsePacket (data + offset, (int) elemSize, depth + 1) || any;
            offset += (int) elemSize;
        }
        return any;
    }

    return parseMessage (data, size);
}

bool OscInput::parseMessage (const uint8_t* data, int size)
{
    int offset = 0;
    juce::String address;
    if (! readPaddedString (data, size, offset, address))
        return false;
    if (! address.startsWithChar ('/'))
        return false;

    juce::String types;
    if (! readPaddedString (data, size, offset, types))
        return false;
    if (! types.startsWithChar (','))
        return false;

    bool hasString = false;
    juce::String firstString;
    bool hasNumber = false;
    double firstNumber = 0.0;

    for (int i = 1; i < types.length(); ++i)
    {
        const juce_wchar t = types[i];
        switch (t)
        {
            case 'i':
            {
                uint32_t raw = 0;
                if (! readBE32 (data, size, offset, raw))
                    return false;
                if (! hasNumber)
                {
                    firstNumber = (double) (int32_t) raw;
                    hasNumber = true;
                }
                break;
            }
            case 'f':
            {
                uint32_t raw = 0;
                if (! readBE32 (data, size, offset, raw))
                    return false;
                float v = 0.0f;
                std::memcpy (&v, &raw, sizeof (v));
                if (! hasNumber)
                {
                    firstNumber = (double) v;
                    hasNumber = true;
                }
                break;
            }
            case 'd':
            {
                uint64_t raw = 0;
                if (! readBE64 (data, size, offset, raw))
                    return false;
                double v = 0.0;
                std::memcpy (&v, &raw, sizeof (v));
                if (! hasNumber)
                {
                    firstNumber = v;
                    hasNumber = true;
                }
                break;
            }
            case 'h':
            {
                uint64_t raw = 0;
                if (! readBE64 (data, size, offset, raw))
                    return false;
                if (! hasNumber)
                {
                    firstNumber = (double) (int64_t) raw;
                    hasNumber = true;
                }
                break;
            }
            case 's':
            {
                juce::String s;
                if (! readPaddedString (data, size, offset, s))
                    return false;
                if (! hasString)
                {
                    firstString = s;
                    hasString = true;
                }
                break;
            }
            case 'b':
            {
                if (! skipBlob (data, size, offset))
                    return false;
                break;
            }
            case 't':
            {
                uint64_t ignored = 0;
                if (! readBE64 (data, size, offset, ignored))
                    return false;
                break;
            }
            case 'T':
            case 'F':
            case 'N':
            case 'I':
                break;
            default:
                return false;
        }
    }

    return processDecodedMessage (address, hasString, firstString, hasNumber, firstNumber);
}

bool OscInput::processDecodedMessage (const juce::String& address, bool hasString, const juce::String& stringArg, bool hasNumber, double numberArg)
{
    if (address == addrStr_)
    {
        if (hasString)
        {
            parseStringTc (stringArg.trim(), true);
            return true;
        }
        if (hasNumber && ! isStringSuppressed())
        {
            parseFloatTime (numberArg);
            return true;
        }
        return false;
    }

    if (address == addrFloat_)
    {
        if (isStringSuppressed())
            return false;
        if (hasNumber)
        {
            parseFloatTime (numberArg);
            return true;
        }
        if (hasString)
        {
            const auto s = stringArg.trim();
            if (s.containsAnyOf (":;"))
            {
                parseStringTc (s, true);
                return true;
            }
            if (looksNumeric (s))
            {
                parseFloatTime (s.getDoubleValue());
                return true;
            }
        }
        return false;
    }

    // No heuristic matching by substring: if the OSC address does not
    // exactly match either the string or float address configured in the
    // UI, this message is ignored.
    return false;
}

bool OscInput::looksNumeric (const juce::String& s)
{
    const auto t = s.trim();
    if (! t.containsAnyOf ("0123456789"))
        return false;
    return t.containsOnly ("0123456789+-.eE");
}

void OscInput::parseStringTc (juce::String text, bool rememberStringTs)
{
    text = text.trim().replaceCharacter (';', ':');
    const auto parts = juce::StringArray::fromTokens (text, ":", "");

    if (parts.size() >= 4)
    {
        const int h = parts[0].getIntValue();
        const int m = parts[1].getIntValue();
        const int s = parts[2].getIntValue();
        const int f = parts[3].getIntValue();
        commit (h, m, s, f, rememberStringTs);
        return;
    }

    const double fps = frameRateToDouble (fps_.load (std::memory_order_relaxed));
    const int fpsInt = juce::jmax (1, (int) std::llround (fps));
    int h = 0, m = 0;
    double secf = 0.0;

    if (parts.size() == 3)
    {
        h = parts[0].getIntValue();
        m = parts[1].getIntValue();
        secf = parts[2].getDoubleValue();
    }
    else if (parts.size() == 2)
    {
        m = parts[0].getIntValue();
        secf = parts[1].getDoubleValue();
    }
    else if (parts.size() == 1)
    {
        secf = parts[0].getDoubleValue();
    }
    else
    {
        return;
    }

    const int s = ((int) secf) % 60;
    int f = (int) std::llround ((secf - std::floor (secf)) * (double) fpsInt);
    f = juce::jlimit (0, fpsInt - 1, f);
    commit (h, m, s, f, rememberStringTs);
}

void OscInput::parseFloatTime (double t)
{
    const auto vt = static_cast<OscValueType> (floatValueType_.load (std::memory_order_relaxed));
    if (vt == OscValueType::Frames)
    {
        const double fpsd = frameRateToDouble (fps_.load (std::memory_order_relaxed));
        t = (fpsd > 0.0) ? t / fpsd : t;
    }
    else if (vt == OscValueType::Normalized)
    {
        t = t * floatMaxSeconds_.load (std::memory_order_relaxed);
    }

    const auto fps = frameRateToDouble (fps_.load (std::memory_order_relaxed));
    const int fpsInt = juce::jmax (1, frameRateToInt (fps_.load (std::memory_order_relaxed)));
    const auto totalFrames = (int64_t) std::llround (juce::jmax (0.0, t) * fps);

    const int frames = (int) (totalFrames % fpsInt);
    const auto totalSec = (int64_t) juce::jmax (0.0, t);
    const int s = (int) (totalSec % 60);
    const int m = (int) ((totalSec / 60) % 60);
    const int h = (int) ((totalSec / 3600) % 24);
    commit (h, m, s, juce::jmax (0, frames), false);
}

void OscInput::commit (int h, int m, int s, int f, bool rememberStringTs)
{
    const auto fpsInt = juce::jmax (1, frameRateToInt (fps_.load (std::memory_order_relaxed)));
    h = ((h % 24) + 24) % 24;
    m = ((m % 60) + 60) % 60;
    s = ((s % 60) + 60) % 60;
    f = juce::jlimit (0, fpsInt - 1, f);

    packedTc_.store (packTimecode (h, m, s, f), std::memory_order_relaxed);
    const auto now = juce::Time::getMillisecondCounterHiRes();
    lastPacketTsMs_.store (now, std::memory_order_relaxed);
    if (rememberStringTs)
        lastStringTsMs_.store (now, std::memory_order_relaxed);
}

bool OscInput::isStringSuppressed() const
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto lastStr = lastStringTsMs_.load (std::memory_order_relaxed);
    return (now - lastStr) < 350.0;
}
} // namespace bridge::engine

