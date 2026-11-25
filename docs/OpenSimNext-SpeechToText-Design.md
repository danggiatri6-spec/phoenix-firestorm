# OpenSim-next Speech-to-Text (STT) Design

Status: approved architecture / implementation plan
Target branch: opensim-next/avatar-interaction-actions

## Product scope

Implement both:
1. Local microphone dictation into the Nearby Chat editor. Recognition is local by default; the user reviews and edits the text before sending.
2. Captions for incoming voice from other avatars. Captions must be associated with the correct speaker UUID. Do not display a guessed speaker attribution.

This work is additive. It must not change the OpenSim/Second Life voice signaling protocol, voice codec, login flow, or existing avatar animation/interaction code.

## Existing voice architecture confirmed in this branch

- The Viewer has separate voice backends: WebRTC (llvoicewebrtc) and Vivox (llvoicevivox).
- The WebRTC wrapper is a separate shared library (indra/llwebrtc) specifically to isolate native WebRTC threading and dependencies from Viewer code.
- LLWebRTCAudioTransport::RecordedDataIsAvailable() receives microphone PCM on the WebRTC audio-device path. It currently forwards the audio to the native WebRTC engine and calculates microphone energy.
- LLWebRTCAudioTransport::NeedMorePlayData() delegates playout to the engine. PullRenderData() is the mixed-render path, so it is not suitable for per-avatar attribution.
- LLWebRTCPeerConnectionImpl::OnAddTrack() receives remote RTP audio tracks, and OnRemoveTrack() reports their removal. The current implementation only sets Opus parameters; it does not expose decoded PCM to the Viewer.
- WebRTC observer callbacks run on native WebRTC threads. No recognition, allocation-heavy work, Viewer UI calls, or blocking locks may run in audio callbacks.
- The nearby chat editor is FSFloaterNearbyChat (indra/newview/fsfloaternearbychat.cpp) and is initialized in postBuild().

## Proposed component boundaries

### Viewer-side STT service

Add a Viewer-owned service (proposed names: LLSpeechToText, LLSpeechAudioQueue, LLSpeechCaptionModel):
- Owns recognizer lifecycle, model selection, language setting, VAD/segment state, and worker thread.
- Receives copied PCM blocks through bounded, non-blocking queues.
- Publishes partial/final transcript events onto the Viewer main thread.
- Has independent enable flags for microphone dictation and incoming captions.
- Defaults to local processing; no audio upload or telemetry.

### Recognition engine

Use whisper.cpp behind a narrow adapter interface, not throughout Viewer code:
- CPU-only operation must be supported.
- Resample input to the recognizer's expected mono sample rate off the real-time callback.
- Model files are optional external assets and must not be silently bundled into the Viewer installer.
- Model download, licensing, integrity verification, storage path, and first-run UX require separate decisions.
- Keep the adapter replaceable so a lighter streaming recognizer can be evaluated later.

### Local microphone dictation

Preferred WebRTC path:
1. Add an optional audio-tap interface to indra/llwebrtc/llwebrtc.h.
2. In LLWebRTCAudioTransport::RecordedDataIsAvailable(), copy a bounded PCM frame into a preallocated/non-blocking handoff buffer after the authoritative engine call.
3. The Viewer consumes frames on a worker thread and runs VAD/recognition.
4. Deliver partial text to a dictation controller; only commit final text into LLChatEntry when the user confirms or the configured push-to-talk dictation mode ends.
5. Preserve the existing mic mute semantics. Muted microphone audio must not be transcribed unless the user explicitly opts into a separate local dictation mode.

Vivox must be audited separately. Do not assume the Vivox SDK exposes raw capture PCM. If it does not, microphone dictation can use a separate OS capture device stream only with explicit device selection and echo/feedback safeguards; it must not silently open a second microphone stream.

### Incoming avatar captions

WebRTC:
- Investigate adding a lightweight webrtc::AudioTrackSinkInterface to each received audio track from OnAddTrack(), removing it in OnRemoveTrack() and during connection shutdown.
- Sink callbacks must only copy PCM and stable track/session identifiers into a bounded queue.
- Determine whether the gateway supplies one remote track per participant or a mixed channel track. This is a release-blocking attribution question: if the track is mixed, the Viewer cannot truthfully label individual speakers from audio alone.
- Map a remote track to an avatar UUID only from authoritative signaling/session metadata. Never infer identity from voice characteristics.

Vivox:
- Inspect the licensed SDK/API and current event model for a supported per-participant PCM tap. Do not reverse engineer proprietary transports or assume event callbacks contain audio samples.
- If no supported PCM tap exists, show the feature as unavailable for that backend until a supported integration is found. Do not record the system output mix as a substitute for per-avatar captions.

