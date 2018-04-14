# Camera
PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4-impl \
    camera.device@3.2-impl \
    camera.msm8937 \
    libmm-qcamera \
    Snap

# Camera configs
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/camera/camera_config.xml:system/etc/camera/camera_config.xml \
    $(LOCAL_PATH)/configs/camera/csidtg_camera.xml:system/etc/camera/csidtg_camera.xml \
    $(LOCAL_PATH)/configs/camera/csidtg_chromatix.xml:system/etc/camera/csidtg_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/ov8856_chromatix.xml:system/etc/camera/ov8856_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/ov8865_chromatix.xml:system/etc/camera/ov8865_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/s5k3l8_chromatix.xml:system/etc/camera/s5k3l8_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/s5k3l8_f3l8yam_chromatix.xml:system/etc/camera/s5k3l8_f3l8yam_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/s5k3p3_chromatix.xml:system/etc/camera/s5k3p3_chromatix.xml \
    $(LOCAL_PATH)/configs/camera/s5k3p3_x50l_chromatix.xml:system/etc/camera/s5k3p3_x50l_chromatix.xml

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml
