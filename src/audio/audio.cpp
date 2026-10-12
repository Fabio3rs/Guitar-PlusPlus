#include "audio.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace gpp::audio {
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
}

void validPcm(const PcmBuffer &pcm) {
    if (pcm.rate < 8000 || pcm.rate > 192000 || pcm.channels < 1 ||
        pcm.channels > 8 || pcm.samples.size() % pcm.channels != 0 ||
        (pcm.loop.end &&
         (pcm.loop.begin >= pcm.loop.end || pcm.loop.end > pcm.frames()))) {
        throw Error("Invalid PCM format or loop region");
    }
    for (float sample : pcm.samples) {
        if (!std::isfinite(sample)) {
            throw Error("Non-finite PCM sample");
        }
    }
}

void append16(std::vector<uint8_t> &out, uint16_t value) {
    out.push_back(uint8_t(value));
    out.push_back(uint8_t(value >> 8));
}

void append32(std::vector<uint8_t> &out, uint32_t value) {
    append16(out, uint16_t(value));
    append16(out, uint16_t(value >> 16));
}

void tag(std::vector<uint8_t> &out, const char *value) {
    out.insert(out.end(), value, value + 4);
}
} // namespace

void check(bool ok, const char *operation) {
    if (!ok) {
        const int error = BASS_ErrorGetCode();
        throw Error(std::string(operation) + ": BASS error " +
                    std::to_string(error));
    }
}

Stream::~Stream() { reset(); }

Stream::Stream(Stream &&other) noexcept
    : handle_(std::exchange(other.handle_, 0)) {}

Stream &Stream::operator=(Stream &&other) noexcept {
    if (this != &other) {
        reset();
        handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
}

void Stream::reset() noexcept {
    if (handle_) {
        BASS_StreamFree(handle_);
        handle_ = 0;
    }
}

void validate(const Settings &settings) {
    auto copy = settings;
    visitSettings(copy, [](const std::string &name, auto &value, double low,
                           double high) {
        const double n = double(value);
        if (!std::isfinite(n) || n < low || n > high) {
            throw Error(name + " must be between " + std::to_string(low) +
                        " and " + std::to_string(high));
        }
    });
    const auto &a = settings.analysis;
    if ((a.fft & (a.fft - 1)) != 0 || a.midiMin > a.midiMax) {
        throw Error(
            "FFT must be a power of two; MIDI minimum must not exceed maximum");
    }
    for (const auto *eq : {&settings.fx.pre, &settings.fx.post}) {
        if (eq->enabled && ((eq->bandwidth > 0 && eq->bandwidth < .1) ||
                            (eq->bandwidth == 0 && eq->q == 0))) {
            throw Error("EQ requires bandwidth >= 0.1 octave, or bandwidth 0 "
                        "and Q > 0");
        }
    }
}

Stream openFile(const std::filesystem::path &path, bool decode) {
    DWORD flags = BASS_SAMPLE_FLOAT | BASS_STREAM_PRESCAN;
    if (decode) {
        flags |= BASS_STREAM_DECODE;
    }
#ifdef _WIN32
    flags |= BASS_UNICODE;
#endif
    Stream result(BASS_StreamCreateFile(FALSE, path.c_str(), 0, 0, flags));
    check(result.get() != 0, "Open audio file");
    return result;
}

SourceInfo sourceInfo(HSTREAM stream) {
    BASS_CHANNELINFO info{};
    check(BASS_ChannelGetInfo(stream, &info), "Read audio format");
    const QWORD length = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
    check(length != QWORD(-1), "Read audio length");
    const double duration = BASS_ChannelBytes2Seconds(stream, length);
    if (!info.freq || !info.chans || !std::isfinite(duration) ||
        duration <= 0) {
        throw Error("Audio source has no finite, nonempty duration");
    }
    return {info.freq, info.chans, duration};
}

void seek(HSTREAM stream, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        throw Error("Seek time must be finite and nonnegative");
    }
    const QWORD position = BASS_ChannelSeconds2Bytes(stream, seconds);
    check(position != QWORD(-1), "Convert seek time");
    check(BASS_ChannelSetPosition(stream, position, BASS_POS_BYTE),
          "Seek audio");
}

