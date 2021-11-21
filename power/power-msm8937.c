/*
 * Copyright (c) 2015,2018 The Linux Foundation. All rights reserved.
 * Copyright (C) 2018-2019 The LineageOS Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_NIDEBUG 0

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define LOG_TAG "QTI PowerHAL"
#include <hardware/hardware.h>
#include <hardware/power.h>
#include <log/log.h>

#include "hint-data.h"
#include "metadata-defs.h"
#include "performance.h"
#include "power-common.h"
#include "utils.h"

static int video_encode_hint_sent;

static int display_fd;
#define SYS_DISPLAY_PWR "/sys/kernel/hbtp/display_pwr"

const int kMinInteractiveDuration = 500;  /* ms */
const int kMaxInteractiveDuration = 5000; /* ms */
const int kMaxLaunchDuration = 5000;      /* ms */

static int current_power_profile = PROFILE_BALANCED;

// clang-format off
static int profile_high_performance[] = {
    SCHED_BOOST_ON_V3, 0x1,
    ALL_CPUS_PWR_CLPS_DIS_V3, 0x1,
    CPUS_ONLINE_MIN_BIG, 0x4,
    MIN_FREQ_BIG_CORE_0, 0xFFF,
    MIN_FREQ_LITTLE_CORE_0, 0xFFF,
    GPU_MIN_POWER_LEVEL, 0x1,
    SCHED_PREFER_IDLE_DIS_V3, 0x1,
    SCHED_SMALL_TASK, 0x1,
    SCHED_MOSTLY_IDLE_NR_RUN, 0x1,
    SCHED_MOSTLY_IDLE_LOAD, 0x1,
};

static int profile_power_save[] = {
    CPUS_ONLINE_MAX_BIG, 0x1,
    MAX_FREQ_BIG_CORE_0, 0x3bf,
    MAX_FREQ_LITTLE_CORE_0, 0x300,
};

static int profile_bias_power[] = {
    MAX_FREQ_BIG_CORE_0, 0x4B0,
    MAX_FREQ_LITTLE_CORE_0, 0x300,
};

static int profile_bias_performance[] = {
    CPUS_ONLINE_MAX_BIG, 0x4,
    MIN_FREQ_BIG_CORE_0, 0x540,
};
// clang-format on

#define VENDOR_HINT_ANIM_BOOST 0x00001083

#ifdef INTERACTION_BOOST
int get_number_of_profiles() {
    return 5;
}
#endif

static int set_power_profile(void* data) {
    int profile = data ? *((int*)data) : 0;
    int ret = -EINVAL;
    const char* profile_name = NULL;

    if (profile == current_power_profile) return 0;

    ALOGV("%s: Profile=%d", __func__, profile);

    if (current_power_profile != PROFILE_BALANCED) {
        undo_hint_action(DEFAULT_PROFILE_HINT_ID);
        ALOGV("%s: Hint undone", __func__);
        current_power_profile = PROFILE_BALANCED;
    }

    if (profile == PROFILE_POWER_SAVE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_power_save,
                                  ARRAY_SIZE(profile_power_save));
        profile_name = "powersave";

    } else if (profile == PROFILE_HIGH_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_high_performance,
                                  ARRAY_SIZE(profile_high_performance));
        profile_name = "performance";

    } else if (profile == PROFILE_BIAS_POWER) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_bias_power,
                                  ARRAY_SIZE(profile_bias_power));
        profile_name = "bias power";

    } else if (profile == PROFILE_BIAS_PERFORMANCE) {
        ret = perform_hint_action(DEFAULT_PROFILE_HINT_ID, profile_bias_performance,
                                  ARRAY_SIZE(profile_bias_performance));
        profile_name = "bias perf";
    } else if (profile == PROFILE_BALANCED) {
        ret = 0;
        profile_name = "balanced";
    }

    if (ret == 0) {
        current_power_profile = profile;
        ALOGD("%s: Set %s mode", __func__, profile_name);
    }
    return ret;
}

/**
 * Returns true if the target is SDM439/SDM429.
 */
static bool is_target_SDM439(void) {
    static int is_SDM439 = -1;
    int soc_id;

    if (is_SDM439 >= 0) return is_SDM439;

    soc_id = get_soc_id();
    is_SDM439 = soc_id == 353 || soc_id == 363 || soc_id == 354 || soc_id == 364;

    return is_SDM439;
}

