/**
 * @file llspeechtotext.cpp
 * @brief Bounded worker-thread implementation for local speech recognition.
 */
#include "llviewerprecompiledheaders.h"

#include "llspeechtotext.h"

#include <utility>

LLSpeechToText::~LLSpeechToText()
{
    stop();
}

bool LLSpeechToText::start(std::unique_ptr<LLSpeechToTextRecognizer> recognizer)
{
    if (!recognizer)
    {
        return false;
    }

    stop();

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRecognizer = std::move(recognizer);
        mFrames.clear();
        mResults.clear();
        mStopping = false;
        mRunning = true;
    }

    mWorker = std::thread(&LLSpeechToText::workerLoop, this);
    return true;
}

void LLSpeechToText::stop()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning && !mWorker.joinable())
        {
            return;
        }
        mStopping = true;
        mFrames.clear();
    }
    mCondition.notify_all();

    if (mWorker.joinable())
    {
        mWorker.join();
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRecognizer.reset();
        mFrames.clear();
        mResults.clear();
        mRunning = false;
        mStopping = false;
    }
}

bool LLSpeechToText::isRunning() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mRunning && !mStopping;
}

bool LLSpeechToText::submitFrame(
    const llwebrtc::LLWebRTCCapturedAudioFrame& frame)
{
    if (frame.mSampleCount == 0 ||
        frame.mSampleCount > llwebrtc::LLWebRTCCapturedAudioFrame::MAX_SAMPLES ||
        frame.mSampleRate == 0 || frame.mChannels == 0)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning || mStopping)
        {
            return false;
        }

        // Prefer recent speech over stale audio if recognition falls behind.
        if (mFrames.size() >= MAX_PENDING_FRAMES)
        {
            mFrames.pop_front();
        }
        mFrames.push_back(frame);
    }
    mCondition.notify_one();
    return true;
}

bool LLSpeechToText::tryPopResult(LLSpeechToTextResult& result)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mResults.empty())
    {
        return false;
    }

    result = std::move(mResults.front());
    mResults.pop_front();
    return true;
}

void LLSpeechToText::workerLoop()
{
    for (;;)
    {
        llwebrtc::LLWebRTCCapturedAudioFrame frame;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait(lock, [this]
            {
                return mStopping || !mFrames.empty();
            });

            if (mStopping)
            {
                break;
            }

            frame = std::move(mFrames.front());
            mFrames.pop_front();
        }

        std::vector<LLSpeechToTextResult> results;
        if (mRecognizer)
        {
            try
            {
                mRecognizer->processAudio(frame, results);
            }
            catch (...)
            {
                // A model/backend failure must not terminate the Viewer worker.
                results.clear();
            }
        }

        if (!results.empty())
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (!mStopping)
            {
                for (auto& result : results)
                {
                    if (result.text.empty())
                    {
                        continue;
                    }
                    if (mResults.size() >= MAX_PENDING_RESULTS)
                    {
                        mResults.pop_front();
                    }
                    mResults.push_back(std::move(result));
                }
            }
        }
    }
}