Levels levels(const PcmBuffer &pcm) {
    validPcm(pcm);
    Levels result;
    double sum = 0;
    for (float sample : pcm.samples) {
        result.peak = std::max(result.peak, std::abs(double(sample)));
        result.clipped += std::abs(sample) >= 1.f;
        sum += double(sample) * sample;
    }
    result.rms = pcm.samples.empty() ? 0 : std::sqrt(sum / pcm.samples.size());
    return result;
}

void normalize(PcmBuffer &pcm, double rms, double peak) {
    const auto measured = levels(pcm);
    if (measured.peak < 1e-9 || measured.rms == 0) {
        return;
    }
    const double gain = std::min(rms / measured.rms, peak / measured.peak);
    for (auto &sample : pcm.samples) {
        sample = float(sample * gain);
    }
}

Waveform waveform(const std::filesystem::path &path,
                  const Checkpoint &checkpoint) {
    auto stream = openFile(path, true);
    Waveform result;
    result.info = sourceInfo(stream.get());
    // Bounded display envelope: decoding does not retain full-song PCM.
    const size_t frames =
        size_t(std::ceil(result.info.duration * result.info.rate));
    const size_t perBin = std::max(size_t(1), (frames + 65535) / 65536);
    result.low.assign((frames + perBin - 1) / perBin, 0);
    result.high = result.low;
    std::vector<float> chunk(4096 * result.info.channels);
    size_t cursor = 0;
    for (;;) {
        if (checkpoint) {
            checkpoint(std::min(1., double(cursor) / frames));
        }
        const DWORD got = BASS_ChannelGetData(
            stream.get(), chunk.data(), DWORD(chunk.size() * sizeof(float)));
        if (got == DWORD(-1)) {
            if (BASS_ErrorGetCode() == BASS_ERROR_ENDED) {
                break;
            }
            check(false, "Decode waveform");
        }
        if (!got) {
            break;
        }
        const size_t count = got / sizeof(float) / result.info.channels;
        for (size_t frame = 0; frame < count; ++frame) {
            const size_t bin = (cursor + frame) / perBin;
            if (bin >= result.low.size()) {
                break;
            }
            for (unsigned ch = 0; ch < result.info.channels; ++ch) {
                const float sample = chunk[frame * result.info.channels + ch];
                result.low[bin] = std::min(result.low[bin], sample);
                result.high[bin] = std::max(result.high[bin], sample);
            }
        }
        cursor += count;
    }
    return result;
}

PcmBuffer readSegment(const std::filesystem::path &path, double start,
                      double length, const Checkpoint &checkpoint) {
    auto stream = openFile(path, true);
    const auto info = sourceInfo(stream.get());
    if (!std::isfinite(start) || !std::isfinite(length) || start < 0 ||
        start >= info.duration || length <= 0) {
        throw Error("Invalid audio segment");
    }
    seek(stream.get(), start);
    PcmBuffer result;
    result.rate = info.rate;
    result.channels = info.channels;
    const double count =
        std::ceil(std::min(length, info.duration - start) * info.rate) *
        info.channels;
    if (count > double(UINT32_MAX - 128) / sizeof(float)) {
        throw Error("Selected segment exceeds the 4 GiB WAV limit; select a "
                    "shorter segment");
    }
    const size_t wanted = size_t(count);
    std::vector<float> chunk(4096 * info.channels);
    while (result.samples.size() < wanted) {
        if (checkpoint) {
            checkpoint(double(result.samples.size()) / wanted);
        }
        const size_t amount =
            std::min(chunk.size(), wanted - result.samples.size());
        const DWORD got = BASS_ChannelGetData(stream.get(), chunk.data(),
                                              DWORD(amount * sizeof(float)));
        if (got == DWORD(-1)) {
            if (BASS_ErrorGetCode() == BASS_ERROR_ENDED) {
                break;
            }
            check(false, "Decode segment");
        }
        if (!got) {
            break;
        }
        result.samples.insert(result.samples.end(), chunk.begin(),
                              chunk.begin() + got / sizeof(float));
    }
    return result;
}

