# Root init scripts
PRODUCT_PACKAGES += \
    fstab.qcom \
    init.class_main.sh \
    init.marmite.usb.sh \
    init.qcom.early_boot.sh \
    init.qcom.rc \
    init.qcom.sensors.sh \
    init.qcom.sh \
    init.qcom.power.rc \
    init.qcom.usb.rc \
    init.qcom.usb.sh \
    init.target.rc \
    init.recovery.qcom.rc \
    ueventd.qcom.rc

# Wileyfox varinats
PRODUCT_PACKAGES += \
    init.variant.mv1.rc \
    init.variant.mv2.rc \
    init.variant.mv3.rc

# OTA
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/releasetools/fixup.sh:install/bin/fixup.sh
