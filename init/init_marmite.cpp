/*
   Copyright (c) 2016, The CyanogenMod Project
   Copyright (c) 2017, The LineageOS Project

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdlib>
#include <fstream>
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <errno.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include "vendor_init.h"
#include "property_service.h"
#include "log.h"


using android::base::GetProperty;
using android::init::property_set;

/*
 * In some device revisions, there is a sound amplifier that is not activated at
 * startup.
 * In stock init binary file, there is mention of the folder, which in theory is
 * created when you connect the device in the sysfs section.
 * If this method does not work, replace this piece of code with the working
 * method or revert it.
 * If you find a more stable method, then replace it.
 *
 * @Author BeYkeRYkt (21-09-2017)
 */
void check_aw87319()
{
    DIR* dir = opendir("/sys/bus/i2c/drivers/AW87319_PA/2-0058");
    if (dir)
    {
        property_set("persist.audio.calfile0", "/vendor/etc/acdbdata/AW87319/AW87319_Bluetooth_cal.acdb");
        property_set("persist.audio.calfile1", "/vendor/etc/acdbdata/AW87319/AW87319_General_cal.acdb");
        property_set("persist.audio.calfile2", "/vendor/etc/acdbdata/AW87319/AW87319_Global_cal.acdb");
        property_set("persist.audio.calfile3", "/vendor/etc/acdbdata/AW87319/AW87319_Handset_cal.acdb");
        property_set("persist.audio.calfile4", "/vendor/etc/acdbdata/AW87319/AW87319_Hdmi_cal.acdb");
        property_set("persist.audio.calfile5", "/vendor/etc/acdbdata/AW87319/AW87319_Headset_cal.acdb");
        property_set("persist.audio.calfile6", "/vendor/etc/acdbdata/AW87319/AW87319_Speaker_cal.acdb");
        closedir(dir);
    }
}

void vendor_load_properties()
{
    check_aw87319();

    std::string cmv = GetProperty("ro.boot.cmv","");

    if (cmv == "mv1")
    {
        /* Swift 2 */
        property_set("ro.sf.lcd_density", "320");
        property_set("ro.media.maxmem", "10590068224");
    }
    else if (cmv == "mv2")
    {
        /* Swift 2 Plus*/
        property_set("ro.sf.lcd_density", "320");
    }
    else if (cmv == "mv3")
    {
        /* Swift 2 X */
        property_set("ro.sf.lcd_density", "440");
//        property_set("ro.power_profile.override", "power_profile_2x");
    }
}

