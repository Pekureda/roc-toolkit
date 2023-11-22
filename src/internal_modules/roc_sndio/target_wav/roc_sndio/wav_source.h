/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_sndio/target_wav/roc_sndio/wav_source.h
//! @brief WAV source.

#ifndef ROC_SNDIO_WAV_SOURCE_H_ 
#define ROC_SNDIO_WAV_SOURCE_H_ 

#include <dr_wav.h>

#include "roc_audio/sample_spec.h"
#include "roc_core/array.h"
#include "roc_core/iarena.h"
#include "roc_core/noncopyable.h"
#include "roc_core/stddefs.h"
#include "roc_core/string_buffer.h"
#include "roc_packet/units.h"
#include "roc_sndio/config.h"
#include "roc_sndio/isource.h"

namespace roc {
namespace sndio {

//! WAV source.
class WAVSource : public ISource, private core::NonCopyable<> {
public:
    //! Initialize.
    WAVSource(core::IArena& arena, const Config& config);

    ~WAVSource();

    //! Open input file or device.
    //!
    //! @b Parameters
    //!  - @p path is input file or device name, "-" for stdin.
    //!
    //! @remarks
    //!  If @p path is NULL, defaults are used.
    bool open(const char* path);

    //! Get device type.
    virtual DeviceType type() const;

    //! Get device state.
    virtual DeviceState state() const;

    //! Pause reading.
    virtual void pause();

    //! Resume paused reading.
    virtual bool resume();

    //! Restart reading from the beginning.
    virtual bool restart(); // TODO

    //! Get sample specification of the source.
    virtual audio::SampleSpec sample_spec() const; // TODO

    //! Get latency of the source.
    virtual core::nanoseconds_t latency() const;

    //! Check if the source supports latency reports.
    virtual bool has_latency() const;

    //! Check if the source has own clock.
    virtual bool has_clock() const;

    //! Adjust source clock to match consumer clock.
    virtual void reclock(core::nanoseconds_t timestamp);

    //! Read frame.
    virtual bool read(audio::Frame& frame); // TODO

private:
    bool valid_;
    bool alreadyOpened_;
    bool eof_;
    bool paused_;
    core::StringBuffer input_name_;
    drwav wav_;
    drwav_int32* input_;
    core::Array<drwav_int32> buffer_;
    size_t buffer_size_;
    core::nanoseconds_t frame_length_;

    bool setup_names_(const char* path);
    bool open_();
    void close_();
    bool setup_buffer_();
};

} // namespace sndio
} // namespace roc

#endif // ROC_SNDIO_WAV_SOURCE_H_
