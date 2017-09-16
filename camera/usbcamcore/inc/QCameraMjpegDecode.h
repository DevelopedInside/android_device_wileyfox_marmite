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

/**
 *  MACROS and CONSTANTS
 **/
#define CEILING16(X) (((X) + 0x000F) & 0xFFF0)
#define PAD_TO_WORD(a)(((a) + 3) & (~3U))
#define PAD_TO_4K(a)(((a) + 0xFFF) & (~0xFFFU))

#define MAX_EXIF_ENTRIES 10
#define MAX_INSTANCES 3
#define MAX_BUFFERS 5
#define MAX_FN_LEN 256
#define MAX_COLOR_FMTS 17


#define DUMP_TO_FILE(filename, p_addr, len) ({ \
  size_t rc = 0; \
  FILE *fp = fopen(filename, "w+"); \
  if (fp) { \
    rc = fwrite(p_addr, 1, len, fp); \
    fclose(fp); \
  } \
})

/** omx_color_format_t:
 *
 * test color format mapping
 **/
typedef struct {
  char format_str[15];
  int eColorFormat;
  float chroma_wt;
} omx_color_format_t;

/** col_formats:
 *
 * Color format mapping from testapp to OMX
 **/
static const omx_color_format_t col_formats[MAX_COLOR_FMTS] =
{
  { "YCRCBLP_H2V2",      (int)OMX_QCOM_IMG_COLOR_FormatYVU420SemiPlanar, 1.5 },
  { "YCBCRLP_H2V2",               (int)OMX_COLOR_FormatYUV420SemiPlanar, 1.5 },
  { "YCRCBLP_H2V1",      (int)OMX_QCOM_IMG_COLOR_FormatYVU422SemiPlanar, 2.0 },
  { "YCBCRLP_H2V1",               (int)OMX_COLOR_FormatYUV422SemiPlanar, 2.0 },
  { "YCRCBLP_H1V2", (int)OMX_QCOM_IMG_COLOR_FormatYVU422SemiPlanar_h1v2, 2.0 },
  { "YCBCRLP_H1V2", (int)OMX_QCOM_IMG_COLOR_FormatYUV422SemiPlanar_h1v2, 2.0 },
  { "YCRCBLP_H1V1",      (int)OMX_QCOM_IMG_COLOR_FormatYVU444SemiPlanar, 3.0 },
  { "YCBCRLP_H1V1",      (int)OMX_QCOM_IMG_COLOR_FormatYUV444SemiPlanar, 3.0 },
  {    "IYUV_H2V2",          (int)OMX_QCOM_IMG_COLOR_FormatYVU420Planar, 1.5 },
  {    "YUV2_H2V2",                   (int)OMX_COLOR_FormatYUV420Planar, 1.5 },
  {    "IYUV_H2V1",          (int)OMX_QCOM_IMG_COLOR_FormatYVU422Planar, 2.0 },
  {    "YUV2_H2V1",                   (int)OMX_COLOR_FormatYUV422Planar, 2.0 },
  {    "IYUV_H1V2",     (int)OMX_QCOM_IMG_COLOR_FormatYVU422Planar_h1v2, 2.0 },
  {    "YUV2_H1V2",     (int)OMX_QCOM_IMG_COLOR_FormatYUV422Planar_h1v2, 2.0 },
  {    "IYUV_H1V1",          (int)OMX_QCOM_IMG_COLOR_FormatYVU444Planar, 3.0 },
  {    "YUV2_H1V1",          (int)OMX_QCOM_IMG_COLOR_FormatYUV444Planar, 3.0 },
  {   "MONOCHROME",                     (int)OMX_COLOR_FormatMonochrome, 1.0 }
};

/** omx_pending_func_t:
 *
 * Intermediate function for state change
 **/
typedef OMX_ERRORTYPE (*omx_pending_func_t)(void *);


typedef struct  {
  struct ion_fd_data ion_info_fd;
  struct ion_allocation_data alloc;
  int p_pmem_fd;
  size_t size;
  int ion_fd;
  uint8_t *addr;
} buffer_t;

typedef struct {
  char file_name[80];
  uint32_t width;
  uint32_t height;
  uint32_t quality;
  int eColorFormat;
  float chroma_wt;
  uint32_t stride;
} omx_image_t;

typedef struct {
  uint32_t input_width;
  uint32_t input_height;
  uint32_t h_offset;
  uint32_t v_offset;
  uint32_t output_width;
  uint32_t output_height;
  uint8_t enable;
} omx_scale_cfg_t;

