#
# Copyright (C) 2016 The CyanogenMod Project
#               2017 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

$(call inherit-product, device/wileyfox/marmite/full_marmite.mk)

#Bootanimation res
TARGET_BOOT_ANIMATION_RES := 720

# Inherit some common  stuffy stuff
$(call inherit-product, vendor/aosp/config/common.mk)

# Set those variables here to overwrite the inherited values.
PRODUCT_NAME := aosp_marmite
BOARD_VENDOR := wileyfox
PRODUCT_DEVICE := marmite

PRODUCT_GMS_CLIENTID_BASE := android-wileyfox

PRODUCT_MANUFACTURER := Wileyfox

PRODUCT_BRAND := Wileyfox
TARGET_VENDOR := wileyfox
TARGET_VENDOR_PRODUCT_NAME := Swift2
TARGET_VENDOR_DEVICE_NAME := marmite

PRODUCT_SYSTEM_PROPERTY_BLACKLIST += \
    ro.product.model
