/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "roc_sndio/wav_source.h"
#include "roc_core/log.h"
#include "roc_core/panic.h"
#include "roc_sndio/backend_map.h"

namespace roc {
namespace sndio {

WavSource::WavSource(core::IArena& arena, const Config& config)
    : input_name_(arena)
    , buffer_(arena)
    , buffer_size_(0)
    , eof_(false)
    , paused_(false)
    , valid_(false) {
    BackendMap::instance(); // Is there any reason it should be there as in SoX?

    if (config.sample_spec.num_channels() == 0) {
        roc_log(LogError, "wav source: # of channels is zero");
        return;
    }

    if (config.latency != 0) {
        roc_log(LogError, "wav source: setting io latency not supported by wav backend");
        return;
    }

    frame_length_ = config.frame_length;
    roc_log(LogInfo, "DEBUG: frame_length: %ld", frame_length_);
    sample_spec_ = config.sample_spec;

    if (frame_length_ == 0) {
        roc_log(LogError, "wav source: frame length is zero");
        return;
    }

    valid_ = true;
}

WavSource::~WavSource() {
    close_();
}

bool WavSource::is_valid() const {
    return valid_;
}

bool WavSource::open(const char* path) {
    roc_panic_if(!valid_);

    roc_log(LogInfo, "wav source: opening: path=%s", path);

    if (file_opened_) {
        roc_panic("wav source: can't call open() more than once");
    }

    if (!setup_names_(path)) {
        return false;
    }

    if (!open_()) {
        return false;
    }

    if (!setup_buffer_()) {
        return false;
    }

    file_opened_ = true;
    return true;
}

DeviceType WavSource::type() const {
    return DeviceType_Source;
}

DeviceState WavSource::state() const {
    roc_panic_if(!valid_);

    if (paused_) {
        return DeviceState_Paused;
    } else {
        return DeviceState_Active;
    }
}

void WavSource::pause() {
    // no-op - but the state is updated
    paused_ = true;
}

bool WavSource::resume() {
    // no-op - but the state is updated
    paused_ = false;
    return true;
}

bool WavSource::restart() {
    roc_panic_if(!valid_);

    roc_log(LogDebug, "wav source: restarting: input=%s", input_name_.c_str());

    if (!seek_(0)) {
        roc_log(LogError, "wav source: seek failed when restarting: input=%s",
                input_name_.c_str());
        return false;
    }

    paused_ = false;
    eof_ = false;

    return true;
}

audio::SampleSpec WavSource::sample_spec() const {
    roc_panic_if(!valid_);

    if (!file_opened_) {
        roc_panic("wav source: sample_spec(): non-open output file or device");
    }

    if (wav_.channels == 1) {
        return audio::SampleSpec(size_t(wav_.sampleRate), audio::ChanLayout_Surround,
                                 audio::ChanOrder_Smpte, audio::ChanMask_Surround_Mono);
    }

    if (wav_.channels == 2) {
        return audio::SampleSpec(size_t(wav_.sampleRate), audio::ChanLayout_Surround,
                                 audio::ChanOrder_Smpte, audio::ChanMask_Surround_Stereo);
    }

    // TODO consider more channels
    roc_panic("wav source: unsupported channel count");
}

core::nanoseconds_t WavSource::latency() const {
    return 0;
}

bool WavSource::has_latency() const {
    roc_panic_if(!valid_);

    if (!file_opened_) {
        roc_panic("wav source: has_latency(): non-open input file or device");
    }

    return false;
}

bool WavSource::has_clock() const {
    roc_panic_if(!valid_);

    if (!file_opened_) {
        roc_panic("wav source: has_clock(): non-open input file or device");
    }

    return false;
}

void WavSource::reclock(core::nanoseconds_t timestamp) {
    // no-op
    (void)timestamp;
}

bool WavSource::read(audio::Frame& frame) {
    roc_panic_if(!valid_);

    if (paused_ || eof_) {
        return false;
    }

    if (!file_opened_) {
        roc_panic("wav source: read: non-open input file");
    }

    audio::sample_t* frame_data = frame.samples();
    size_t frame_left = frame.num_samples();

    audio::sample_t* buffer_data = buffer_.data();

    while (frame_left != 0) {
        size_t n_samples = frame_left;
        if (n_samples > buffer_size_) {
            n_samples = buffer_size_;
        }
        
        drwav_uint64 frames_read = drwav_read_pcm_frames_f32(&wav_, n_samples, buffer_data);
        if (frames_read < n_samples) {
            roc_log(LogDebug, "wav source: got eof from wav");
            eof_ = true;
            break;
        }

        memcpy(frame_data, buffer_data, frames_read * sizeof(audio::sample_t));

        frame_data += frames_read;
        frame_left -= frames_read;
    }

    if (frame_left == frame.num_samples()) {
        return false;
    }

    if (frame_left != 0) {
        memset(frame_data, 0, frame_left * sizeof(audio::sample_t));
    }

    return true;
}

bool WavSource::setup_names_(const char* path) {
    if (path) {
        if (!input_name_.assign(path)) {
            roc_log(LogError, "sox source: can't allocate string");
            return false;
        }
    }

    return true;
}

bool WavSource::open_() {
    if (file_opened_) {
        roc_panic("wav source: already opened");
    }

    if (!drwav_init_file(&wav_, input_name_.c_str(), NULL)) {
        roc_log(LogInfo, "wav source: can't open: input=%s", input_name_.c_str());
        return false;
    }

    if (wav_.sampleRate != sample_spec_.sample_rate()) {
        roc_log(LogError,
                "wav source: requested sampling rate %lu but file has %lu: input=%s",
                (unsigned long)sample_spec_.sample_rate(), (unsigned long)wav_.sampleRate,
                input_name_.c_str());
        close_();
        return false;
    }

    sample_spec_.channel_set().set_channel_range(
        0, sample_spec_.channel_set().max_channels() - 1, false);
    sample_spec_.channel_set().set_channel_range(0, wav_.channels, true);

    roc_log(LogInfo,
            "wav source:"
            " in_bits=%lu out_bits=%lu in_rate=%lu out_rate=%lu"
            " in_ch=%lu out_ch=%lu",
            (unsigned long)wav_.bitsPerSample, (unsigned long)wav_.bitsPerSample,
            (unsigned long)wav_.sampleRate, (unsigned long)wav_.sampleRate,
            (unsigned long)wav_.channels, (unsigned long)wav_.channels);

    return true;
}

void WavSource::close_() {
    if (!file_opened_) {
        return;
    }

    file_opened_ = false;
    // TODO make sure it's everything that must be done to close
    drwav_uninit(&wav_);
}

bool WavSource::setup_buffer_() {
    // TODO the logic below can be extracted to helper method at least, consider TODO
    // below
    const float total_samples =
        roundf(float(frame_length_) / core::Second * wav_.sampleRate * wav_.channels);
    const size_t min_val = ROC_MIN_OF(size_t);
    const size_t max_val = ROC_MAX_OF(size_t);

    if (total_samples * wav_.channels <= min_val) {
        buffer_size_ = min_val / wav_.channels * wav_.channels; // 0
    } else if (total_samples * wav_.channels >= (float)max_val) {
        buffer_size_ = max_val / wav_.channels * wav_.channels;
    } else {
        buffer_size_ = (size_t)total_samples * wav_.channels; // TODO see if this logic is
                                                              // sufficient - Change will
                                                              // probably require
                                                              // tinkering with SampleSpec
                                                              // but let's skip it at this
                                                              // point
    }
    // buffer_size_ = sample_spec_.ns_2_samples_overall(frame_length_); // TODO check later
    //                                                                  // if it properly
    //                                                                  // converts. If not
    //                                                                  // try code above

    if (buffer_size_ == 0) {
        roc_log(LogError, "wav source: buffer size is zero");
        return false;
    }
    if (!buffer_.resize(buffer_size_)) {
        roc_log(LogError, "wav source: can't allocate sample buffer");
        return false;
    }

    return true;
}

bool WavSource::seek_(drwav_uint64 target_frame_index) {
    return drwav_seek_to_pcm_frame(&wav_, target_frame_index);
}

} // namespace sndio
} // namespace roc
