# Keystore (LineageOS's keystore doesn't support msm8937, we use msm8952)
ifneq ($(TARGET_PROVIDES_KEYMASTER),true)
PRODUCT_PACKAGES += \
    keystore.msm8952
endif
