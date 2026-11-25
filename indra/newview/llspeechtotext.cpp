/**
 * @file llspeechtotext.cpp
 * @brief Bounded worker-thread implementation for local speech recognition.
 *
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
#include "llviewerprecompiledheaders.h"

#include "llspeechtotext.h"

#include <exception>
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
        mFailed = false;
        mError.clear();
        mRunning = true;
    }

    try
    {
        mWorker = std::thread(&LLSpeechToText::workerLoop, this);
    }
    catch (...)
    {
        // Thread creation can fail (for example, under resource pressure).
        // Restore the stopped state so callers can report failure cleanly.
        std::lock_guard<std::mutex> lock(mMutex);
        mRecognizer.reset();
        mRunning = false;
        mStopping = false;
        mFailed = false;
        mError.clear();
        return false;
    }
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
        // Ask the active backend to abort a long inference before joining.
        // The recognizer remains alive until the worker exits.
        if (mRecognizer)
        {
            mRecognizer->requestStop();
        }
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
        mFailed = false;
        mError.clear();
    }
}

bool LLSpeechToText::isRunning() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mRunning && !mStopping && !mFailed;
}

bool LLSpeechToText::hasFailed() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mFailed;
}

std::string LLSpeechToText::getError() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mError;
}

bool LLSpeechToText::submitFrame(
    const llwebrtc::LLWebRTCCapturedAudioFrame& frame)
{
    if (frame.mSampleCount == 0 ||
        frame.mSampleCount > llwebrtc::LLWebRTCCapturedAudioFrame::MAX_SAMPLES ||
        frame.mSampleRate < 8000 || frame.mSampleRate > 48000 ||
        frame.mChannels == 0 || frame.mChannels > 2 ||
        frame.mSampleCount % frame.mChannels != 0)
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
            catch (const std::exception& error)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mFailed = true;
                mError = error.what();
                mFrames.clear();
                break;
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mFailed = true;
                mError = "Unknown speech recognition backend failure";
                mFrames.clear();
                break;
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
