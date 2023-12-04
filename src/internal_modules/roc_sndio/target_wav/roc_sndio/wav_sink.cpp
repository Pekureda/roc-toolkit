/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_sndio/wav_sink.h"
#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_core/endian_ops.h"
#include "roc_sndio/backend_map.h"

namespace roc {
namespace sndio {

WavSink::WavSink(core::IArena& arena, const Config& config)
    : output_file_(NULL)
    , header_(config.sample_spec.num_channels(),
              config.sample_spec.sample_rate(),
              16)
    , buffer_(arena)
    , buffer_size_(0)
    , valid_(false) {
    BackendMap::instance();

    if (config.sample_spec.num_channels() == 0) {
        roc_log(LogError, "wav sink: # of channels is zero");
        return;
    }

    if (config.latency != 0) {
        roc_log(LogError, "wav sink: setting io latency not supported by wav backend");
        return;
    }

    frame_length_ = config.frame_length;
    sample_spec_ = config.sample_spec;

    if (frame_length_ == 0) {
        roc_log(LogError, "wav sink: frame length is zero");
        return;
    }

    valid_ = true;
}

WavSink::~WavSink() {
    close_();
}

bool WavSink::is_valid() const {
    return valid_;
}

bool WavSink::open(const char* path) {
    roc_panic_if(!valid_);

    roc_log(LogDebug, "wav sink: opening: path=%s", path);

    if (!output_file_) {
        roc_panic("wav sink: can't call open() more than once");
    }

    if (!open_(path)) {
        return false;
    }

    if (!setup_buffer_()) {
        return false;
    }

    return true;
}

DeviceType WavSink::type() const {
    return DeviceType_Sink;
}

DeviceState WavSink::state() const {
    return DeviceState_Active;
}

void WavSink::pause() {
    // no-op
}

bool WavSink::resume() {
    return true;
}

bool WavSink::restart() {
    return true;
}

audio::SampleSpec WavSink::sample_spec() const {
    roc_panic_if(!valid_);

    if (!output_file_) {
        roc_panic("wav sink: sample_spec(): non-open output file or device");
    }

    if (header_.num_channels() == 1) {
        return audio::SampleSpec(size_t(header_.sample_rate()),
                                 audio::ChanLayout_Surround, audio::ChanOrder_Smpte,
                                 audio::ChanMask_Surround_Mono);
    }

    if (header_.num_channels() == 2) {
        return audio::SampleSpec(size_t(header_.sample_rate()),
                                 audio::ChanLayout_Surround, audio::ChanOrder_Smpte,
                                 audio::ChanMask_Surround_Stereo);
    }

    roc_panic("wav sink: unsupported channel count");
}

core::nanoseconds_t WavSink::latency() const {
    return 0;
}

bool WavSink::has_latency() const {
    roc_panic_if(!valid_);

    if (!output_file_) {
        roc_panic("wav sink: has_latency(): non-open output file or device");
    }

    return false;
}

bool WavSink::has_clock() const {
    roc_panic_if(!valid_);

    if (!output_file_) {
        roc_panic("wav sink: has_clock(): non-open output file or device");
    }

    return false;
}

void WavSink::write(audio::Frame& frame) {
    roc_panic_if(!valid_);

    const audio::sample_t* frame_data = frame.samples();
    size_t frame_size = frame.num_samples();

    audio::sample_t* buffer_data = buffer_.data();
    size_t buffer_pos = 0;

    while (frame_size > 0) { // ASK how saving should exactly work?
        for (; buffer_pos < buffer_size_ && frame_size > 0; buffer_pos++) {
            buffer_data[buffer_pos] = core::EndianOps::swap_native_le<audio::sample_t>(*frame_data); // Is this sufficient?
                                                            // Probably channels will be
                                                            // swapped but it's to be
                                                            // checked
            frame_data++;
            frame_size--;
        }

        if (buffer_pos == buffer_size_) {
            write_(buffer_data, buffer_pos);
            buffer_pos = 0;
        }
    }

    write_(buffer_data, buffer_pos);
}

bool WavSink::setup_buffer_() {
    buffer_size_ = sample_spec_.ns_2_samples_overall(frame_length_);
    if (buffer_size_ == 0) {
        roc_log(LogError, "wav sink: buffer size is zero");
        return false;
    }
    if (!buffer_.resize(buffer_size_)) {
        roc_log(LogError, "wav sink: can't allocate sample buffer");
        return false;
    }

    return true;
}

bool WavSink::open_(const char* path) {
    output_file_ = fopen(path, "w"); // TODO check w+ if needed
    if (!output_file_) {
        roc_log(LogDebug, "wav sink: can't open: path=%s", path);
        return false;
    }

    // TODO Check if this is out of concern, but probably is -> any coversion?
    // unsigned long in_rate = (unsigned long)out_signal_.rate;
    // unsigned long out_rate = (unsigned long)output_->signal.rate;

    // if (in_rate != 0 && in_rate != out_rate) {
    //     roc_log(LogError,
    //             "sox sink:"
    //             " can't open output file or device with the required sample rate:"
    //             " required_by_output=%lu requested_by_user=%lu",
    //             out_rate, in_rate);
    //     return false;
    // }

    // sample_spec_.set_sample_rate((unsigned long)output_->signal.rate);

    roc_log(LogInfo,
            "wav sink:"
            " opened: bits=%lu out_rate=%lu in_rate=%lu ch=%lu",
            (unsigned long)header_.bits_per_sample(), (unsigned long)header_.sample_rate(),
            (unsigned long)header_.sample_rate(), (unsigned long)header_.num_channels());

    return true;
}

void WavSink::write_(const audio::sample_t* samples, size_t n_samples) {
    if (n_samples > 0) {
        if (fseek(output_file_, 0, SEEK_SET) != 0) {
            roc_log(LogError, "wav sink: failed to seek to the beginning of the file");
        }

        char* header_bytes = header_.to_bytes(n_samples);
        if (fwrite(header_bytes, 1, sizeof(WavHeader), output_file_)
            != sizeof(WavHeader)) {
            roc_log(LogError, "wav sink: failed to write header");
        }

        if (fseek(output_file_, 0, SEEK_END) != 0) {
            roc_log(LogError, "wav sink: failed to seek to append position of the file");
        }

        if (fwrite(samples, sizeof(audio::sample_t), n_samples, output_file_)
            != n_samples) {
            roc_log(LogError, "wav sink: failed to write output buffer");
        }

        if (fflush(output_file_)) {
            roc_log(LogError, "wav sink: failed to flush data to the file");
        }
    }
}

void WavSink::close_() {
    if (!output_file_) {
        return;
    }

    roc_log(LogDebug, "wav sink: closing output");

    if (fclose(output_file_)) {
        roc_panic("wav sink: can't close output");
    }

    output_file_ = NULL;
}

} // namespace sndio
} // namespace roc
