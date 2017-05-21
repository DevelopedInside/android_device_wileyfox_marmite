# ???
ADDITIONAL_DEFAULT_PROPERTIES += \
    ro.usb.id.midi=90BA \
    ro.usb.id.midi_adb=90BB \
    ro.usb.id.mtp=2281 \
    ro.usb.id.mtp_adb=2282 \
    ro.usb.id.ptp=2284 \
    ro.usb.id.ptp_adb=2283 \
    ro.usb.id.ums=2286 \
    ro.usb.id.ums_adb=2285 \
    ro.usb.vid=2970 

# USB Secure
ADDITIONAL_DEFAULT_PROPERTIES += \
    ro.secure=0 \
    ro.adb.secure=0 \
    ro.allow.mock.location=0 \
    ro.config.always_show_roaming=true

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

PRODUCT_BUILD_PROP_OVERRIDES += BUILD_UTC_DATE=0
