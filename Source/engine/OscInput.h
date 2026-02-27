#pragma once

#include <JuceHeader.h>

#include "vendor/timecode/EngineTimecodeCore.h"

#include <atomic>

namespace bridge::engine
{
class OscInput final : private juce::Thread
{
public:
    OscInput();
    ~OscInput() override;

    bool start (int port, juce::String bindIp, FrameRate fps, juce::String addrStr, juce::String addrFloat, juce::String& errorOut);
    void stop();

    bool getIsRunning() const;
    bool isReceiving() const;
    Timecode getCurrentTimecode() const;
    FrameRate getDetectedFrameRate() const;

private:
    void run() override;

    static bool readBE32 (const uint8_t* data, int size, int& offset, uint32_t& out);
    static bool readBE64 (const uint8_t* data, int size, int& offset, uint64_t& out);
    static bool readPaddedString (const uint8_t* data, int size, int& offset, juce::String& out);
    static bool skipBlob (const uint8_t* data, int size, int& offset);

    bool parsePacket (const uint8_t* data, int size, int depth);
    bool parseMessage (const uint8_t* data, int size);
    bool processDecodedMessage (const juce::String& address, bool hasString, const juce::String& stringArg, bool hasNumber, double numberArg);

    static bool looksNumeric (const juce::String& s);
    void parseStringTc (juce::String text, bool rememberStringTs);
    void parseFloatTime (double t);
    void commit (int h, int m, int s, int f, bool rememberStringTs);
    bool isStringSuppressed() const;

    std::atomic<bool> running_ { false };
    std::atomic<bool> bindFellBack_ { false };
    std::atomic<uint64_t> packedTc_ { 0 };
    std::atomic<double> lastPacketTsMs_ { 0.0 };
    std::atomic<double> lastStringTsMs_ { 0.0 };
    std::atomic<FrameRate> fps_ { FrameRate::FPS_25 };
    juce::String addrStr_ { "/frames/str" };
    juce::String addrFloat_ { "/time" };
    juce::String bindIp_ { "0.0.0.0" };
    std::unique_ptr<juce::DatagramSocket> socket_;

    juce::File debugLog_;
    bool debugEnabled_ { false };
    double lastDebugPacketTsMs_ { 0.0 };
};
} // namespace bridge::engine