/** omx_enc_args_t:
 *  @main: main image info
 *  @thumbnail: thumbnail info
 *  @output_file: output filenmae
 *  @rotation: image rotation
 *  @encode_thumbnail: flag to indicate if thumbnail needs to be
 *                   encoded
 *  @abort_time: abort time
 *  @use_pmem: flag to indicate if ion needs to be used
 *  @main_scale_cfg: main image scale config
 *  @tn_scale_cfg: thumbnail scale config
 *  @instance_cnt: encoder instance count
 *  @burst_count: burst capture count
 *  @auto_pad: add padding to the input image
 *  @use_workbuf: Flag to indicate whether to use work buf
 *
 *  Represents the test arguments passed from the user
 **/
typedef struct {
  omx_image_t main;
  omx_image_t thumbnail;
  char output_file[80];
  int16_t rotation;
  uint8_t  encode_thumbnail;
  uint32_t abort_time;
  uint8_t use_pmem;
  omx_scale_cfg_t main_scale_cfg;
  omx_scale_cfg_t tn_scale_cfg;
  int instance_cnt;
  uint32_t burst_count;
  uint8_t auto_pad;
  bool use_workbuf;
} omx_enc_args_t;

/** omx_enc_args_t:
 *  @main: main image info
 *  @thumbnail: thumbnail info
 *  @output_file: output filename
 *  @rotation: image rotation
 *  @encode_thumbnail: flag to indicate if thumbnail needs to be
 *                   encoded
 *  @abort_time: abort time
 *  @use_pmem: flag to indicate if ion needs to be used
 *  @main_scale_cfg: main image scale config
 *  @tn_scale_cfg: thumbnail scale config
 *  @instance_cnt: encoder instance count
 *  @frame_info: YUV input frame extension
 *  @callbacks: OMX callbacks
 *  @encoding: flag to indicate if encoding is in progress
 *  @usePadding: check if image is padded
 *  @lock: mutex variable
 *  @cond: condition variable
 *  @thread_id: thread id
 *  @p_in_buffers: input buffer header
 *  @p_out_buffers: output buffer header
 *  @in_buffer: input buffer
 *  @out_buffer: output buffer
 *  @buf_count: number of captures. (for burst mode)
 *  @ebd_count: EBD count
 *  @fbd_count: FBD count
 *  @p_handle: OMX handle
 *  @inputPort: input port
 *  @outputPort: output port
 *  @total_size: total size of the buffer
 *  @error_flag: check if error is occured
 *  @state_change_pending: flag to indicate of state change is
 *                       pending
 *  @last_error: last error
 *  @work_buf: Work buffer
 *  @use_workbuf: Flag to indicate if the work buffer can be
 *    used
 *
 *  Represents the encoder test object
 **/
typedef struct {
  omx_image_t main;
  omx_image_t thumbnail;
  char output_file[MAX_BUFFERS][MAX_FN_LEN];
  int16_t rotation;
  uint8_t  encode_thumbnail;
  uint32_t abort_time;
  uint8_t use_pmem;
  omx_scale_cfg_t main_scale_cfg;
  omx_scale_cfg_t tn_scale_cfg;

  QOMX_YUV_FRAME_INFO frame_info;
  OMX_CALLBACKTYPE callbacks;

  int encoding;
  int usePadding;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  pthread_t thread_id;

  OMX_BUFFERHEADERTYPE *p_in_buffers[MAX_BUFFERS];
  OMX_BUFFERHEADERTYPE *p_thumb_buf[MAX_BUFFERS];
  OMX_BUFFERHEADERTYPE *p_out_buffers[MAX_BUFFERS];
  buffer_t in_buffer[MAX_BUFFERS];
  buffer_t out_buffer[MAX_BUFFERS];
  uint32_t buf_count;
  uint32_t ebd_count;
  uint32_t total_ebd_count;
  uint32_t fbd_count;

  OMX_HANDLETYPE p_handle;
  OMX_PARAM_PORTDEFINITIONTYPE *inputPort;
  OMX_PARAM_PORTDEFINITIONTYPE *outputPort;
  OMX_PARAM_PORTDEFINITIONTYPE *thumbPort;
  OMX_U32 total_size;
  OMX_BOOL error_flag;
  OMX_BOOL state_change_pending;
  OMX_ERRORTYPE last_error;
  OMX_U64 encode_time;
  OMX_BOOL aborted;
  pthread_t abort_thread_id;
  pthread_mutex_t abort_mutx;
  pthread_cond_t  abort_cond;
  OMX_STATETYPE  omx_state;
  buffer_t work_buf;
  bool use_workbuf;
  unsigned char * inputaddr;
  unsigned char * outputaddr;
  int* jpegLength;
} omx_enc_t;

int encodeJpeg(unsigned char* inbuffer,unsigned char* outbuffer,int* length);
int JpegtoYUV(unsigned char*JpegBuf,int jpegsize,unsigned char* yuvBuf);
bool RGB2YUV(unsigned char* RgbBuf,int nWidth,int nHeight,unsigned char* yuvBuf,unsigned int len);
#endif /* __QCAMERA_MJPEG_DECODE_H */
