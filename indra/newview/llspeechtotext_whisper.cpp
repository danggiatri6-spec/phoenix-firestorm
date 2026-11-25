/**
 * @file llspeechtotext_whisper.cpp
 * @brief Optional CPU Whisper.cpp recognizer adapter.
 *
 * Define LL_HAS_WHISPER_CPP and link the pinned whisper.cpp target to enable.
 * Standard Viewer builds remain independent of the optional dependency.
 */
#include "llviewerprecompiledheaders.h"

#include "llspeechtotext.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
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
          mContext(nullptr)
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
        if (!mContext && !loadModel())
        {
            return;
        }

        const std::size_t input_frames = frame.mSampleCount / frame.mChannels;
        if (!input_frames || !frame.mSampleRate)
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
            mAudio.insert(mAudio.end(), mono[left] * (1.0f - fraction) +
                mono[right] * fraction);
        }

        // Decode bounded five-second windows. This first adapter emits final
        // segments only; live partial hypotheses can be added after UI wiring.
        const std::size_t window_samples = 5 * WHISPER_SAMPLE_RATE;
        while (mAudio.size() >= window_samples)
        {
            whisper_full_params params =
                whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
            params.n_threads = 2;
            params.no_context = true;
            params.single_segment = false;
            params.print_progress = false;
            params.print_realtime = false;
            params.print_timestamps = false;
            params.language = mLanguage.empty() ? "auto" : mLanguage.c_str();

            if (whisper_full(mContext, params, mAudio.data(),
                    static_cast<int>(window_samples)) == 0)
            {
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
            }
            mAudio.erase(mAudio.begin(), mAudio.begin() + window_samples);
        }
    }

    void reset() override
    {
        mAudio.clear();
    }

private:
    bool loadModel()
    {
        if (mModelPath.empty())
        {
            return false;
        }
        whisper_context_params params = whisper_context_default_params();
        params.use_gpu = false;
        mContext = whisper_init_from_file_with_params(mModelPath.c_str(), params);
        return mContext != nullptr;
    }

    std::string mModelPath;
    std::string mLanguage;
    whisper_context* mContext;
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
