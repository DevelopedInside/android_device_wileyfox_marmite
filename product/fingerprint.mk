# Fingerprint
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.0-service

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:system/etc/permissions/android.hardware.fingerprint.xml
