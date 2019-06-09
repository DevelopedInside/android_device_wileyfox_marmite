# ANT
PRODUCT_PACKAGES += \
    AntHalService \
    com.dsi.ant.antradio_library

# Permissions
PRODUCT_COPY_FILES += \
    external/ant-wireless/antradio-library/com.dsi.ant.antradio_library.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/com.dsi.ant.antradio_library.xml
