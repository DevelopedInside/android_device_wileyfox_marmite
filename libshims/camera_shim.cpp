
/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#include <cutils/native_handle.h>
#include <ui/PixelFormat.h>

// GraphicBuffer(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int, native_handle*, bool);
extern "C" void _ZN7android13GraphicBufferC1EjjijjjP13native_handleb(unsigned int, unsigned int, android::PixelFormat, unsigned int, unsigned int, unsigned int, native_handle*, bool);
 
extern "C" void _ZN7android13GraphicBufferC1EjjijjP13native_handleb(unsigned int inWidth, unsigned int inHeight, android::PixelFormat inFormat, unsigned int inUsage, unsigned int inStride, native_handle_t* inHandle, bool keepOwnership) {
  _ZN7android13GraphicBufferC1EjjijjjP13native_handleb(
      inWidth, inHeight, inFormat, 0, inUsage, inStride, inHandle, keepOwnership);
}