FxChain::~FxChain() { clear(); }

FxChain::FxChain(FxChain &&other) noexcept
    : channel_(std::exchange(other.channel_, 0)),
      effects_(std::exchange(other.effects_, {})) {}

FxChain &FxChain::operator=(FxChain &&other) noexcept {
    if (this != &other) {
        clear();
        channel_ = std::exchange(other.channel_, 0);
        effects_ = std::exchange(other.effects_, {});
    }
    return *this;
}

void FxChain::clear() noexcept {
    for (auto &effect : effects_) {
        if (effect) {
            BASS_ChannelRemoveFX(channel_, effect);
            effect = 0;
        }
    }
    channel_ = 0;
}

void FxChain::apply(DWORD channel, unsigned rate, const FxSettings &settings) {
    Settings validation;
    validation.fx = settings;
    validate(validation);
    for (const auto *eq : {&settings.pre, &settings.post}) {
        if (settings.enabled && eq->enabled && eq->frequency >= rate * .5) {
            throw Error(
                "EQ frequency must be below this source's Nyquist frequency");
        }
    }
    if (channel_ != channel) {
        clear();
        channel_ = channel;
    }
    const bool active[] = {settings.enabled && settings.pre.enabled,
                           settings.enabled && settings.distortion,
                           settings.enabled && settings.post.enabled};
    for (size_t i = 0; i < effects_.size(); ++i) {
        if (!active[i]) {
            if (effects_[i]) {
                check(BASS_ChannelRemoveFX(channel_, effects_[i]), "Bypass FX");
                effects_[i] = 0;
            }
            continue;
        }
        if (!effects_[i]) {
            effects_[i] = BASS_ChannelSetFX(
                channel_, i == 1 ? BASS_FX_BFX_DISTORTION : BASS_FX_BFX_PEAKEQ,
                int(20 - 10 * i));
            check(effects_[i] != 0, "Attach FX");
        }
        if (i == 1) {
            BASS_BFX_DISTORTION fx{
                float(settings.drive),  float(settings.dry),
                float(settings.wet),    float(settings.feedback),
                float(settings.volume), BASS_BFX_CHANALL};
            check(BASS_FXSetParameters(effects_[i], &fx), "Set distortion");
        } else {
            const auto &eq = i == 0 ? settings.pre : settings.post;
            BASS_BFX_PEAKEQ fx{0,
                               float(eq.bandwidth),
                               float(eq.q),
                               float(eq.frequency),
                               float(eq.gain),
                               BASS_BFX_CHANALL};
            check(BASS_FXSetParameters(effects_[i], &fx), "Set EQ");
        }
    }
}

