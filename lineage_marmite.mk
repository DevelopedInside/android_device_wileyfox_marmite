#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2018 The LineageOS Project
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

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_m.mk)

# Inherit some common Pixel Experience stuff
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit from marmite device
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# Vendor security patch level
PRODUCT_PROPERTY_OVERRIDES += \
    ro.lineage.build.vendor_security_patch=2018-06-01

# Set those variables here to overwrite the inherited values.
PRODUCT_NAME := lineage_marmite
BOARD_VENDOR := wileyfox
PRODUCT_DEVICE := marmite

PRODUCT_GMS_CLIENTID_BASE := android-wileyfox

PRODUCT_MANUFACTURER := Wileyfox

PRODUCT_BRAND := Wileyfox
TARGET_VENDOR := wileyfox
TARGET_VENDOR_PRODUCT_NAME := Swift2
TARGET_VENDOR_DEVICE_NAME := marmite

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="marmite-user 8.1.0 OPM5.171019.017 38c792b31d release-keys"

BUILD_FINGERPRINT := Wileyfox/Swift2/marmite:8.1.0/OPM5.171019.017/38c792b31d:user/release-keys

# Due to multi-density builds, these are set by init
PRODUCT_SYSTEM_PROPERTY_BLACKLIST += \
    ro.product.model \
    ro.sf.lcd_density
