#!/vendor/bin/sh

# Copyright (c) 2012-2013, 2016-2017, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of The Linux Foundation nor
#       the names of its contributors may be used to endorse or promote
#       products derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Set Memory parameters.
#
# Set per_process_reclaim tuning parameters
# 2GB 64-bit will have aggressive settings when compared to 1GB 32-bit
# 1GB and less will use vmpressure range 50-70, 2GB will use 10-70
# 1GB and less will use 512 pages swap size, 2GB will use 1024
#
# Set Low memory killer minfree parameters
# 32 bit all memory configurations will use 15K series
# 64 bit up to 2GB with use 14K, and above 2GB will use 18K
#
# Set ALMK parameters (usually above the highest minfree values)
# 32 bit will have 53K & 64 bit will have 81K
#

arch_type=`uname -m`
MemTotalStr=`cat /proc/meminfo | grep MemTotal`
MemTotal=${MemTotalStr:16:8}

# Read adj series and set adj threshold for PPR and ALMK.
# This is required since adj values change from framework to framework.
adj_series=`cat /sys/module/lowmemorykiller/parameters/adj`
adj_1="${adj_series#*,}"
set_almk_ppr_adj="${adj_1%%,*}"

# PPR and ALMK should not act on HOME adj and below.
# Normalized ADJ for HOME is 6. Hence multiply by 6
# ADJ score represented as INT in LMK params, actual score can be in decimal
# Hence add 6 considering a worst case of 0.9 conversion to INT (0.9*6).
# For uLMK + Memcg, this will be set as 6 since adj is zero.
set_almk_ppr_adj=$(((set_almk_ppr_adj * 6) + 6))
echo $set_almk_ppr_adj > /sys/module/lowmemorykiller/parameters/adj_max_shift
echo $set_almk_ppr_adj > /sys/module/process_reclaim/parameters/min_score_adj

#Set other memory parameters
echo 1 > /sys/module/process_reclaim/parameters/enable_process_reclaim
echo 70 > /sys/module/process_reclaim/parameters/pressure_max
echo 30 > /sys/module/process_reclaim/parameters/swap_opt_eff
echo 0 > /sys/module/lowmemorykiller/parameters/enable_adaptive_lmk
echo 10 > /sys/module/process_reclaim/parameters/pressure_min
echo 1024 > /sys/module/process_reclaim/parameters/per_swap_size
echo "18432,23040,27648,32256,55296,80640" > /sys/module/lowmemorykiller/parameters/minfree
# For 64-bit arch, vmpressure_file_min = LMK minfree's last bin value
echo 80640 > /sys/module/lowmemorykiller/parameters/vmpressure_file_min

# Let kernel know our image version/variant/crm_version
if [ -f /sys/devices/soc0/select_image ]; then
    image_version="10:"
    image_version+=`getprop ro.build.id`
    image_version+=":"
    image_version+=`getprop ro.build.version.incremental`
    image_variant=`getprop ro.product.name`
    image_variant+="-"
    image_variant+=`getprop ro.build.type`
    oem_version=`getprop ro.build.version.codename`
    echo 10 > /sys/devices/soc0/select_image
    echo $image_version > /sys/devices/soc0/image_version
    echo $image_variant > /sys/devices/soc0/image_variant
    echo $oem_version > /sys/devices/soc0/image_crm_version
fi
