# GPS
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/flp.conf:system/etc/flp.conf \
    $(LOCAL_PATH)/configs/gps.conf:system/etc/gps.conf \
    $(LOCAL_PATH)/configs/izat.conf:system/etc/izat.conf \
    $(LOCAL_PATH)/configs/lowi.conf:system/etc/lowi.conf \
    $(LOCAL_PATH)/configs/sap.conf:system/etc/sap.conf \
    $(LOCAL_PATH)/configs/xtwifi.conf:system/etc/xtwifi.conf

PRODUCT_BOOT_JARS += \
    com.qti.location.sdk

PRODUCT_PACKAGES += \
    libcurl \
    libgnsspps

# GPS
PRODUCT_PACKAGES += \
    gps.msm8937

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml

# Properties
PRODUCT_PROPERTY_OVERRIDES += \
    persist.gps.qc_nlp_in_use=1 \
    persist.loc.nlp_name=com.qualcomm.location \
    ro.gps.agps_provider=1	