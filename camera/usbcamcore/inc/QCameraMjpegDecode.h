/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __QCAMERA_MJPEG_DECODE_H
#define __QCAMERA_MJPEG_DECODE_H
#include <sys/types.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*KHRONOS header files*/
#include "OMX_Types.h"
#include "OMX_Index.h"
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "QOMX_JpegExtensions.h"
#include "qomx_core.h"
#include "qexif.h"



#include <sys/time.h>
#include "errno.h"
#include <utils/Log.h>
#ifdef _ANDROID_
#include <cutils/properties.h>
#endif
#include <utils/threads.h>
#include <unistd.h>
#include <dirent.h>

#ifdef LOAD_ADSP_RPC_LIB
#include <dlfcn.h>
#include <stdlib.h>
#endif

#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#define PRCTL_H <SYSTEM_HEADER_PREFIX/prctl.h>
#include PRCTL_H

#ifdef LIB2D_ROTATION_ENABLE
#include "mm_lib2d.h"
#endif

#include <linux/msm_ion.h>
#include <sys/mman.h>

#define PAD_TO_4K(a)(((a) + 0xFFF) & (~0xFFFU))
#define PAD_TO_WORD(a)(((a) + 3) & (~3U))

typedef OMX_ERRORTYPE (*omx_pending_func_t)(void *);

typedef struct  {
  struct ion_fd_data ion_info_fd;
  struct ion_allocation_data alloc;
  int p_pmem_fd;
  size_t size;
  int ion_fd;
  uint8_t *addr;
} buffer_t;

typedef struct{
  int img_width;
  int img_height;
  float img_chroma_wt;
  int jpg_quality;
  int img_eColorFormat;
  OMX_U32 total_size;
  int output_size;
  OMX_HANDLETYPE omx_handle;
  OMX_CALLBACKTYPE callbacks;
  QOMX_YUV_FRAME_INFO frame_info;
  OMX_PARAM_PORTDEFINITIONTYPE *inputPort;
  OMX_PARAM_PORTDEFINITIONTYPE *outputPort;
  OMX_BUFFERHEADERTYPE *p_in_buffers;
  OMX_BUFFERHEADERTYPE *p_out_buffers;
  buffer_t work_buf;
  buffer_t in_buffer;
  buffer_t out_buffer;
  OMX_STATETYPE  omx_state;
  OMX_BOOL fill_buffer_done;
  OMX_BOOL error_flag;
  OMX_BOOL state_change_pending;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  pthread_t thread_id;
} jpeg_encoder_task;

int encodeJpeg(jpeg_encoder_task* task);
jpeg_encoder_task* createEncodeJpegTask(int width, int height, int quality);
void releaseEncodeJpegTask(jpeg_encoder_task* task);
#endif /* __QCAMERA_MJPEG_DECODE_H */
