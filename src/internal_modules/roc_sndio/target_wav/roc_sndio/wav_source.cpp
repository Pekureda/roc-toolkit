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
    : valid_(false)
    , eof_(false)
    , paused_(false)
    , input_name_(arena)
    , input_(NULL)
    , buffer_(arena)
    , buffer_size_(0) { 
    BackendMap::instance(); // Is there any reason it should be there as in SoX?

    // Should other fields of config be ignored, no logs generated
    // or should some logs be generated if values are not `default`
    frame_length_ = config.frame_length;

    valid_ = true;
}

WavSource::~WavSource() {
    close_();
}

bool WavSource::open(const char* path) {
    roc_panic_if(!valid_);

    roc_log(LogInfo, "wav source: opening: path=%s", path);

    if (wav_.pUserData != NULL) {
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

    already_opened_ = true;
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

bool WavSource::setup_buffer_() {
    // TODO possibly reuse sample_spec from config but update {sample rate, channel count(how, meaning of ChannelSet?)}
    audio::SampleSpec sample_spec; // Dummy for conversion -> how to take care of conversion? Code for it shouldn't be copied -!> improve arch
    buffer_size_ = sample_spec.ns_2_samples_overall(frame_length_); // Now it's incorrect

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

bool WavSource::open_() {
    if (already_opened_) {
        roc_panic("wav source: already opened");
    }

    if (!drwav_init_file(&wav_, input_name_.c_str(), NULL)) {
        roc_log(LogInfo, "wav source: can't open: input=%s",
                input_name_.c_str());
        return false;
    }

    // TODO revisit
    drwav_uint64 frames_read = drwav_read_pcm_frames(&wav_, wav_.totalPCMFrameCount, input_);

    roc_log(LogInfo,
            "wav source:"
            " in_bits=%lu out_bits=%lu in_rate=%lu out_rate=%lu"
            " in_ch=%lu out_ch=%lu",
            (unsigned long)wav_.bitsPerSample, (unsigned long)wav_.bitsPerSample,
            (unsigned long)wav_.sampleRate, (unsigned long)wav_.sampleRate,
            (unsigned long)wav_.channels, (unsigned long)wav_.channels);

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

void WavSource::pause() {
    roc_panic_if(!valid_);

    if (paused_) {
        return;
    }

    if (!input_) {
        roc_panic("wav source: pause: non-open input file or device");
    }

    roc_log(LogDebug, "wav source: pausing: input=%s", input_name_.c_str());

    paused_ = true;
}

bool WavSource::resume() {
    roc_panic_if(!valid_);

    if (!paused_) {
        return true;
    }

    roc_log(LogDebug, "wav source: resuming: input=%s", input_name_.c_str());

    if (!input_) {
        if (!open_()) { // TODO Unnecessary depth -> move to outside if;; Name of the file must be known beforehand so an open operations should have been performed already, possibly guaranteed fail.
            roc_log(LogError, "wav source: open failed when resuming: input=%s",
                input_name_.c_str());
            return false;
        }
    }

    paused_ = false;
    return true;
}

bool WavSource::restart() {
    roc_panic_if(!valid_);

    roc_log(LogDebug, "sox source: restarting: input=%s", input_name_.c_str());

    // TODO
    // if (is_file_ && !eof_) {
    //     if (!seek_(0)) {
    //         roc_log(LogError,
    //                 "sox source: seek failed when restarting: driver=%s input=%s",
    //                 driver_name_.c_str(), input_name_.c_str());
    //         return false;
    //     }
    // } else {
    //     if (input_) {
    //         close_();
    //     }

    //     if (!open_()) {
    //         roc_log(LogError,
    //                 "sox source: open failed when restarting: driver=%s input=%s",
    //                 driver_name_.c_str(), input_name_.c_str());
    //         return false;
    //     }
    // }

    paused_ = false;
    eof_ = false;

    return true;
}

void WavSource::close_() {
    // TODO make sure it's everything that must be done to close
    drwav_uninit(&wav_);
}

core::nanoseconds_t WavSource::latency() const {
    return 0;
}

bool WavSource::has_latency() const {
    return false;
}

bool WavSource::has_clock() const {
    return false;
}

void WavSource::reclock(core::nanoseconds_t timestamp) {
    // no-op
}

}
}