/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_sndio/wav_header.h
//! @brief WAV header.

#ifndef ROC_SNDIO_WAV_HEADER_H_
#define ROC_SNDIO_WAV_HEADER_H_

#include "roc_core/stddefs.h"

namespace roc {
namespace sndio {

//! WAV header
//! @remarks
//!  Holds data of a WAV header
//!  Allows easy generation of WAV header
class WavHeader {
public:
    //! Initialize
    WavHeader(uint16_t num_channels, uint32_t sample_rate, uint16_t bits_per_sample);

    //! Get number of channels
    uint16_t num_channels() const;

    //! Get sample rate
    uint32_t sample_rate() const;

    //! Get number of bits per sample
    uint16_t bits_per_sample() const;

    //! Resets samples counter
    void reset_sample_counter(uint32_t num_samples);

    //! Convert header data to byte array
    //! @remarks
    //!  User is the owner of the array returned
    char* to_bytes(uint32_t num_samples);

private:
    // RIFF header
    const uint32_t chunk_id_;
    uint32_t chunk_size_;
    const uint32_t format_;
    // WAVE fmt
    const uint32_t subchunk1_id_;
    const uint32_t subchunk1_size_;
    const uint16_t audio_format_;
    const uint16_t num_channels_;
    const uint32_t sample_rate_;
    const uint32_t byte_rate_;
    const uint16_t block_align_;
    const uint16_t bits_per_sample_;
    // WAVE data
    const uint32_t subchunk2_id_;
    uint32_t subchunk2_size_;

    // Help data
    uint32_t num_samples_;
};

} // namespace sndio
} // namespace roc

#endif // ROC_SNDIO_WAV_HEADER_H_