## UI behavior

- Add a microphone/dictation control beside Nearby Chat input.
- Dictation inserts text into the editor; it never sends automatically by default.
- Add a captions panel/overlay with speaker display name, partial/final state, and timestamp.
- Provide independent settings: microphone dictation, incoming captions, language (auto or fixed), model, caption visibility, and clear caption history.
- Respect mute/block state and local privacy controls. Do not save audio. Caption history should be ephemeral by default.

## Threading, performance, and safety requirements

- Audio callbacks: bounded copy only; no allocations, model inference, disk I/O, logging per frame, or Viewer UI access.
- Queue overflow: drop stale frames and reset/close the affected segment cleanly; never block the audio thread.
- Recognition: worker thread(s), bounded CPU use, configurable model size, and no GPU requirement.
- UI: marshal transcript events to the main thread.
- Shutdown: stop capture taps, detach track sinks, drain/close queues, join workers, then destroy recognizer/model state.
- Voice disabled, connection closed, track removed, mute/block changes, and application shutdown must all clean up safely.

## Implementation sequence

1. Complete backend capability audit: WebRTC track cardinality/participant mapping and Vivox PCM access.
2. Implement the Viewer STT service and recognizer adapter with a synthetic PCM test harness.
3. Complete the WebRTC microphone capture bridge and add the local dictation UI.
4. Add WebRTC incoming-track sinks only after confirming track-to-avatar attribution.
5. Implement Vivox support only if its supported API exposes the required PCM.
6. Add settings, packaging/model management, performance tests, and two-Viewer/multi-speaker tests.

## Acceptance criteria

- Existing voice login, spatial audio, mute, push-to-talk, and Second Life compatibility remain unchanged.
- Dictation works without blocking rendering or voice transmission and does not auto-send.
- Captions identify a speaker only when the backend provides a verified participant mapping.
- No audio is sent to a cloud service by default and no raw audio is persisted.
- Feature can be disabled independently for microphone dictation and incoming captions.


## Audit update (2026-11-25)

- WebRTC local capture tap is technically feasible at the existing LLWebRTCAudioTransport::RecordedDataIsAvailable() boundary. The implementation must use a bounded preallocated handoff; do not run Whisper in the audio callback.
- WebRTC remote decoded audio is observable at the received AudioTrack boundary in LLWebRTCPeerConnectionImpl::OnAddTrack(). The current code does not attach an AudioTrackSinkInterface. Before implementing captions, confirm the gateway's track cardinality and whether track/stream identifiers can be mapped to participant UUIDs from existing voice signaling metadata.
- Vivox source audit found daemon command/event handling, device enumeration, mic tuning, and capture-buffer recording/playback commands, but no raw PCM frame callback exposed to Viewer code. Therefore native per-avatar captions and shared-pipeline dictation cannot currently be claimed for Vivox. A supported SDK API or a separately selected OS capture path would be required; do not capture system output as a false per-speaker substitute.
- The initial implementation should target the WebRTC backend and keep Vivox capability explicitly unavailable until a supported audio-frame API is identified.


## Implementation update (2026-11-25)

The first WebRTC capture bridge is now committed on the OpenSim-next development branch:

- `LLWebRTCCapturedAudioFrame` is a fixed-size, interleaved signed-16-bit PCM frame (up to 1,920 samples, with sample-rate and channel metadata).
- `LLWebRTCAudioTransport` has a 16-slot single-producer/single-consumer ring. The audio callback only copies a bounded frame when the opt-in capture switch is enabled; a full queue drops the recognition frame rather than blocking voice transmission.
- The Viewer can enable/disable the tap and poll frames through `LLWebRTCVoiceClient`. Capture is disabled by default and explicitly disabled before WebRTC shutdown.
- Disabling capture flushes pending queued PCM. No recognition engine, UI control, or automatic chat sending is wired yet.

This is an API/implementation milestone, not a validated build. A full Windows build and runtime test are still required before treating the bridge as production-ready. The next implementation step is the Viewer-side bounded worker and recognizer adapter, followed by the Nearby Chat dictation control.


## Worker implementation update (2026-11-25)

The Viewer-side worker boundary is now present:

- Added `LLSpeechToTextRecognizer`, a replaceable backend contract. A Whisper.cpp implementation can be supplied without coupling model headers or inference calls to the voice DLL.
- Added `LLSpeechToText`: a dedicated worker thread, bounded pending-frame queue (25 frames), bounded transcript-result queue (32 events), and explicit start/stop/join lifecycle.
- When recognition falls behind, the oldest queued PCM is discarded to favor current speech. Stopping clears pending audio and joins the worker before releasing the recognizer.
- Added `LLWebRTCVoiceClient::pumpSpeechCapture()` to move at most a caller-selected number of captured frames per Viewer tick into the worker queue. The capture tap follows the worker's explicitly user-initiated lifetime and is disabled when the worker stops or the voice client shuts down.
- Registered `llspeechtotext.cpp` in the Viewer CMake source list.

