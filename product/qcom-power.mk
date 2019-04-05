# IRQ Balancer
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/msm_irqbalance.conf:$(TARGET_COPY_OUT_VENDOR)/etc/msm_irqbalance.conf

# QCOM
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/qti_whitelist.xml:$(TARGET_COPY_OUT_VENDOR)/etc/sysconfig/qti_whitelist.xml

# Power HAL
PRODUCT_PACKAGES += \
    android.hardware.power@1.1-service-qti

# Powerhint
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/powerhint.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/powerhint.xml \
    $(LOCAL_PATH)/configs/powerhint_mpctl.xml:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.xml

# Public Libraries
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/public.libraries.txt:$(TARGET_COPY_OUT_VENDOR)/etc/public.libraries.txt
