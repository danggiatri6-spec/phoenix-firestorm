/**
 * @file llspeechtotext.h
 * @brief Local speech-to-text worker and recognizer adapter contract.
 *
 * OpenSim-next extension. Recognition is deliberately isolated from the
 * WebRTC audio callback and from the Viewer UI thread. *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * OpenSim-next Viewer Extension
 * Copyright (C) 2026, OpenSim-next contributors.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$

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

    // Optional cooperative cancellation for long-running inference.
    virtual void requestStop() {}
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
    bool hasFailed() const;
    std::string getError() const;

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
    bool mFailed{ false };
    std::string mError;
};

// Returns a Whisper.cpp recognizer when the optional backend is compiled in.
// Returns nullptr in standard builds without LL_HAS_WHISPER_CPP.
std::unique_ptr<LLSpeechToTextRecognizer> createWhisperSpeechRecognizer(
    const std::string& model_path, const std::string& language = "auto");

#endif // LL_LLSPEECHTOTEXT_H
