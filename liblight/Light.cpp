/*
 * Copyright (C) 2018 The LineageOS Project
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

#define LOG_TAG "LightService"

#include "Light.h"

#include <android-base/logging.h>

namespace {
using android::hardware::light::V2_0::LightState;

static constexpr int DEFAULT_MAX_BRIGHTNESS = 255;

static uint32_t rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

static bool isLit(const LightState& state) {
    return (state.color & 0x00ffffff);
}

struct color {
    unsigned int r, g, b;
    float _L, _a, _b;
};

// This hardware only allows primary colors
static struct color colors[] = {
    { 255,   0,   0, 0, 0, 0 }, // red
    { 255, 255,   0, 0, 0, 0 }, // yellow
    {   0, 255,   0, 0, 0, 0 }, // green
    {   0, 255, 255, 0, 0, 0 }, // cyan
    {   0,   0, 255, 0, 0, 0 }, // blue
    { 255,   0, 255, 0, 0, 0 }, // magenta
    { 255, 255, 255, 0, 0, 0 }, // white
    { 127, 127, 127, 0, 0, 0 }, // grey
    {   0,   0,   0, 0, 0, 0 }, // black
};

static constexpr int MAX_COLOR = 9;

// Convert RGB to L*a*b colorspace
// from http://www.brucelindbloom.com
static void rgb2lab(unsigned int R, unsigned int G, unsigned int B,
                    float *_L, float *_a, float *_b) {

    float r, g, b, X, Y, Z, fx, fy, fz, xr, yr, zr;
    float Ls, as, bs;
    float eps = 216.f / 24389.f;
    float k = 24389.f / 27.f;

    float Xr = 0.964221f;  // reference white D50
    float Yr = 1.0f;
    float Zr = 0.825211f;

    // RGB to XYZ
    r = R / 255.f; //R 0..1
    g = G / 255.f; //G 0..1
    b = B / 255.f; //B 0..1

    // assuming sRGB (D65)
    if (r <= 0.04045)
        r = r / 12;
    else
        r = (float) pow((r + 0.055) / 1.055, 2.4);

    if (g <= 0.04045)
        g = g / 12;
    else
        g = (float) pow((g + 0.055) / 1.055, 2.4);

    if (b <= 0.04045)
        b = b / 12;
    else
        b = (float) pow((b + 0.055) / 1.055, 2.4);


    X = 0.436052025f * r + 0.385081593f * g + 0.143087414f * b;
    Y = 0.222491598f * r + 0.71688606f * g + 0.060621486f * b;
    Z = 0.013929122f * r + 0.097097002f * g + 0.71418547f * b;

    // XYZ to Lab
    xr = X / Xr;
    yr = Y / Yr;
    zr = Z / Zr;

    if (xr > eps)
        fx = (float) pow(xr, 1 / 3.);
    else
        fx = (float) ((k * xr + 16.) / 116.);

    if (yr > eps)
        fy = (float) pow(yr, 1 / 3.);
    else
        fy = (float) ((k * yr + 16.) / 116.);

    if (zr > eps)
        fz = (float) pow(zr, 1 / 3.);
    else
        fz = (float) ((k * zr + 16.) / 116);

    Ls = (116 * fy) - 16;
    as = 500 * (fx - fy);
    bs = 200 * (fy - fz);

    *_L = (2.55 * Ls + .5);
    *_a = (as + .5);
    *_b = (bs + .5);
}

// find the color with the shortest distance
static struct color *
nearest_color(unsigned int r, unsigned int g, unsigned int b)
{
    int i = 0;
    float _L, _a, _b;
    double L_dist, a_dist, b_dist, total;
    double distance = 3 * 255;

    struct color *nearest = NULL;

    rgb2lab(r, g, b, &_L, &_a, &_b);

    for (i = 0; i < MAX_COLOR; i++) {
        L_dist = pow(_L - colors[i]._L, 2);
        a_dist = pow(_a - colors[i]._a, 2);
        b_dist = pow(_b - colors[i]._b, 2);
        total = sqrt(L_dist + a_dist + b_dist);
        if (total < distance) {
            nearest = &colors[i];
            distance = total;
        }
    }

    return nearest;
}

}  // anonymous namespace

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

Light::Light(std::pair<std::ofstream, uint32_t>&& lcd_backlight, std::ofstream&& button_backlight,
             std::ofstream&& red_led, std::ofstream&& green_led, std::ofstream&& blue_led,
             std::ofstream&& red_blink, std::ofstream&& green_blink, std::ofstream&& blue_blink,
             std::ofstream&& red_led_time, std::ofstream&& green_led_time, std::ofstream&& blue_led_time)
    : mLcdBacklight(std::move(lcd_backlight)),
      mButtonBacklight(std::move(button_backlight)),
      mRedLed(std::move(red_led)),
      mGreenLed(std::move(green_led)),
      mBlueLed(std::move(blue_led)),
      mRedBlink(std::move(red_blink)),
      mGreenBlink(std::move(green_blink)),
      mBlueBlink(std::move(blue_blink)),
      mRedLedTime(std::move(red_led_time)),
      mGreenLedTime(std::move(green_led_time)),
      mBlueLedTime(std::move(blue_led_time)) {
    auto attnFn(std::bind(&Light::setAttentionLight, this, std::placeholders::_1));
    auto backlightFn(std::bind(&Light::setLcdBacklight, this, std::placeholders::_1));
    auto batteryFn(std::bind(&Light::setBatteryLight, this, std::placeholders::_1));
    auto buttonsFn(std::bind(&Light::setButtonsBacklight, this, std::placeholders::_1));
    auto notifFn(std::bind(&Light::setNotificationLight, this, std::placeholders::_1));
    mLights.emplace(std::make_pair(Type::ATTENTION, attnFn));
    mLights.emplace(std::make_pair(Type::BACKLIGHT, backlightFn));
    mLights.emplace(std::make_pair(Type::BATTERY, batteryFn));
    mLights.emplace(std::make_pair(Type::BUTTONS, buttonsFn));
    mLights.emplace(std::make_pair(Type::NOTIFICATIONS, notifFn));

    for (int i = 0; i < MAX_COLOR; i++) {
        rgb2lab(colors[i].r, colors[i].g, colors[i].b,
                &colors[i]._L, &colors[i]._a, &colors[i]._b);
    }
}

// Methods from ::android::hardware::light::V2_0::ILight follow.
Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = mLights.find(type);

    if (it == mLights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    it->second(state);

    return Status::SUCCESS;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : mLights) {
        types.push_back(light.first);
    }

    _hidl_cb(types);

    return Void();
}

void Light::setAttentionLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mAttentionState = state;
    setSpeakerBatteryLightLocked();
}

void Light::setLcdBacklight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    uint32_t brightness = rgbToBrightness(state);

    // If max panel brightness is not the default (255),
    // apply linear scaling across the accepted range.
    if (mLcdBacklight.second != DEFAULT_MAX_BRIGHTNESS) {
        int old_brightness = brightness;
        brightness = brightness * mLcdBacklight.second / DEFAULT_MAX_BRIGHTNESS;
        LOG(VERBOSE) << "scaling brightness " << old_brightness << " => " << brightness;
    }

    mLcdBacklight.first << brightness << std::endl;
}

void Light::setButtonsBacklight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    uint32_t brightness = rgbToBrightness(state);

    mButtonBacklight << brightness << std::endl;
}

void Light::setBatteryLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mBatteryState = state;
    setSpeakerBatteryLightLocked();
}

void Light::setNotificationLight(const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mNotificationState = state;
    setSpeakerBatteryLightLocked();
}

void Light::setSpeakerBatteryLightLocked() {
    if (isLit(mNotificationState)) {
        setSpeakerLightLocked(mNotificationState);
    } else if (isLit(mAttentionState)) {
        setSpeakerLightLocked(mAttentionState);
    } else if (isLit(mBatteryState)) {
        setSpeakerLightLocked(mBatteryState);
    } else {
        // Lights off
        mRedLed << 0 << std::endl;
        mGreenLed << 0 << std::endl;
        mBlueLed << 0 << std::endl;
        mRedBlink << 0 << std::endl;
        mGreenBlink << 0 << std::endl;
        mBlueBlink << 0 << std::endl;
    }
}

void Light::setSpeakerLightLocked(const LightState& state) {
    int red, green, blue, blink;
    int onMs, offMs;
    uint32_t colorRGB = state.color;
    char breath_pattern[64] = { 0, };
    struct color *nearest = NULL;

    switch (state.flashMode) {
        case Flash::TIMED:
            onMs = state.flashOnMs;
            offMs = state.flashOffMs;
            break;
        case Flash::NONE:
        default:
            onMs = 0;
            offMs = 0;
            break;
    }

    red = (colorRGB >> 16) & 0xff;
    green = (colorRGB >> 8) & 0xff;
    blue = colorRGB & 0xff;
    blink = onMs > 0 && offMs > 0;

    // Disable all blinking to start
    mRedLed << 0 << std::endl;
    mGreenLed << 0 << std::endl;
    mBlueLed << 0 << std::endl;

    if (blink) {
        // Driver doesn't permit us to set individual duty cycles, so only
        // pick pure colors at max brightness when blinking.
        nearest = nearest_color(red, green, blue);

        red = nearest->r;
        green = nearest->g;
        blue = nearest->b;

        // Make sure the values are between 1 and 7 seconds
        if (onMs < 1000)
            onMs = 1000;
        else if (onMs > 7000)
            onMs = 7000;

        if (offMs < 1000)
            offMs = 1000;
        else if (offMs > 7000)
            offMs = 7000;

        // Ramp up, lit, ramp down, unlit. in seconds.
        sprintf(breath_pattern, "1 %d 1 %d", (int)(onMs / 1000), (int)(offMs / 1000));
    } else {
        blink = 0;
        sprintf(breath_pattern, "1 2 1 2");
    }

    // Do everything with the lights out, then turn up the brightness
    mRedLedTime << breath_pattern << std::endl;
    mRedBlink << (blink && red ? 1 : 0) << std::endl;
    mGreenLedTime << breath_pattern << std::endl;
    mGreenBlink << (blink && green ? 1 : 0) << std::endl;
    mBlueLedTime << breath_pattern << std::endl;
    mBlueBlink << (blink && blue ? 1 : 0) << std::endl;

    mRedLed << red << std::endl;
    mGreenLed << green << std::endl;
    mBlueLed << blue << std::endl;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