The worker currently has no concrete recognition backend. This is intentional: Whisper.cpp model loading, CPU/thread settings, language selection, licensing, and model-file lifecycle must be integrated and tested before enabling dictation in the UI. No text is recognized or inserted into chat yet. The branch has not been built in this environment; a Windows x64 build remains a required validation step.

## Capture/worker hardening update (2026-11-25)

- Hardened worker startup: if std::thread creation fails, the service releases the recognizer and returns to a stopped state rather than remaining marked as running.
- Tightened submitted-frame validation: accepted sample rates are 8–48 kHz, channels are limited to mono/stereo, and interleaved sample counts must align to the channel count.
- Hardened the WebRTC capture tap against null audio buffers, zero-length frames, unsupported channel counts, and multiplication overflow when checking the fixed PCM frame capacity.
- These are source changes only; no Windows x64 build or runtime test has been run in this environment.

## Integration boundary remains unchanged

The PCM tap and worker are not yet connected to an active Viewer idle callback or Nearby Chat UI. pumpSpeechCapture() is currently an available bridge method, not an automatically running loop. There is still no concrete recognizer backend, model loading, microphone dictation button, transcript-to-chat-editor insertion, or per-Avatar remote subtitle path. The next implementation step must wire a service lifecycle and main-thread result pump without enabling microphone capture until the user explicitly starts dictation.

## Whisper.cpp dependency assessment (2026-11-25)

- Upstream whisper.cpp provides a C-style API, CPU-only inference, and quantized model support. Its current API exposes explicit context initialization and context cleanup; the adapter should own one context on the dedicated STT worker and never share it concurrently.
- Upstream README memory estimates list tiny at about 273 MB and base at about 388 MB. This is a useful baseline, not a guarantee of total Viewer process memory or real-time performance. Start by evaluating tiny/tiny.en on target hardware; do not default to the small model for low-memory PCs.
- The OpenSim-next repository search did not find an existing whisper.cpp integration or dependency. Do not introduce an unpinned FetchContent download into normal Viewer builds. The next dependency step should be an explicit optional, version-pinned third-party build/package with the feature disabled by default until Windows x64 CI validates it.
- No Whisper adapter source has been added yet; upstream API and model selection still need a pinned revision and build verification.

## Optional Whisper adapter prototype (2026-11-25)

- Added `indra/newview/llspeechtotext_whisper.cpp` and a `createWhisperSpeechRecognizer(model_path, language)` factory declaration.
- The adapter is guarded by `LL_HAS_WHISPER_CPP`; without that compile definition it builds as a stub factory returning nullptr, so the standard Viewer does not gain a mandatory Whisper dependency.
- When enabled and linked against whisper.cpp, it requests CPU inference, converts interleaved S16 mono/stereo frames to mono float at 16 kHz, buffers five-second windows, and emits final text segments from the worker thread. It uses two inference threads by default.
- Model loading is lazy on the worker's first audio frame. The model file is external and is not downloaded or bundled automatically.
- This is an adapter prototype, not yet an end-user feature: no pinned whisper.cpp revision/package, CMake target linkage, user setting/model picker, start/stop UI, Viewer idle pump, or chat editor insertion has been completed. No Windows x64 compilation or runtime accuracy/performance test has been run.
- The adapter currently emits final segments only after each five-second window; it does not claim word-level live captions or remote-speaker attribution.

## Whisper adapter hardening update (2026-11-25)

- Validate captured-frame sample count, channel count, channel alignment, and sample-rate range before model initialization or audio conversion.
- Add a coarse RMS silence gate for completed five-second windows. Windows below the low energy threshold are discarded without invoking Whisper, reducing needless CPU use during silence. This is not a full VAD and does not yet provide speech endpointing.
- Remove a duplicate standard-library include from the worker header.
- The adapter remains optional and unbuilt: whisper.cpp is not pinned or linked, and no Windows x64 compile, runtime, latency, memory, or transcription-quality test has been performed.

## Nearby Chat insertion boundary update (2026-11-25)

- Added `FSFloaterNearbyChat::insertSpeechToText()` as a main-thread UI helper. It appends recognized text to the existing editable Nearby Chat input, places the cursor at the end, and does not invoke the chat commit/send callback.
- This helper is not yet called by a transcript-result pump. The Viewer idle integration, explicit dictation start/stop control, model-path setting, and recognizer startup are still pending.
- The insertion helper is source-level work only and has not been compiled or runtime-tested.
