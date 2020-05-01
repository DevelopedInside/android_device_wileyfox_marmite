/*
 * Copyright (C) 2015 The CyanogenMod Open Source Project
 * Copyright (C) 2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "amplifier_marmite"
//#define LOG_NDEBUG 0

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/audio_amplifier.h>
#include <hardware/hardware.h>

#include <tinyalsa/asoundlib.h>

#include <msm8916/platform.h>
#include "audio_hw.h"

#define AMPLIFIER_PROP_KEY                 "ro.hardware.amplifier"

/* AW87319 */
#define AMP_SPKR_BOOST_MIXER_CTL           "SpkrMono BOOST Switch"
#define AMP_EAR_PA_BOOST_MIXER_CTL         "EAR PA Boost"
#define AMP_RX2_MIXER_CTL                  "RX2 MIX1 INP1"
#define AMP_RDAC2_MIXER_CTL                "RDAC2 MUX"
#define AMP_HPHR_MIXER_CTL                 "HPHR"
#define AMP_SPK_SWITCH_MIXER_CTL           "Ext Spk Switch"

/* Default MTP */
#define MTP_RX3_MIXER_CTL                  "RX3 MIX1 INP1"
#define MTP_SPK_MIXER_CTL                  "SPK"

static bool isAW87319 = false;

static int is_speaker(uint32_t snd_device) {
    int speaker = 0;

    switch (snd_device) {
        case SND_DEVICE_OUT_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_REVERSE:
        case SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES:
        case SND_DEVICE_OUT_VOICE_SPEAKER:
        case SND_DEVICE_OUT_SPEAKER_AND_HDMI:
        case SND_DEVICE_OUT_SPEAKER_AND_USB_HEADSET:
        case SND_DEVICE_OUT_SPEAKER_AND_ANC_HEADSET:
            speaker = 1;
            break;
    }

    return speaker;
}

static void mixer_set_int(struct mixer* mixer, const char* name, int value)
{
    /* Acquire control by name */
    struct mixer_ctl* ctl = mixer_get_ctl_by_name(mixer, name);
    if (!ctl) {
        ALOGE("Mixer: Invalid mixer control (%s)\n", name);
        return;
    }

    /* Set value to control */
    if (mixer_ctl_set_value(ctl, 0, value)) {
        ALOGE("Mixer: Invalid value for index 0\n");
        return;
    }
}

static void mixer_set_enum(struct mixer* mixer, const char* name,
        const char* value)
{
    /* Acquire control by name */
    struct mixer_ctl* ctl = mixer_get_ctl_by_name(mixer, name);
    if (!ctl) {
        ALOGE("Mixer: Invalid mixer control (%s)\n", name);
        return;
    }

    /* Set value to control */
    if (mixer_ctl_set_enum_by_string(ctl, value)) {
        ALOGE("Mixer: Invalid enum value\n");
        return;
    }
}

static int amp_set_input_devices(amplifier_device_t *device, uint32_t devices)
{
    return 0;
}

static int amp_set_output_devices(amplifier_device_t *device, uint32_t devices)
{
    return 0;
}

