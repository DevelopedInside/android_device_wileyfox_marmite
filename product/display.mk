# Bootanimation
TARGET_SCREEN_HEIGHT := 1280
TARGET_SCREEN_WIDTH := 720

PRODUCT_AAPT_CONFIG := normal xhdpi xxhdpi
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

$(call inherit-product-if-exists, frameworks/native/build/phone-xxhdpi-2048-dalvik-heap.mk)
$(call inherit-product-if-exists, frameworks/native/build/phone-xxhdpi-2048-hwui-memory.mk)

# Ambient display
PRODUCT_PACKAGES += \
    CMDoze
