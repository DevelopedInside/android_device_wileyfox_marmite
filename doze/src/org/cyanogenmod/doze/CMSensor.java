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
import android.content.Intent;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.UserHandle;
import android.util.Log;

public abstract class CMSensor {

    private static final boolean DEBUG = false;
    private static final String TAG = CMSensor.class.getSimpleName();

    private static final String DOZE_INTENT = "com.android.systemui.doze.pulse";
    private static final int BATCH_LATENCY_IN_MS = 100;

    private Context mContext;
    private SensorManager mSensorManager;
    private Sensor mSensor;

    private SensorEventListener mSensorEventListener = new SensorEventListener() {
        @Override
        public void onSensorChanged(SensorEvent event) {
            onSensorEvent(event);
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
            // Do nothing
        }
    };

    CMSensor(Context context, int type) {
        mContext = context;
        mSensorManager = (SensorManager) mContext.getSystemService(Context.SENSOR_SERVICE);
        mSensor = mSensorManager.getDefaultSensor(type);
    }

    void launchDozePulse() {
        Log.d(TAG, "Launch doze pulse");
        mContext.sendBroadcastAsUser(new Intent(DOZE_INTENT),
		new UserHandle(UserHandle.USER_CURRENT));
    }

    void enable() {
        if (DEBUG) Log.d(TAG, "Enabling");
        mSensorManager.registerListener(mSensorEventListener, mSensor,
                SensorManager.SENSOR_DELAY_NORMAL, BATCH_LATENCY_IN_MS * 1000);
    }

    void disable() {
        if (DEBUG) Log.d(TAG, "Disabling");
        mSensorManager.unregisterListener(mSensorEventListener);
    }

    void onSensorEvent(SensorEvent event) {
        if (DEBUG) Log.d(TAG, "Got sensor event: " + event.values[0]);
    }
}