static int amp_enable_output_devices(amplifier_device_t *device,
        uint32_t devices, bool enable)
{
    if (is_speaker(devices)) {
        /* Acquire mixer card */
        struct mixer* mixer = mixer_open(0);
        if (!mixer) {
            ALOGE("Mixer: Failed to open mixer\n");
            return -errno;
        }

        if (isAW87319) {
            /* Apply mixers for AW87319 */
            mixer_set_int(mixer, AMP_SPKR_BOOST_MIXER_CTL, 0);
            mixer_set_enum(mixer, AMP_EAR_PA_BOOST_MIXER_CTL, "DISABLE");
            mixer_set_enum(mixer, AMP_RX2_MIXER_CTL, enable ? "RX1" : "ZERO");
            mixer_set_enum(mixer, AMP_RX2_MIXER_CTL, enable ? "RX2" : "ZERO");
            mixer_set_enum(mixer, AMP_RDAC2_MIXER_CTL, enable ? "RX2" : "ZERO");
            mixer_set_enum(mixer, AMP_HPHR_MIXER_CTL, enable ? "Switch" : "ZERO");
            mixer_set_enum(mixer, AMP_SPK_SWITCH_MIXER_CTL, enable ? "On" : "Off");
        } else {
            /* Apply mixers for default MTP */
            mixer_set_enum(mixer, MTP_RX3_MIXER_CTL, enable ? "RX1" : "ZERO");
            mixer_set_enum(mixer, MTP_SPK_MIXER_CTL, enable ? "Switch" : "ZERO");
        }

        /* Release mixer card */
        mixer_close(mixer);
    }
    return 0;
}

static int amp_enable_input_devices(amplifier_device_t *device,
        uint32_t devices, bool enable)
{
    return 0;
}

static int amp_set_mode(amplifier_device_t *device, audio_mode_t mode)
{
    return 0;
}

static int amp_output_stream_start(amplifier_device_t *device,
        struct audio_stream_out *stream, bool offload)
{
    return 0;
}

static int amp_input_stream_start(amplifier_device_t *device,
        struct audio_stream_in *stream)
{
    return 0;
}

static int amp_output_stream_standby(amplifier_device_t *device,
        struct audio_stream_out *stream)
{
    return 0;
}

static int amp_input_stream_standby(amplifier_device_t *device,
        struct audio_stream_in *stream)
{
    return 0;
}

static int amp_set_parameters(struct amplifier_device *device,
        struct str_parms *parms)
{
    return 0;
}

static int amp_out_set_parameters(struct amplifier_device *device,
        struct str_parms *parms)
{
    return 0;
}

static int amp_in_set_parameters(struct amplifier_device *device,
        struct str_parms *parms)
{
    return 0;
}

static int amp_dev_close(hw_device_t *device)
{
    if (device)
        free(device);

    return 0;
}

static int amp_module_open(const hw_module_t *module, const char *name,
        hw_device_t **device)
{
    if (strcmp(name, AMPLIFIER_HARDWARE_INTERFACE)) {
        ALOGE("%s:%d: %s does not match amplifier hardware interface name\n",
                __func__, __LINE__, name);
        return -ENODEV;
    }

    amplifier_device_t *amp_dev = calloc(1, sizeof(amplifier_device_t));
    if (!amp_dev) {
        ALOGE("%s:%d: Unable to allocate memory for amplifier device\n",
                __func__, __LINE__);
        return -ENOMEM;
    }

    /* check amplifier */
    isAW87319 = (bool) property_get_bool(AMPLIFIER_PROP_KEY, false);

    amp_dev->common.tag = HARDWARE_DEVICE_TAG;
    amp_dev->common.module = (hw_module_t *) module;
    amp_dev->common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    amp_dev->common.close = amp_dev_close;

    amp_dev->set_input_devices = amp_set_input_devices;
    amp_dev->set_output_devices = amp_set_output_devices;
    amp_dev->enable_output_devices = amp_enable_output_devices;
    amp_dev->enable_input_devices = amp_enable_input_devices;
    amp_dev->set_mode = amp_set_mode;
    amp_dev->output_stream_start = amp_output_stream_start;
    amp_dev->input_stream_start = amp_input_stream_start;
    amp_dev->output_stream_standby = amp_output_stream_standby;
    amp_dev->input_stream_standby = amp_input_stream_standby;
    amp_dev->set_parameters = amp_set_parameters;
    amp_dev->out_set_parameters = amp_out_set_parameters;
    amp_dev->in_set_parameters = amp_in_set_parameters;

    *device = (hw_device_t *) amp_dev;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "Marmite audio amplifier HAL",
        .author = "The LineageOS Project",
        .methods = &hal_module_methods,
    },
};
