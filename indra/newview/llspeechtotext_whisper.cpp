/**
 * @file llspeechtotext_whisper.cpp
 * @brief Optional CPU Whisper.cpp recognizer adapter.
 *
 * Define LL_HAS_WHISPER_CPP and link the pinned whisper.cpp target to enable.
 * Standard Viewer builds remain independent of the optional dependency. *
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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(LL_HAS_WHISPER_CPP)
#include <whisper.h>

namespace
{
class LLWhisperSpeechRecognizer final : public LLSpeechToTextRecognizer
{
public:
    LLWhisperSpeechRecognizer(const std::string& model_path, const std::string& language)
        : mModelPath(model_path),
          mLanguage(language),
          mContext(nullptr),
          mStopRequested(false)
    {
    }

    ~LLWhisperSpeechRecognizer() override
    {
        if (mContext)
        {
            whisper_free(mContext);
            mContext = nullptr;
        }
    }

    void processAudio(
        const llwebrtc::LLWebRTCCapturedAudioFrame& frame,
        std::vector<LLSpeechToTextResult>& results) override
    {
        // Validate the frame before touching the model or dividing by channels.
        if (frame.mSampleCount == 0 ||
            frame.mSampleCount > llwebrtc::LLWebRTCCapturedAudioFrame::MAX_SAMPLES ||
            frame.mChannels == 0 || frame.mChannels > 2 ||
            frame.mSampleCount % frame.mChannels != 0 ||
            frame.mSampleRate < 8000 || frame.mSampleRate > 48000)
        {
            return;
        }

        const std::size_t input_frames = frame.mSampleCount / frame.mChannels;
        if (!input_frames)
        {
            return;
        }

        if (!mContext && !loadModel())
        {
            return;
        }

        // Convert interleaved signed-16 PCM to mono float and resample to
        // Whisper's required 16 kHz input. This work runs only on the STT worker.
        std::vector<float> mono(input_frames);
        for (std::size_t i = 0; i < input_frames; ++i)
        {
            int32_t sum = 0;
            for (uint32_t channel = 0; channel < frame.mChannels; ++channel)
            {
                sum += frame.mSamples[i * frame.mChannels + channel];
            }
            mono[i] = static_cast<float>(sum) /
                (32768.0f * static_cast<float>(frame.mChannels));
        }

        const std::size_t output_frames =
            (input_frames * WHISPER_SAMPLE_RATE) / frame.mSampleRate;
        if (!output_frames)
        {
            return;
        }

        const float ratio = static_cast<float>(frame.mSampleRate) /
            static_cast<float>(WHISPER_SAMPLE_RATE);
        for (std::size_t i = 0; i < output_frames; ++i)
        {
            const float source = static_cast<float>(i) * ratio;
            const std::size_t left = std::min<std::size_t>(
                static_cast<std::size_t>(source), input_frames - 1);
            const std::size_t right = std::min(left + 1, input_frames - 1);
            const float fraction = source - static_cast<float>(left);
            mAudio.push_back(mono[left] * (1.0f - fraction) +
                mono[right] * fraction);
        }

        // Decode bounded five-second windows. This first adapter emits final
        // segments only; live partial hypotheses can be added after UI wiring.
        const std::size_t window_samples = 5 * WHISPER_SAMPLE_RATE;
        while (mAudio.size() >= window_samples)
        {
            // Do not spend CPU decoding a completely silent five-second window.
            // A low threshold keeps quiet speech eligible; this is only a coarse
            // silence gate, not voice activity detection or speech segmentation.
            double energy = 0.0;
            for (std::size_t i = 0; i < window_samples; ++i)
            {
                const double sample = static_cast<double>(mAudio[i]);
                energy += sample * sample;
            }
            const double rms = std::sqrt(energy / static_cast<double>(window_samples));
            if (rms < 0.003)
            {
                mAudio.erase(mAudio.begin(), mAudio.begin() + window_samples);
                continue;
            }

            whisper_full_params params =
                whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            params.n_threads = 2;
            params.no_context = true;
            params.single_segment = false;
            params.print_progress = false;
            params.print_realtime = false;
            params.print_timestamps = false;
            params.abort_callback = &LLWhisperSpeechRecognizer::abortCallback;
            params.abort_callback_user_data = this;
            params.language = mLanguage.empty() ? "auto" : mLanguage.c_str();

            const int decode_result = whisper_full(mContext, params, mAudio.data(),
                static_cast<int>(window_samples));
            if (decode_result != 0)
            {
                throw std::runtime_error("Whisper inference failed with code " +
                    std::to_string(decode_result));
            }

            const int count = whisper_full_n_segments(mContext);
            for (int i = 0; i < count; ++i)
            {
                const char* segment = whisper_full_get_segment_text(mContext, i);
                if (segment && *segment)
                {
                    LLSpeechToTextResult result;
                    result.text = segment;
                    result.is_final = true;
                    results.push_back(std::move(result));
                }
            }
            mAudio.erase(mAudio.begin(), mAudio.begin() + window_samples);
        }
    }

    void reset() override
    {
        mAudio.clear();
    }

    void requestStop() override
    {
        mStopRequested.store(true, std::memory_order_relaxed);
    }

private:
    static bool abortCallback(void* user_data)
    {
        const auto* recognizer = static_cast<const LLWhisperSpeechRecognizer*>(user_data);
        return recognizer->mStopRequested.load(std::memory_order_relaxed);
    }

    bool loadModel()
    {
        if (mModelPath.empty())
        {
            return false;
        }
        whisper_context_params params = whisper_context_default_params();
        params.use_gpu = false;
        mContext = whisper_init_from_file_with_params(mModelPath.c_str(), params);
        if (!mContext)
        {
            throw std::runtime_error("Whisper could not load the selected model. "
                "The file may be corrupt or not a compatible GGML/GGUF model.");
        }
        return true;
    }

    std::string mModelPath;
    std::string mLanguage;
    whisper_context* mContext;
    std::atomic<bool> mStopRequested;
    std::vector<float> mAudio;
};
} // anonymous namespace
#endif // LL_HAS_WHISPER_CPP

std::unique_ptr<LLSpeechToTextRecognizer> createWhisperSpeechRecognizer(
    const std::string& model_path, const std::string& language)
{
#if defined(LL_HAS_WHISPER_CPP)
    if (model_path.empty())
    {
        return nullptr;
    }
    return std::unique_ptr<LLSpeechToTextRecognizer>(
        new LLWhisperSpeechRecognizer(model_path, language));
#else
    (void)model_path;
    (void)language;
    return nullptr;
#endif
}
