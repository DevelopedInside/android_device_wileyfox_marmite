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

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit some common LineageOS stuff
$(call inherit-product, vendor/cm/config/common_full_phone.mk)

# Inherit from marmite device
$(call inherit-product, $(LOCAL_PATH)/device.mk)

# Set those variables here to overwrite the inherited values.
BOARD_VENDOR := wileyfox
PRODUCT_DEVICE := marmite
PRODUCT_NAME := lineage_marmite
PRODUCT_BRAND := Wileyfox
PRODUCT_MODEL := Wileyfox Swift 2
PRODUCT_MANUFACTURER := Wileyfox

PRODUCT_GMS_CLIENTID_BASE := android-wileyfox

TARGET_VENDOR_PRODUCT_NAME := marmite

PRODUCT_SYSTEM_PROPERTY_BLACKLIST += \
    ro.product.model
