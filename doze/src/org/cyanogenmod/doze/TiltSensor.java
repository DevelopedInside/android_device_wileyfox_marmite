/**
 * Copyright (c) 2015-2016 The CyanogenMod Project
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
package org.cyanogenmod.doze;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.os.SystemClock;
import android.util.Log;

public class TiltSensor extends CMSensor {

    private static final int MIN_PULSE_INTERVAL_MS = 2500;

    private long mEntryTimestamp;

    TiltSensor(Context context) {
        super(context, Sensor.TYPE_TILT_DETECTOR);
    }

    @Override
    void onSensorEvent(SensorEvent event) {
        super.onSensorEvent(event);

        if (SystemClock.elapsedRealtime() - mEntryTimestamp < MIN_PULSE_INTERVAL_MS) {
            return;
        }

        mEntryTimestamp = SystemClock.elapsedRealtime();

        if (event.values[0] == 1) {
            launchDozePulse();
        }
    }
}