static int process_video_encode_hint(void* metadata) {
    char governor[80];
    struct video_encode_metadata_t video_encode_metadata;

    if (!metadata) return HINT_NONE;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");
        return HINT_NONE;
    }

    /* Initialize encode metadata struct fields */
    memset(&video_encode_metadata, 0, sizeof(struct video_encode_metadata_t));
    video_encode_metadata.state = -1;
    video_encode_metadata.hint_id = DEFAULT_VIDEO_ENCODE_HINT_ID;

    if (parse_video_encode_metadata((char*)metadata, &video_encode_metadata) == -1) {
        ALOGE("Error occurred while parsing metadata.");
        return HINT_NONE;
    }

    if (video_encode_metadata.state == 1) {
        if (is_schedutil_governor(governor)) {
            if (is_target_SDM439()) {
                /* sample_ms = 10mS
                 * SLB for Core0 = -6
                 * SLB for Core1 = -6
                 * SLB for Core2 = -6
                 * SLB for Core3 = -6
                 * hispeed load = 95
                 * hispeed freq = 998Mhz */
                int resource_values[] = {CPUBW_HWMON_SAMPLE_MS,
                                         0xa,
                                         0x40c68100,
                                         0xfffffffa,
                                         0x40c68110,
                                         0xfffffffa,
                                         0x40c68120,
                                         0xfffffffa,
                                         0x40c68130,
                                         0xfffffffa,
                                         0x41440100,
                                         0x5f,
                                         0x4143c100,
                                         0x3e6};
                if (!video_encode_hint_sent) {
                    perform_hint_action(video_encode_metadata.hint_id, resource_values,
                                        ARRAY_SIZE(resource_values));
                    video_encode_hint_sent = 1;
                    return HINT_HANDLED;
                }
            } else {
                /* sample_ms = 10mS */
                int resource_values[] = {CPUBW_HWMON_SAMPLE_MS, 0xa};
                if (!video_encode_hint_sent) {
                    perform_hint_action(video_encode_metadata.hint_id, resource_values,
                                        ARRAY_SIZE(resource_values));
                    video_encode_hint_sent = 1;
                    return HINT_HANDLED;
                }
            }
        } else if (is_interactive_governor(governor)) {
            /* Sched_load and migration_notification disable
             * timer rate - 40mS*/
            int resource_values[] = {INT_OP_CLUSTER0_USE_SCHED_LOAD,      0x1,
                                     INT_OP_CLUSTER1_USE_SCHED_LOAD,      0x1,
                                     INT_OP_CLUSTER0_USE_MIGRATION_NOTIF, 0x1,
                                     INT_OP_CLUSTER1_USE_MIGRATION_NOTIF, 0x1,
                                     INT_OP_CLUSTER0_TIMER_RATE,          BIG_LITTLE_TR_MS_40,
                                     INT_OP_CLUSTER1_TIMER_RATE,          BIG_LITTLE_TR_MS_40};
            if (!video_encode_hint_sent) {
                perform_hint_action(video_encode_metadata.hint_id, resource_values,
                                    ARRAY_SIZE(resource_values));
                video_encode_hint_sent = 1;
                return HINT_HANDLED;
            }
        }
    } else if (video_encode_metadata.state == 0) {
        if (is_interactive_governor(governor) || is_schedutil_governor(governor)) {
            undo_hint_action(video_encode_metadata.hint_id);
            video_encode_hint_sent = 0;
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}

static void process_interaction_hint(void* data) {
    static struct timespec s_previous_boost_timespec;
    static int s_previous_duration = 0;
    static int interaction_handle = -1;

    struct timespec cur_boost_timespec;
    long long elapsed_time;
    int duration = kMinInteractiveDuration;

    if (data) {
        int input_duration = *((int*)data);
        if (input_duration > duration) {
            duration = (input_duration > kMaxInteractiveDuration) ? kMaxInteractiveDuration
                                                                  : input_duration;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &cur_boost_timespec);

    elapsed_time = calc_timespan_us(s_previous_boost_timespec, cur_boost_timespec);
    // don't hint if previous hint's duration covers this hint's duration
    if ((s_previous_duration * 1000) > (elapsed_time + duration * 1000)) {
        return;
    }
    s_previous_boost_timespec = cur_boost_timespec;
    s_previous_duration = duration;

    if (CHECK_HANDLE(interaction_handle)) {
        release_request(interaction_handle);
    }

    if (duration > kMinInteractiveDuration) {
        interaction_handle = perf_hint_enable_with_type(VENDOR_HINT_SCROLL_BOOST, duration, 
                                                        SCROLL_VERTICAL);
    }
}

static int process_activity_launch_hint(void* data) {
    static int launch_handle = -1;
    static int launch_handle_packing = -1;
    static int launch_mode = 0;

    // release lock early if launch has finished
    if (!data) {
        if (CHECK_HANDLE(launch_handle)) {
            release_request(launch_handle);
            launch_handle = -1;
        }
        if (CHECK_HANDLE(launch_handle_packing)) {
            release_request(launch_handle_packing);
            launch_handle_packing = -1;
        }
        launch_mode = 0;
        // boost for end animations
        perf_hint_enable_with_type(VENDOR_HINT_ANIM_BOOST, -1, -1);
        return HINT_HANDLED;
    }

    if (!launch_mode) {
        launch_handle = perf_hint_enable_with_type(VENDOR_HINT_FIRST_LAUNCH_BOOST,
                                                   kMaxLaunchDuration, LAUNCH_BOOST_V1);
        if (!CHECK_HANDLE(launch_handle)) {
            ALOGE("Failed to perform launch boost");
            return HINT_NONE;
        }
        launch_handle_packing = perf_hint_enable_with_type(VENDOR_HINT_FIRST_LAUNCH_BOOST,
                kMaxLaunchDuration, LAUNCH_BOOST_V2);
        if (!CHECK_HANDLE(launch_handle_packing)) {
            ALOGE("Failed to perform launch boost packing");
            return HINT_NONE;
        }
        launch_mode = 1;
    }
    return HINT_HANDLED;
}

int power_hint_override(power_hint_t hint, void* data) {
    int ret_val = HINT_NONE;

    if (hint == POWER_HINT_SET_PROFILE) {
        if (set_power_profile(data) < 0) ALOGE("Setting power profile failed. perfd not started?");
        return HINT_HANDLED;
    }

    // Skip other hints in high/low power modes
    if (current_power_profile == PROFILE_POWER_SAVE ||
        current_power_profile == PROFILE_HIGH_PERFORMANCE) {
        return HINT_HANDLED;
    }

    switch (hint) {
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_INTERACTION:
            process_interaction_hint(data);
            ret_val = HINT_HANDLED;
            break;
        case POWER_HINT_LAUNCH:
            ret_val = process_activity_launch_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}

int set_interactive_override(int on) {
    char governor[80];
    int rc = 0;

    static const char* display_on = "1";
    static const char* display_off = "0";
    char err_buf[80];
    static int init_interactive_hint = 0;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");
        return HINT_NONE;
    }

    if (!on) {
        /* Display off */
        if (is_interactive_governor(governor)) {
            int resource_values[] = {INT_OP_CLUSTER0_TIMER_RATE, BIG_LITTLE_TR_MS_50,
                                     INT_OP_CLUSTER1_TIMER_RATE, BIG_LITTLE_TR_MS_50,
                                     INT_OP_NOTIFY_ON_MIGRATE,   0x00};
            perform_hint_action(DISPLAY_STATE_HINT_ID, resource_values,
                                ARRAY_SIZE(resource_values));
        }
    } else {
        /* Display on */
        if (is_interactive_governor(governor)) {
            undo_hint_action(DISPLAY_STATE_HINT_ID);
        }
    }

    if (init_interactive_hint == 0) {
        // First time the display is turned off
        display_fd = TEMP_FAILURE_RETRY(open(SYS_DISPLAY_PWR, O_RDWR));
        if (display_fd < 0) {
            strerror_r(errno, err_buf, sizeof(err_buf));
            ALOGE("Error opening %s: %s\n", SYS_DISPLAY_PWR, err_buf);
        } else
            init_interactive_hint = 1;
    } else if (!on) {
        /* Display off */
        rc = TEMP_FAILURE_RETRY(write(display_fd, display_off, strlen(display_off)));
        if (rc < 0) {
            strerror_r(errno, err_buf, sizeof(err_buf));
            ALOGE("Error writing %s to  %s: %s\n", display_off, SYS_DISPLAY_PWR, err_buf);
        }
    } else {
        /* Display on */
        rc = TEMP_FAILURE_RETRY(write(display_fd, display_on, strlen(display_on)));
        if (rc < 0) {
            strerror_r(errno, err_buf, sizeof(err_buf));
            ALOGE("Error writing %s to  %s: %s\n", display_on, SYS_DISPLAY_PWR, err_buf);
        }
    }
    return HINT_HANDLED;
}