PcmBuffer processFx(const PcmBuffer &raw, const FxSettings &settings,
                    const Checkpoint &checkpoint) {
    validPcm(raw);
    Settings validation;
    validation.fx = settings;
    validate(validation);
    PcmBuffer padded;
    padded.rate = raw.rate;
    padded.channels = raw.channels;
    const size_t pre = size_t(std::llround(settings.preRoll * raw.rate));
    const size_t tail = size_t(std::llround(settings.tail * raw.rate));
    padded.samples.assign(pre * raw.channels, 0);
    padded.samples.insert(padded.samples.end(), raw.samples.begin(),
                          raw.samples.end());
    padded.samples.resize(padded.samples.size() + tail * raw.channels, 0);
    // FLOAT WAV avoids the prototype's intermediate PCM16 quantization.
    auto bytes = wavBytes(padded, true);
    Stream stream(
        BASS_StreamCreateFile(TRUE, bytes.data(), 0, bytes.size(),
                              BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT));
    check(stream.get() != 0, "Open synthesis decode stream");
    FxChain fx;
    fx.apply(stream.get(), raw.rate, settings);
    PcmBuffer result;
    result.rate = raw.rate;
    result.channels = raw.channels;
    result.loop = raw.loop;
    std::vector<float> chunk(4096 * raw.channels);
    for (;;) {
        if (checkpoint) {
            checkpoint(double(result.samples.size()) /
                       std::max(size_t(1), padded.samples.size()));
        }
        DWORD got = BASS_ChannelGetData(stream.get(), chunk.data(),
                                        DWORD(chunk.size() * sizeof(float)));
        if (got == DWORD(-1)) {
            if (BASS_ErrorGetCode() == BASS_ERROR_ENDED) {
                break;
            }
            check(false, "Decode FX result");
        }
        if (!got) {
            break;
        }
        result.samples.insert(result.samples.end(), chunk.begin(),
                              chunk.begin() + got / sizeof(float));
    }
    if (result.samples.size() < pre * raw.channels) {
        throw Error("Incomplete FX output");
    }
    result.samples.erase(result.samples.begin(),
                         result.samples.begin() + pre * raw.channels);
    const size_t fade = std::min(
        result.frames(), size_t(std::llround(settings.fade * raw.rate)));
    for (size_t frame = 0; frame < fade; ++frame) {
        const float gain =
            fade > 1 ? float(fade - frame - 1) / float(fade - 1) : 0;
        for (unsigned ch = 0; ch < result.channels; ++ch) {
            result.samples[(result.frames() - fade + frame) * result.channels +
                           ch] *= gain;
        }
    }
    if (settings.normalize) {
        normalize(result, settings.rms, settings.peak);
    }
    validPcm(result);
    return result;
}

std::vector<uint8_t> wavBytes(const PcmBuffer &pcm, bool float32) {
    validPcm(pcm);
    const uint32_t bytesPerSample = float32 ? 4 : 2;
    const uint64_t count = uint64_t(pcm.samples.size()) * bytesPerSample;
    const uint32_t extra = pcm.loop.end ? 68 : 0;
    if (count + 36 + extra > UINT32_MAX) {
        throw Error("WAV exceeds RIFF size limit");
    }
    std::vector<uint8_t> out;
    out.reserve(size_t(count) + 44 + extra);
    tag(out, "RIFF");
    append32(out, uint32_t(count) + 36 + extra);
    tag(out, "WAVE");
    tag(out, "fmt ");
    append32(out, 16);
    append16(out, float32 ? 3 : 1);
    append16(out, uint16_t(pcm.channels));
    append32(out, pcm.rate);
    append32(out, pcm.rate * pcm.channels * bytesPerSample);
    append16(out, uint16_t(pcm.channels * bytesPerSample));
    append16(out, uint16_t(bytesPerSample * 8));
    tag(out, "data");
    append32(out, uint32_t(count));
    for (float value : pcm.samples) {
        if (float32) {
            uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value),
                          "32-bit float required");
            std::memcpy(&bits, &value, sizeof(bits));
            append32(out, bits);
        } else {
            append16(out, uint16_t(int16_t(std::lrint(
                              std::clamp(value, -1.f, 1.f) * 32767.f))));
        }
    }
    if (pcm.loop.end) {
        tag(out, "smpl");
        append32(out, 60);
        append32(out, 0);
        append32(out, 0);
        append32(out, 1000000000u / pcm.rate);
        append32(out, 60);
        append32(out, 0);
        append32(out, 0);
        append32(out, 0);
        append32(out, 1);
        append32(out, 0);
        append32(out, 0);
        append32(out, 0);
        append32(out, uint32_t(pcm.loop.begin));
        append32(out, uint32_t(pcm.loop.end - 1));
        append32(out, 0);
        append32(out, 0);
    }
    return out;
}

