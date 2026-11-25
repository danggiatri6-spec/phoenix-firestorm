/**
 * @file llspeechtotext.h
 * @brief Local speech-to-text worker and recognizer adapter contract.
 *
 * OpenSim-next extension. Recognition is deliberately isolated from the
 * WebRTC audio callback and from the Viewer UI thread.
 */
#ifndef LL_LLSPEECHTOTEXT_H
#define LL_LLSPEECHTOTEXT_H

#include "llwebrtc.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct LLSpeechToTextResult
{
    std::string text;
    bool        is_final{ false };
};

// Implementations (for example, a Whisper.cpp adapter) run only on the
// LLSpeechToText worker thread. They must not access Viewer UI objects.
class LLSpeechToTextRecognizer
{
public:
    virtual ~LLSpeechToTextRecognizer() = default;

    virtual void processAudio(
        const llwebrtc::LLWebRTCCapturedAudioFrame& frame,
        std::vector<LLSpeechToTextResult>& results) = 0;

    virtual void reset() = 0;
};

class LLSpeechToText
{
public:
    static constexpr std::size_t MAX_PENDING_FRAMES = 25;
    static constexpr std::size_t MAX_PENDING_RESULTS = 32;

    LLSpeechToText() = default;
    ~LLSpeechToText();

    LLSpeechToText(const LLSpeechToText&) = delete;
    LLSpeechToText& operator=(const LLSpeechToText&) = delete;

    // Takes ownership of the recognizer. Returns false for a null adapter.
    bool start(std::unique_ptr<LLSpeechToTextRecognizer> recognizer);
    void stop();
    bool isRunning() const;

    // Called by the Viewer/main thread after polling WebRTC. This method
    // copies into a bounded queue; it never waits for recognition to finish.
    bool submitFrame(const llwebrtc::LLWebRTCCapturedAudioFrame& frame);

    // Called by the Viewer/main thread to retrieve completed transcript events.
    bool tryPopResult(LLSpeechToTextResult& result);

private:
    void workerLoop();

    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    std::deque<llwebrtc::LLWebRTCCapturedAudioFrame> mFrames;
    std::deque<LLSpeechToTextResult> mResults;
    std::unique_ptr<LLSpeechToTextRecognizer> mRecognizer;
    std::thread mWorker;
    bool mRunning{ false };
    bool mStopping{ false };
};

#endif // LL_LLSPEECHTOTEXT_H
