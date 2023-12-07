/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "wav_header.h"
#include "roc_core/endian_ops.h"

namespace roc {
namespace sndio {

namespace {
using core::EndianOps;
}

WavHeader::WavHeader(uint16_t num_channels,
                     uint32_t sample_rate,
                     uint16_t bits_per_sample)
    : chunk_id_(EndianOps::swap_native_be<uint32_t>(0x52494646))     // {'R','I','F','F'}
    , format_(EndianOps::swap_native_be<uint32_t>(0x57415645))       // {'W','A','V','E'}
    , subchunk1_id_(EndianOps::swap_native_be<uint32_t>(0x666d7420)) // {'f','m','t',''}
    , subchunk1_size_(EndianOps::swap_native_le<uint16_t>(0x20))     // 32
    , audio_format_(EndianOps::swap_native_le<uint16_t>(0x1))        // No compression
    , num_channels_(EndianOps::swap_native_le(num_channels))
    , sample_rate_(EndianOps::swap_native_le<uint32_t>(sample_rate))
    , byte_rate_(EndianOps::swap_native_le<uint32_t>(sample_rate * num_channels
                                                     * (bits_per_sample / 8u)))
    , block_align_(
          EndianOps::swap_native_le<uint16_t>(num_channels * (bits_per_sample / 8u)))
    , bits_per_sample_(EndianOps::swap_native_le<uint16_t>(bits_per_sample))
    , subchunk2_id_(
          EndianOps::swap_native_be<uint32_t>(0x64617461)) /* {'d','a','t','a'} */ {
}

uint16_t WavHeader::num_channels() const {
    return num_channels_;
}

uint32_t WavHeader::sample_rate() const {
    return sample_rate_;
}

uint16_t WavHeader::bits_per_sample() const {
    return bits_per_sample_;
}

// NOTE Each sample is 4B -> that is expected
char* WavHeader::to_bytes(uint32_t num_samples) {
    // TODO may be optimized but let's leave it simple for now
    subchunk2_size_ = num_samples * num_channels_ * (bits_per_sample_ / 8u);
    chunk_size_ = 36u + subchunk2_size_;

    char* header_data = new char[sizeof(WavHeader)];
    uint32_t offset = 0;

    memcpy(header_data + offset, &chunk_id_, sizeof(chunk_id_));
    offset += sizeof(chunk_id_);
    memcpy(header_data + offset, &chunk_size_, sizeof(chunk_size_));
    offset += sizeof(chunk_size_);
    memcpy(header_data + offset, &format_, sizeof(format_));
    offset += sizeof(format_);
    memcpy(header_data + offset, &subchunk1_id_, sizeof(subchunk1_id_));
    offset += sizeof(subchunk1_id_);
    memcpy(header_data + offset, &subchunk1_size_, sizeof(subchunk1_size_));
    offset += sizeof(subchunk1_size_);
    memcpy(header_data + offset, &audio_format_, sizeof(audio_format_));
    offset += sizeof(audio_format_);
    memcpy(header_data + offset, &num_channels_, sizeof(num_channels_));
    offset += sizeof(num_channels_);
    memcpy(header_data + offset, &sample_rate_, sizeof(sample_rate_));
    offset += sizeof(sample_rate_);
    memcpy(header_data + offset, &byte_rate_, sizeof(byte_rate_));
    offset += sizeof(byte_rate_);
    memcpy(header_data + offset, &block_align_, sizeof(block_align_));
    offset += sizeof(block_align_);
    memcpy(header_data + offset, &bits_per_sample_, sizeof(bits_per_sample_));
    offset += sizeof(bits_per_sample_);
    memcpy(header_data + offset, &subchunk2_id_, sizeof(subchunk2_id_));

    return header_data;
}

} // namespace sndio
} // namespace roc
