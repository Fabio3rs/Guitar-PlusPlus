#include "session.h"
#include <algorithm>
#include <chrono>

namespace AudioStudio {
void Session::processSfx(const Job &job, Completion &done,
                         const audio::Checkpoint &checkpoint) {
    if (job.kind == "sfx.load") {
        done.sfxDefinition = audio::sfx::load(job.source);
    } else if (job.kind == "sfx.generate") {
        const auto start = std::chrono::steady_clock::now();
        auto result = std::make_shared<SfxProduct>();
        result->definition = job.sfxDefinition;
        result->pcm = audio::sfx::render(job.sfxDefinition,
                                         [&](double p) { checkpoint(p * .9); });
        result->levels = audio::levels(result->pcm);

        auto &wave = result->waveform;
        const auto &pcm = result->pcm;
        wave.info = {pcm.rate, pcm.channels, pcm.seconds()};
        const size_t stride = std::max(size_t(1), (pcm.frames() + 2047) / 2048);
        wave.low.assign((pcm.frames() + stride - 1) / stride, 0);
        wave.high = wave.low;
        for (size_t i = 0; i < pcm.frames(); ++i) {
            if (i % 4096 == 0) {
                checkpoint(.9 + .05 * double(i) / pcm.frames());
            }

            for (unsigned ch = 0; ch < pcm.channels; ++ch) {
                const auto value = pcm.samples[i * pcm.channels + ch];
                wave.low[i / stride] = std::min(wave.low[i / stride], value);
                wave.high[i / stride] = std::max(wave.high[i / stride], value);
            }
        }

        checkpoint(.95);
        result->previewWav = std::make_shared<const std::vector<uint8_t>>(
            audio::wavBytes(pcm, true));
        checkpoint(1);

        result->milliseconds = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        done.sfxProduct = std::move(result);
    } else if (job.kind == "sfx.export") {
        audio::sfx::exportWav(job.output, job.sfxProduct->pcm, job.float32,
                              false, checkpoint);
    } else {
        throw audio::Error("Unknown SFX worker job");
    }
}

void Session::publishSfx(Completion &done) {
    if (done.sfxDefinition) {
        sfxDefinition = std::move(*done.sfxDefinition);
        sfxPath = done.job.source;
        ++sfxEdit;
        sfxStale = true;
        sfxDirty = false;
    }

    if (done.sfxProduct) {
        sfxPreview.close();
        sfxProduct = std::move(done.sfxProduct);
        sfxStale = done.job.sfxEdit != sfxEdit;
    }
}

uint64_t Session::loadSfx(const fs::path &path, bool discardEdits) {
    if (sfxDirty && !discardEdits) {
        throw audio::Error("Save the SFX draft or explicitly discard its edits "
                           "before loading");
    }

    Job job;
    job.kind = "sfx.load";
    job.source = fs::absolute(path);
    return enqueue(std::move(job));
}

void Session::newSfx(bool discardEdits) {
    if (sfxDirty && !discardEdits) {
        throw audio::Error("Save the SFX draft or explicitly discard its edits "
                           "before creating a recipe");
    }

    cancel();
    sfxDefinition = {};
    sfxDefinition.layers[0].type = audio::sfx::Generator::Pluck;
    sfxDefinition.layers[0].envelope = {.001, 0, 1, .05, true};
    sfxPath.clear();
    editSfx();
}

void Session::editSfx() {
    if (busy_ && jobKind == "sfx.load") {
        cancel();
    }

    ++sfxEdit;
    sfxStale = true;
    sfxDirty = true;
}

uint64_t Session::generateSfx() {
    audio::sfx::validate(sfxDefinition);
    Job job;
    job.kind = "sfx.generate";
    job.sfxDefinition = sfxDefinition;
    job.sfxEdit = sfxEdit;
    return enqueue(std::move(job));
}

uint64_t Session::exportSfx(const fs::path &path, bool float32) {
    if (!sfxProduct) {
        throw audio::Error("Generate an SFX before exporting");
    }

    Job job;
    job.kind = "sfx.export";
    job.output = artifact(path);
    job.sfxProduct = sfxProduct;
    job.float32 = float32;
    return enqueue(std::move(job));
}

void Session::saveSfx(const fs::path &path) {
    const auto target = path.is_absolute() ? path : outputRoot / path;
    // Save edits back only to the explicitly opened recipe. Save As never
    // overwrites another existing recipe, including hard links/symlinks.
    const bool replace = !sfxPath.empty() && fs::weakly_canonical(target) ==
                                                 fs::weakly_canonical(sfxPath);
    audio::sfx::save(target, sfxDefinition, replace);
    sfxPath = fs::absolute(target);
    sfxDirty = false;
    notice = "Saved recipe " + sfxPath.string();
}

void Session::playSfx(bool looping) {
    if (!sfxProduct) {
        throw audio::Error("Generate an SFX before previewing");
    }

    const auto &pcm = sfxProduct->pcm;
    if (!sfxPreview.handle()) {
        sfxPreview.openWav(sfxProduct->previewWav);
    }

    const double begin = pcm.loop.end ? double(pcm.loop.begin) / pcm.rate : 0;
    const double end =
        pcm.loop.end ? double(pcm.loop.end) / pcm.rate : pcm.seconds();
    sfxPreview.loop(begin, end, looping);
    sfxPreview.controls(sfxVolume, 0, streamBuffer);
    sfxPreview.position(0);
    sfxPreview.play();
    if (BASS_GetDevice() == 0) {
        notice =
            "No Sound: SFX generation/export work; preview is not audible.";
    }
}

void Session::stopSfx() {
    if (sfxPreview.handle()) {
        sfxPreview.stop();
    }
}
} // namespace AudioStudio