void writeWav(const std::filesystem::path &path, const PcmBuffer &pcm,
              bool float32) {
    const auto bytes = wavBytes(pcm, float32);
    auto temporary = path;
    temporary +=
        "." + std::to_string(Clock::now().time_since_epoch().count()) + ".tmp";
    try {
        std::ofstream out(temporary, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  std::streamsize(bytes.size()));
        out.close();
        if (!out) {
            throw Error("Cannot write WAV: " + path.string());
        }
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::vector<DeviceInfo> devices() {
    std::vector<DeviceInfo> result;
    BASS_DEVICEINFO info{};
    for (int i = 0; BASS_GetDeviceInfo(DWORD(i), &info); ++i) {
        result.push_back({i, info.name ? info.name : "Unknown",
                          bool(info.flags & BASS_DEVICE_ENABLED),
                          bool(info.flags & BASS_DEVICE_DEFAULT)});
    }
    return result;
}

void validate(const DeviceSettings &s) {
    if (s.device < -1 || s.rate < 8000 || s.rate > 192000 || s.bufferMs < 10 ||
        s.bufferMs > 5000 || s.updateMs < 5 || s.updateMs > 100 ||
        s.bufferMs <= s.updateMs || s.devicePeriodMs < 1 ||
        s.devicePeriodMs > 100 || s.deviceBufferMs < 2 * s.devicePeriodMs ||
        s.deviceBufferMs > 5000) {
        throw Error("Invalid device settings: channel buffer > update period; "
                    "device buffer >= 2 x device period");
    }
}

BassSystem::BassSystem(const DeviceSettings &settings) {
    validate(settings);
    if (HIWORD(BASS_GetVersion()) != BASSVERSION ||
        HIWORD(BASS_FX_GetVersion()) != BASSVERSION) {
        throw Error("Incompatible BASS/BASS_FX API version");
    }
    check(BASS_SetConfig(BASS_CONFIG_BUFFER, DWORD(settings.bufferMs)),
          "Configure stream buffer");
    check(BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, DWORD(settings.updateMs)),
          "Configure stream updates");
#ifndef __APPLE__
    check(
        BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, DWORD(settings.deviceBufferMs)),
        "Configure device buffer");
#endif
    check(
        BASS_SetConfig(BASS_CONFIG_DEV_PERIOD, DWORD(settings.devicePeriodMs)),
        "Configure device period");
    check(BASS_Init(settings.device, DWORD(settings.rate), 0, nullptr, nullptr),
          "Initialize BASS device");
    device_ = int(BASS_GetDevice());
}

BassSystem::~BassSystem() {
    if (device_ >= 0) {
        BASS_SetDevice(DWORD(device_));
        BASS_Free();
    }
}

void AudioTransport::close() {
    fx_.clear();
    stream_.reset();
    memory_.clear();
    info_ = {};
    loopSync_ = 0;
    loopControl_.error = 0;
}

void AudioTransport::open(const std::filesystem::path &path,
                          const FxSettings *settings) {
    auto next = openFile(path, false);
    const auto info = sourceInfo(next.get());
    FxChain nextFx;
    if (settings) {
        nextFx.apply(next.get(), info.rate, *settings);
    }
    close();
    stream_ = std::move(next);
    fx_ = std::move(nextFx);
    info_ = info;
}

void AudioTransport::open(const PcmBuffer &pcm) {
    auto memory = wavBytes(pcm, true);
    Stream next(BASS_StreamCreateFile(TRUE, memory.data(), 0, memory.size(),
                                      BASS_SAMPLE_FLOAT));
    check(next.get() != 0, "Open PCM preview");
    const auto info = sourceInfo(next.get());
    close();
    memory_ = std::move(memory);
    stream_ = std::move(next);
    info_ = info;
    if (pcm.loop.end) {
        loop(double(pcm.loop.begin) / pcm.rate, double(pcm.loop.end) / pcm.rate,
             true);
    }
}

void AudioTransport::play() {
    const auto start = Clock::now();
    check(BASS_ChannelPlay(stream_.get(), FALSE), "Play audio");
    lastPlayMs = elapsed(start);
}

void AudioTransport::pause() {
    check(BASS_ChannelPause(stream_.get()), "Pause audio");
}

void AudioTransport::stop() {
    check(BASS_ChannelStop(stream_.get()), "Stop audio");
    position(0);
}

