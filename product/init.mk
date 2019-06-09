# Root init scripts
PRODUCT_PACKAGES += \
    init.marmite.usb.sh \
    init.recovery.qcom.rc

# /vendor/
PRODUCT_PACKAGES += \
    ueventd.rc

# /vendor/bin
PRODUCT_PACKAGES += \
    init.class_late.sh \
    init.class_main.sh \
    init.qcom.early_boot.sh \
    init.qcom.sensors.sh \
    init.qcom.sh \
    init.qcom.usb.sh

# /vendor/etc/
PRODUCT_PACKAGES += \
    fstab.qcom

# /vendor/etc/init/hw
PRODUCT_PACKAGES += \
    init.msm.usb.configfs.rc \
    init.qcom.rc \
    init.qcom.power.rc \
    init.qcom.usb.rc \
    init.target.rc

# Device varinats
PRODUCT_PACKAGES += \
    init.variant.mv1.rc \
    init.variant.mv2.rc \
    init.variant.mv3.rc

# OTA
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/releasetools/fixup.sh:install/bin/fixup.sh
