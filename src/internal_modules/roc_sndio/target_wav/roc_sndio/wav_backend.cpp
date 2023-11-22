/*
 * Copyright (c) 2023 Roc Streaming authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define DR_WAV_IMPLEMENTATION

#include "roc_core/log.h"
#include "roc_sndio/wav_backend.h"
#include "roc_sndio/wav_sink.h"
#include "roc_sndio/wav_source.h"

namespace roc {
namespace sndio {

void WAVBackend::discover_drivers(core::Array<DriverInfo, MaxDrivers>& driver_list) {
    // TODO
}

IDevice* WAVBackend::open_device(DeviceType device_type,
                                 DriverType driver_type,
                                 const char* driver,
                                 const char* path,
                                 const Config& config,
                                 core::IArena& arena) {
    // TODO
    return NULL;
}


} // namespace sndio
} // namespace roc