void AudioTransport::position(double seconds) {
    if (seconds > info_.duration) {
        throw Error("Seek exceeds source duration");
    }
    const auto start = Clock::now();
    seek(stream_.get(), seconds);
    lastSeekMs = elapsed(start);
}

double AudioTransport::position() const {
    if (!stream_.get()) {
        return 0;
    }
    const QWORD value = BASS_ChannelGetPosition(stream_.get(), BASS_POS_BYTE);
    check(value != QWORD(-1), "Read playback position");
    return BASS_ChannelBytes2Seconds(stream_.get(), value);
}

void AudioTransport::loop(double begin, double end, bool enabled) {
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 ||
        begin >= end || end > info_.duration + .0001) {
        throw Error("Invalid playback loop");
    }
    // BASS 2.4.14 does not implement POS_END/POS_LOOP despite newer headers.
    // Keep mixtime syncs for compatibility with both older and current BASS.
    // Lock only while replacing syncs and flushing existing buffered data.
    const DWORD channel = stream_.get();
    check(BASS_ChannelLock(channel, TRUE), "Lock channel for loop change");
    struct Unlock {
        DWORD channel;
        ~Unlock() { BASS_ChannelLock(channel, FALSE); }
    } unlock{channel};
    HSYNC next = 0;
    if (enabled) {
        const QWORD endBytes = BASS_ChannelSeconds2Bytes(channel, end);
        const QWORD length = BASS_ChannelGetLength(channel, BASS_POS_BYTE);
        const DWORD type = endBytes >= length ? BASS_SYNC_END : BASS_SYNC_POS;
        next = BASS_ChannelSetSync(channel, type | BASS_SYNC_MIXTIME,
                                   type == BASS_SYNC_END ? 0 : endBytes,
                                   loopCallback, &loopControl_);
        check(next != 0, "Set loop sync");
    }
    if (loopSync_) {
        BASS_ChannelRemoveSync(channel, loopSync_);
    }
    loopSync_ = next;
    loopControl_.begin = BASS_ChannelSeconds2Bytes(channel, begin);
    loopControl_.error = 0;
    double current = position();
    if (enabled && current >= end) {
        current = begin;
    }
    seek(channel, current);
}

void CALLBACK AudioTransport::loopCallback(HSYNC, DWORD channel, DWORD,
                                           void *user) {
    auto &control = *static_cast<LoopControl *>(user);
    if (!BASS_ChannelSetPosition(channel, control.begin.load(),
                                 BASS_POS_BYTE)) {
        control.error.store(BASS_ErrorGetCode());
    }
}

void AudioTransport::controls(float volume, float pan, float bufferSeconds) {
    if (!std::isfinite(volume) || !std::isfinite(pan) ||
        !std::isfinite(bufferSeconds) || volume < 0 || volume > 1 || pan < -1 ||
        pan > 1 || bufferSeconds < 0 || bufferSeconds > 5) {
        throw Error("Invalid volume, pan or buffer duration");
    }
    check(BASS_ChannelSetAttribute(stream_.get(), BASS_ATTRIB_VOL, volume),
          "Set volume");
    check(BASS_ChannelSetAttribute(stream_.get(), BASS_ATTRIB_PAN, pan),
          "Set pan");
    check(BASS_ChannelSetAttribute(stream_.get(), BASS_ATTRIB_BUFFER,
                                   bufferSeconds),
          "Set stream buffering");
}

void AudioTransport::effects(const FxSettings &settings) {
    const DWORD channel = stream_.get();
    check(BASS_ChannelLock(channel, TRUE), "Lock channel for FX change");
    struct Unlock {
        DWORD channel;
        ~Unlock() { BASS_ChannelLock(channel, FALSE); }
    } unlock{channel};
    // Build a replacement while mixing is paused. Failed parameter updates
    // remove only the candidate effects and leave the previous chain intact.
    FxChain next;
    next.apply(channel, info_.rate, settings);
    fx_ = std::move(next);
}
} // namespace gpp::audio
