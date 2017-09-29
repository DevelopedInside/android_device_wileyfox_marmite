# CAF Audio
PRODUCT_PACKAGES += \
    audiod \
    audio.a2dp.default \
    audio.r_submix.default \
    audio.usb.default \
    audio.primary.msm8937 \
    libaudio-resampler \
    libqcompostprocbundle \
    libqcomvisualizer \
    libqcomvoiceprocessing \
    tinymix

# Audio calibration database (ether)
ACDB_TARGET ?= AW87319

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Bluetooth_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Bluetooth_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_General_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_General_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Global_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Global_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Handset_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Handset_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Hdmi_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Hdmi_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Headset_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Headset_cal.acdb \
    $(LOCAL_PATH)/audio/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Speaker_cal.acdb:system/etc/acdbdata/$(ACDB_TARGET)/$(ACDB_TARGET)_Speaker_cal.acdb

# Individual audio configs
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio/audio_policy.conf:system/etc/audio_policy.conf \
    $(LOCAL_PATH)/audio/audio_policy_configuration.xml:system/etc/audio_policy_configuration.xml \
    $(LOCAL_PATH)/audio/mixer_paths_AW87319.xml:system/etc/mixer_paths_AW87319.xml

# Based on (https://github.com/LineageOS/android/commit/64e1481a189d34c2e9fa603571f1158164e3654d)
PRODUCT_COPY_FILES += \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/audio_effects.conf:system/vendor/etc/audio_effects.conf \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/aanc_tuning_mixer.txt:system/etc/aanc_tuning_mixer.txt \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/audio_platform_info_extcodec.xml:system/etc/audio_platform_info_extcodec.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/audio_platform_info.xml:system/etc/audio_platform_info.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_mtp.xml:system/etc/mixer_paths_mtp.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_skuk.xml:system/etc/mixer_paths_skuk.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_skuhf.xml:system/etc/mixer_paths_qrd_skuhf.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_skuh.xml:system/etc/mixer_paths_qrd_skuh.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_skui.xml:system/etc/mixer_paths_qrd_skui.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_skum.xml:system/etc/mixer_paths_qrd_skum.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_skun.xml:system/etc/mixer_paths_qrd_skun.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_wcd9306.xml:system/etc/mixer_paths_wcd9306.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_wcd9326.xml:system/etc/mixer_paths_wcd9326.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_wcd9330.xml:system/etc/mixer_paths_wcd9330.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_wcd9335.xml:system/etc/mixer_paths_wcd9335.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths.xml:system/etc/mixer_paths.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_sku1.xml:system/etc/mixer_paths_qrd_sku1.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/mixer_paths_qrd_sku2.xml:system/etc/mixer_paths_qrd_sku2.xml

# Files 2 Vendor
PRODUCT_COPY_FILES += \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/audio_output_policy.conf:system/vendor/etc/audio_output_policy.conf

# SoundTriggers
PRODUCT_COPY_FILES += \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/sound_trigger_mixer_paths.xml:system/etc/sound_trigger_mixer_paths.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/sound_trigger_mixer_paths_wcd9306.xml:system/etc/sound_trigger_mixer_paths_wcd9306.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/sound_trigger_mixer_paths_wcd9330.xml:system/etc/sound_trigger_mixer_paths_wcd9330.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/sound_trigger_mixer_paths_wcd9335.xml:system/etc/sound_trigger_mixer_paths_wcd9335.xml \
    hardware/qcom/audio-caf/msm8996/configs/msm8937/sound_trigger_platform_info.xml:system/etc/sound_trigger_platform_info.xml

#XML Audio configuration files
PRODUCT_COPY_FILES += \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:/system/etc/a2dp_audio_policy_configuration.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:/system/etc/audio_policy_volumes.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/default_volume_tables.xml:/system/etc/default_volume_tables.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:/system/etc/r_submix_audio_policy_configuration.xml \
    $(TOPDIR)frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:/system/etc/usb_audio_policy_configuration.xml

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:system/etc/permissions/android.hardware.audio.low_latency.xml

# Properties
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1 \
    audio_hal.period_size=192
