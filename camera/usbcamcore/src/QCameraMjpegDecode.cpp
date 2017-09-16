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

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraMjpegDecode"

#include "QCameraMjpegDecode.h"

extern "C" {
// #include "jpeg_buffer.h"
// #include "jpeg_common.h"
// #include "jpegd.h"
#include "jpeglib.h"
#include "setjmp.h"
}

/* TBDJ: Can be removed */
#define MIN(a,b)  (((a) < (b)) ? (a) : (b))
#define MAX(a,b)  (((a) > (b)) ? (a) : (b))

#define CLAMP(x,min,max) MIN(MAX((x),(min)),(max))

#define TIME_IN_US(r) ((int64_t)r.tv_sec * 1000000LL + r.tv_usec)

#define EXT_LEN 4
#define FILE_EXT "_%d_%d.jpg"

/** STR_ADD_EXT:
 *  @str_in: input string
 *  @str_out: output string
 *  @ind1: instance index
 *  @ind2: buffer index
 *
 *  add addtional extension to the output string
 **/
#define STR_ADD_EXT(str_in, str_out, ind1, ind2) ({ \
  snprintf(str_out, MAX_FN_LEN-1, "%s_%d_%d.jpg", str_in, ind1, ind2);\
})

/*JPEG SW Decode*/

void* buffer_allocate(buffer_t *p_buffer)
{
  void *l_buffer = NULL;

  int lrc = 0;
  struct ion_handle_data lhandle_data;

   p_buffer->alloc.len = p_buffer->size;
   p_buffer->alloc.align = 4096;
   p_buffer->alloc.flags = 0;
   p_buffer->alloc.heap_id_mask = 0x1 << ION_IOMMU_HEAP_ID;

   p_buffer->ion_fd = open("/dev/ion", O_RDONLY);
   if(p_buffer->ion_fd < 0) {
    ALOGE("%s :Ion open failed", __func__);
    goto ION_ALLOC_FAILED;
  }

  /* Make it page size aligned */
  p_buffer->alloc.len = (p_buffer->alloc.len + 4095) & (~4095U);
  lrc = ioctl(p_buffer->ion_fd, ION_IOC_ALLOC, &p_buffer->alloc);
  if (lrc < 0) {
    ALOGE("%s :ION allocation failed len %zu", __func__,
      p_buffer->alloc.len);
    goto ION_ALLOC_FAILED;
  }

  p_buffer->ion_info_fd.handle = p_buffer->alloc.handle;
  lrc = ioctl(p_buffer->ion_fd, ION_IOC_SHARE,
    &p_buffer->ion_info_fd);
  if (lrc < 0) {
    ALOGE("%s :ION map failed %s", __func__, strerror(errno));
    goto ION_MAP_FAILED;
  }

  p_buffer->p_pmem_fd = (int)p_buffer->ion_info_fd.fd;

  l_buffer = mmap(NULL, p_buffer->alloc.len, PROT_READ  | PROT_WRITE,
    MAP_SHARED, p_buffer->p_pmem_fd, 0);

  if (l_buffer == MAP_FAILED) {
    ALOGE("%s :ION_MMAP_FAILED: %s (%d)", __func__,
      strerror(errno), errno);
    goto ION_MAP_FAILED;
  }
  ALOGE("%s:%d] fd %d", __func__, __LINE__, p_buffer->p_pmem_fd);

  return l_buffer;

ION_MAP_FAILED:
  lhandle_data.handle = p_buffer->ion_info_fd.handle;
  ioctl(p_buffer->ion_fd, ION_IOC_FREE, &lhandle_data);
  return NULL;
ION_ALLOC_FAILED:
  return NULL;

}

int buffer_deallocate(buffer_t *p_buffer)
{
  int lrc = 0;
  size_t lsize = (p_buffer->size + 4095) & (~4095U);

  struct ion_handle_data lhandle_data;
  lrc = munmap(p_buffer->addr, lsize);

  close(p_buffer->ion_info_fd.fd);

  lhandle_data.handle = p_buffer->ion_info_fd.handle;
  ioctl(p_buffer->ion_fd, ION_IOC_FREE, &lhandle_data);

  close(p_buffer->ion_fd);
  return lrc;
}

struct jpeg_error_manager
{
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;    /* for return to caller */
};

typedef struct jpeg_error_manager * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
    my_error_ptr myerr = (my_error_ptr) cinfo->err;
    (*cinfo->err->output_message) (cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}

int JpegtoYUV(unsigned char*JpegBuf,int jpegsize,unsigned char* yuvBuf){
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_manager jerr;
    int row_stride = 0;
    FILE* fp = NULL;
    unsigned int len;
    unsigned char* rgb_buffer = NULL;
    int rgb_size;

    ALOGD("%s: Jpeg start SW Decode E",__func__);
    if(JpegBuf ==NULL || yuvBuf == NULL){
        ALOGE("%s:JpegBuf yuvBuf is NULL",__func__);
        return -1;
    }
    DUMP_TO_FILE("/data/misc/camera/dump_640_480_001.jpeg",JpegBuf,jpegsize);
    fp = fopen("/data/misc/camera/dump_640_480_001.jpeg", "rb");
    if (fp == NULL){
        ALOGE("%s:open jpeg_file failed",__func__);
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    if (setjmp(jerr.setjmp_buffer)){
        ALOGE("%s:Jpeg Decode setjmp_buffer error",__func__);
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        return -1;
    }
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    row_stride = cinfo.output_width * cinfo.output_components;
    int w  = cinfo.output_width;
    int h  = cinfo.output_height;
    rgb_size = row_stride * cinfo.output_height;
    // ALOGE("RGB info:output_width=%d,output_width=%d,row_stride=%d,rgb_size=%d,color_space=%d",
    //                     cinfo.output_width,
    //                     cinfo.output_height,
    //                     row_stride,
    //                     rgb_size,
    //                     cinfo.out_color_space);
    rgb_buffer = (unsigned char *)malloc(sizeof(char) * rgb_size);
    if(rgb_buffer == NULL){
        ALOGE("rgb_buffer malloc failed");
        jpeg_destroy_decompress(&cinfo);
        fclose(fp);
        return -1;
    }
    unsigned char* rowptr;
    while (cinfo.output_scanline < cinfo.output_height){
        rowptr = rgb_buffer + cinfo.output_scanline * row_stride;
        jpeg_read_scanlines(&cinfo,&rowptr, 1);
        rowptr += row_stride;
    }
    jpeg_finish_decompress(&cinfo);
    fclose(fp);
    jpeg_destroy_decompress(&cinfo);

    ALOGD("%s: Jpeg SW Decode end X",__func__);
    len = w*h*3/2;
    RGB2YUV(rgb_buffer,w,h,yuvBuf,len);
    free(rgb_buffer);
    return 0;
}

bool RGB2YUV(unsigned char* RgbBuf,int nWidth,int nHeight,unsigned char* yuvBuf,unsigned int len){
    int i, j;
    unsigned char*bufY, *bufU, *bufV,*bufRGB;
    memset(yuvBuf,0,(unsigned int )len);
    bufY = yuvBuf;
    bufV = yuvBuf + nWidth * nHeight;
    bufU = bufV+1;
    unsigned char y, u, v, r, g, b;

    for (j = 0; j<nHeight;j++){
        bufRGB = RgbBuf + nWidth * (nHeight - 1 - j) * 3 ;
        for (i = 0;i<nWidth;i++){
            r = *(bufRGB++);
            g = *(bufRGB++);
            b = *(bufRGB++);
            y = (unsigned char)( ( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16  ;
            u = (unsigned char)( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128 ;
            v = (unsigned char)( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128 ;
            *(bufY++) = MAX( 0, MIN(y, 255 ));
            if (j%2==0&&i%2 ==0){
                if (u>255){
                    u=255;
                }
                if (u<0){
                     u = 0;
                }
                *(bufU) =u;
                bufU += 2;
            }
            else{
                if (i%2==0){
                    if (v>255){
                        v = 255;
                    }
                    if (v<0){
                        v = 0;
                    }
                *(bufV) =v;
                bufV += 2;
                }
            }
        }
    }
    return true;
}

// /*JPEG HW Encode*/

void omx_enc_get_buffer_offset(OMX_U32 width, OMX_U32 height,
  OMX_U32* p_y_offset, OMX_U32* p_cbcr_offset, OMX_U32* p_buf_size,
  int usePadding,
  int rotation,
  OMX_U32 *p_cbcrStartOffset,
  float chroma_wt)
{
  ALOGD("omx_enc_get_buffer_offset width=%d,height=%d,chroma_wt=%f",width,height,chroma_wt);
  if (usePadding) {
    //int cbcr_offset = 0;
    uint32_t actual_size = width * height;
    uint32_t padded_size = CEILING16(width) * CEILING16(height);
    *p_y_offset = 0;
    *p_cbcr_offset = padded_size;
    if ((rotation == 90) || (rotation == 180)) {
      *p_y_offset += padded_size - actual_size;
      *p_cbcr_offset += ((padded_size - actual_size) >> 1);
    }
    *p_buf_size = PAD_TO_4K((uint32_t)((float)padded_size * chroma_wt));
  } else {
    *p_y_offset = 0;
    *p_cbcr_offset = 0;
    *p_buf_size = PAD_TO_4K((uint32_t)((float)(width*height) * chroma_wt));
    *p_cbcrStartOffset = PAD_TO_WORD((uint32_t)(width*height));
  }
  ALOGD("p_buf_size=%d,p_cbcrStartOffset =%d",*p_buf_size,*p_cbcrStartOffset);
}

OMX_ERRORTYPE omx_enc_deallocate_buffer(buffer_t *p_buffer,
  int use_pmem)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  int rc = 0;
  if (!p_buffer->addr) {
    /* buffer not allocated */
    return ret;
  }
  if (use_pmem) {
    rc = buffer_deallocate(p_buffer);
    memset(p_buffer, 0x0, sizeof(buffer_t));
  } else {
    free(p_buffer->addr);
    p_buffer->addr = NULL;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_send_buffers(void *data)
{
  omx_enc_t *p_client = (omx_enc_t *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t i = 0;
  QOMX_BUFFER_INFO lbuffer_info;

  memset(&lbuffer_info, 0x0, sizeof(QOMX_BUFFER_INFO));
  for (i = 0; i < (uint32_t)p_client->buf_count; i++) {
    lbuffer_info.fd = (uint32_t)p_client->in_buffer[i].p_pmem_fd;
    ALOGD("%s:%d] buffer %d fd - %d", __func__, __LINE__, i,
      (int)lbuffer_info.fd);
    ret = OMX_UseBuffer(p_client->p_handle, &(p_client->p_in_buffers[i]), 0,
      &lbuffer_info, p_client->total_size,
      p_client->in_buffer[i].addr);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }

    ret = OMX_UseBuffer(p_client->p_handle, &(p_client->p_out_buffers[i]),
      1, NULL, p_client->total_size, p_client->out_buffer[i].addr);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }

    ret = OMX_UseBuffer(p_client->p_handle, &(p_client->p_thumb_buf[i]), 2,
      &lbuffer_info, p_client->total_size,
      p_client->in_buffer[i].addr);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }

  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_free_buffers(void *data)
{
  omx_enc_t *p_client = (omx_enc_t *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t i = 0;

  for (i = 0; i < (uint32_t)p_client->buf_count; i++) {
    ALOGD("%s:%d] buffer %d", __func__, __LINE__, i);
    ret = OMX_FreeBuffer(p_client->p_handle, 0, p_client->p_in_buffers[i]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
    ret = OMX_FreeBuffer(p_client->p_handle, 2 , p_client->p_thumb_buf[i]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
    ret = OMX_FreeBuffer(p_client->p_handle, 1 , p_client->p_out_buffers[i]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_change_state(omx_enc_t *p_client,
  OMX_STATETYPE new_state, omx_pending_func_t p_exec)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  ALOGD("%s:%d] new_state %d p_exec %p", __func__, __LINE__,
    new_state, p_exec);

  pthread_mutex_lock(&p_client->lock);


  if (p_client->aborted) {
    ALOGD("%s:%d] Abort has been requested, quiting!!", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->lock);
    return OMX_ErrorNone;
  }

  if (p_client->omx_state != new_state) {
    p_client->state_change_pending = OMX_TRUE;
  } else {
    ALOGD("%s:%d] new_state is the same: %d ", __func__, __LINE__,
    new_state);

    pthread_mutex_unlock(&p_client->lock);

    return OMX_ErrorNone;
  }
  ALOGD("%s:%d] **** Before send command", __func__, __LINE__);
  ret = OMX_SendCommand(p_client->p_handle, OMX_CommandStateSet,
    new_state, NULL);
  ALOGD("%s:%d] **** After send command", __func__, __LINE__);

  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->lock);
    return OMX_ErrorIncorrectStateTransition;
  }
  ALOGD("%s:%d] ", __func__, __LINE__);
  if (p_client->error_flag) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    pthread_mutex_unlock(&p_client->lock);
    return OMX_ErrorIncorrectStateTransition;
  }
  if (p_exec) {
    ret = p_exec(p_client);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      pthread_mutex_unlock(&p_client->lock);
      return OMX_ErrorIncorrectStateTransition;
    }
  }
  ALOGD("%s:%d] ", __func__, __LINE__);
  if (p_client->state_change_pending) {
    ALOGD("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&p_client->cond, &p_client->lock);
    ALOGD("%s:%d] after wait", __func__, __LINE__);
    p_client->omx_state = new_state;
  }
  pthread_mutex_unlock(&p_client->lock);
  ALOGD("%s:%d] ", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_set_io_ports(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  ALOGD("%s:%d: E", __func__, __LINE__);

  p_client->inputPort = (OMX_PARAM_PORTDEFINITIONTYPE *)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  if (NULL == p_client->inputPort) {
    ALOGE("%s: Error in malloc for inputPort",__func__);
    return OMX_ErrorInsufficientResources;
  }

  p_client->outputPort = (OMX_PARAM_PORTDEFINITIONTYPE *)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  if (NULL == p_client->outputPort) {
    free(p_client->inputPort);
    ALOGE("%s: Error in malloc for outputPort ",__func__);
    return OMX_ErrorInsufficientResources;
  }
  p_client->thumbPort =(OMX_PARAM_PORTDEFINITIONTYPE *) malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  if (NULL == p_client->thumbPort) {
  free(p_client->inputPort);
  free(p_client->outputPort);
    ALOGE("%s: Error in malloc for thumbPort",__func__);
    return OMX_ErrorInsufficientResources;
  }

  p_client->inputPort->nPortIndex = 0;
  p_client->outputPort->nPortIndex = 1;
  p_client->thumbPort->nPortIndex = 2;

  ret = OMX_GetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->inputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  ret = OMX_GetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->thumbPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  ret = OMX_GetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->outputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  p_client->inputPort->format.image.nFrameWidth =
    p_client->main.width;
  p_client->inputPort->format.image.nFrameHeight =
    p_client->main.height;
  p_client->inputPort->format.image.nStride =
    p_client->main.width;
  p_client->inputPort->format.image.nSliceHeight =
    p_client->main.height;
  p_client->inputPort->format.image.eColorFormat =
    (OMX_COLOR_FORMATTYPE) p_client->main.eColorFormat;
  p_client->inputPort->nBufferSize = p_client->total_size;
  p_client->inputPort->nBufferCountActual = p_client->buf_count;
  ret = OMX_SetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->inputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  p_client->thumbPort->format.image.nFrameWidth =
    p_client->thumbnail.width;
  p_client->thumbPort->format.image.nFrameHeight =
    p_client->thumbnail.height;
  p_client->thumbPort->format.image.nStride =
    (int32_t)p_client->thumbnail.width;
  p_client->thumbPort->format.image.nSliceHeight =
    p_client->thumbnail.height;
  p_client->thumbPort->format.image.eColorFormat =
    (OMX_COLOR_FORMATTYPE) p_client->thumbnail.eColorFormat;
  p_client->thumbPort->nBufferSize = p_client->total_size;
  p_client->thumbPort->nBufferCountActual = p_client->buf_count;
  ret = OMX_SetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->thumbPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  // Enable thumbnail port
  if (p_client->thumbPort) {
  ret = OMX_SendCommand(p_client->p_handle, OMX_CommandPortEnable,
      p_client->thumbPort->nPortIndex, NULL);
  }

  p_client->outputPort->nBufferSize = p_client->total_size;
  p_client->outputPort->nBufferCountActual = p_client->buf_count;
  ret = OMX_SetParameter(p_client->p_handle, OMX_IndexParamPortDefinition,
    p_client->outputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  return ret;
}

OMX_ERRORTYPE omx_enc_configure_buffer_ext(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_INDEXTYPE buffer_index;

  ALOGD("%s:%d: E", __func__, __LINE__);

  omx_enc_get_buffer_offset(p_client->main.width,
    p_client->main.height,
    &p_client->frame_info.yOffset,
    &p_client->frame_info.cbcrOffset[0],
    &p_client->total_size,
    p_client->usePadding,
    p_client->rotation,
    &p_client->frame_info.cbcrStartOffset[0],
    p_client->main.chroma_wt);

  ret = OMX_GetExtensionIndex(p_client->p_handle,
    (OMX_STRING)"OMX.QCOM.image.exttype.bufferOffset", &buffer_index);
  if (ret != OMX_ErrorNone) {
    ALOGE("%s: %d] Failed", __func__, __LINE__);
    return ret;
  }
  ALOGD("%s:%d] yOffset = %u, cbcrOffset = %u, totalSize = %u,"
    "cbcrStartOffset = %u", __func__, __LINE__,
    (uint32_t)(p_client->frame_info.yOffset),
    (uint32_t)(p_client->frame_info.cbcrOffset[0]),
    (uint32_t)(p_client->total_size),
    (uint32_t)(p_client->frame_info.cbcrStartOffset[0]));

  ret = OMX_SetParameter(p_client->p_handle, buffer_index,
    &p_client->frame_info);
  if (ret != OMX_ErrorNone) {
    ALOGE("%s: %d] Failed", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_allocate_buffer(buffer_t *p_buffer,
  int use_pmem)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  /*Allocate buffers*/
  if (use_pmem) {
    p_buffer->addr = (uint8_t *)buffer_allocate(p_buffer);
    if (NULL == p_buffer->addr) {
      ALOGE("%s:%d] Error",__func__, __LINE__);
      return OMX_ErrorUndefined;
    }
  } else {
    /* Allocate heap memory */
    p_buffer->addr = (uint8_t *)malloc(p_buffer->size);
    if (NULL == p_buffer->addr) {
      ALOGE("%s:%d] Error",__func__, __LINE__);
      return OMX_ErrorUndefined;
    }
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_set_exif_info(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_INDEXTYPE exif_indextype;
  QOMX_EXIF_INFO exif_info;
  QEXIF_INFO_DATA exif_data[MAX_EXIF_ENTRIES];
  uint32_t num_exif_values = 0;

  ret = OMX_GetExtensionIndex(p_client->p_handle,
    (OMX_STRING)"OMX.QCOM.image.exttype.exif", &exif_indextype);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }

  exif_data[num_exif_values].tag_id = EXIFTAGID_GPS_LONGITUDE_REF;
  exif_data[num_exif_values].tag_entry.type = EXIF_ASCII;
  exif_data[num_exif_values].tag_entry.count = 2;
  exif_data[num_exif_values].tag_entry.copy = 1;

  sprintf(exif_data[num_exif_values].tag_entry.data._ascii,"%s","se");
  //exif_data[num_exif_values].tag_entry.data._ascii = "se";
  num_exif_values++;

  exif_data[num_exif_values].tag_id = EXIFTAGID_GPS_LONGITUDE;
  exif_data[num_exif_values].tag_entry.type = EXIF_RATIONAL;
  exif_data[num_exif_values].tag_entry.count = 1;
  exif_data[num_exif_values].tag_entry.copy = 1;
  exif_data[num_exif_values].tag_entry.data._rat.num = 31;
  exif_data[num_exif_values].tag_entry.data._rat.denom = 1;
  num_exif_values++;

  exif_info.numOfEntries = num_exif_values;
  exif_info.exif_data = exif_data;

  ret = OMX_SetParameter(p_client->p_handle, exif_indextype,
    &exif_info);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_set_thumbnail_data(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_THUMBNAIL_INFO thumbnail_info;
  OMX_INDEXTYPE thumb_indextype;

  if (p_client->encode_thumbnail) {
    ret = OMX_GetExtensionIndex(p_client->p_handle,
      (OMX_STRING)"OMX.QCOM.image.exttype.thumbnail",
      &thumb_indextype);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }

    /* fill thumbnail info*/
    thumbnail_info.scaling_enabled = p_client->tn_scale_cfg.enable;
    thumbnail_info.input_width = p_client->thumbnail.width;
    thumbnail_info.input_height = p_client->thumbnail.height;
    thumbnail_info.crop_info.nWidth = p_client->tn_scale_cfg.input_width;
    thumbnail_info.crop_info.nHeight = p_client->tn_scale_cfg.input_height;
    thumbnail_info.crop_info.nLeft = (int32_t)p_client->tn_scale_cfg.h_offset;
    thumbnail_info.crop_info.nTop = (int32_t)p_client->tn_scale_cfg.v_offset;
    thumbnail_info.output_width = p_client->tn_scale_cfg.output_width;
    thumbnail_info.output_height = p_client->tn_scale_cfg.output_height;
    thumbnail_info.tmbOffset = p_client->frame_info;

    ret = OMX_SetParameter(p_client->p_handle, thumb_indextype,
      &thumbnail_info);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
  }

  return ret;
}

OMX_ERRORTYPE omx_enc_set_quality(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_IMAGE_PARAM_QFACTORTYPE quality;

  quality.nPortIndex = 0;
  quality.nQFactor = p_client->main.quality;
  ALOGD("%s:%d] Setting main image quality %d",
    __func__, __LINE__, p_client->main.quality);
  ret = OMX_SetParameter(p_client->p_handle, OMX_IndexParamQFactor,
    &quality);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_encoding_mode(omx_enc_t *p_client)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_INDEXTYPE indextype;
  QOMX_ENCODING_MODE encoding_mode;

  rc = OMX_GetExtensionIndex(p_client->p_handle,
    (OMX_STRING)QOMX_IMAGE_EXT_ENCODING_MODE_NAME, &indextype);
  if (rc != OMX_ErrorNone) {
    ALOGE("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  /* hardcode to parallel encoding */

  if (p_client->encode_thumbnail) {
    encoding_mode = OMX_Parallel_Encoding;
  } else {
    encoding_mode = OMX_Serial_Encoding;
  }
  ALOGE("%s:%d] encoding mode = %d ", __func__, __LINE__,
    (int)encoding_mode);
  rc = OMX_SetParameter(p_client->p_handle, indextype, &encoding_mode);
  if (rc != OMX_ErrorNone) {
    ALOGE("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}

static OMX_ERRORTYPE omx_enc_set_workbuf(
  omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_WORK_BUFFER work_buffer;
  OMX_INDEXTYPE work_buffer_index;
  //int i;

  if (!p_client->work_buf.addr || !p_client->work_buf.size) {
    ALOGE("%s:%d] Error invalid input %d",
      __func__, __LINE__, ret);
    return OMX_ErrorUndefined;
  }
  //Pass the ION buffer to be used as o/p for HW
  memset(&work_buffer, 0x0, sizeof(QOMX_WORK_BUFFER));
  ret = OMX_GetExtensionIndex(p_client->p_handle,
    (OMX_STRING)QOMX_IMAGE_EXT_WORK_BUFFER_NAME,
    &work_buffer_index);
  if (ret) {
    ALOGE("%s:%d] Error getting work buffer index %d",
      __func__, __LINE__, ret);
    return ret;
  }
  work_buffer.fd = p_client->work_buf.p_pmem_fd;
  work_buffer.vaddr = p_client->work_buf.addr;
  work_buffer.length = (uint32_t)p_client->work_buf.size;
  ALOGE("%s:%d] Work buffer %d %p WorkBufSize: %d", __func__, __LINE__,
    work_buffer.fd, work_buffer.vaddr, work_buffer.length);

  ret = OMX_SetConfig(p_client->p_handle, work_buffer_index,
    &work_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_set_rotation_angle(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_CONFIG_ROTATIONTYPE rotType;
  rotType.nPortIndex = 0;
  rotType.nRotation = p_client->rotation;
  ret = OMX_SetConfig(p_client->p_handle, OMX_IndexConfigCommonRotate,
    &rotType);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_set_scaling_params(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_CONFIG_RECTTYPE recttype;
  OMX_CONFIG_RECTTYPE rect_type_in;

  memset(&rect_type_in, 0, sizeof(rect_type_in));
  rect_type_in.nPortIndex = 0;
  rect_type_in.nWidth = p_client->main_scale_cfg.input_width;
  rect_type_in.nHeight = p_client->main_scale_cfg.input_height;
  rect_type_in.nLeft = (int32_t)p_client->main_scale_cfg.h_offset;
  rect_type_in.nTop = (int32_t)p_client->main_scale_cfg.v_offset;

  ALOGD("%s:%d] OMX_IndexConfigCommonInputCrop w = %d, h = %d, l = %d, t = %d,"
    " port_idx = %d", __func__, __LINE__,
    (int)p_client->main_scale_cfg.input_width, (int)p_client->main_scale_cfg.input_height,
    (int)p_client->main_scale_cfg.h_offset, (int)p_client->main_scale_cfg.v_offset,
    (int)rect_type_in.nPortIndex);

  ret = OMX_SetConfig(p_client->p_handle, OMX_IndexConfigCommonInputCrop,
    &rect_type_in);
  if (OMX_ErrorNone != ret) {
    ALOGE("%s:%d] Error in setting input crop params", __func__, __LINE__);
    return ret;
  }

  recttype.nLeft = (int32_t)p_client->main_scale_cfg.h_offset;
  recttype.nTop = (int32_t)p_client->main_scale_cfg.v_offset;
  recttype.nWidth = p_client->main_scale_cfg.output_width;
  recttype.nHeight = p_client->main_scale_cfg.output_height;
  recttype.nPortIndex = 0;
  ALOGD("%s:%d] OMX_IndexConfigCommonOutputCrop w = %d, h = %d, l = %d, t = %d,"
    " port_idx = %d", __func__, __LINE__,
    (int)p_client->main_scale_cfg.output_width, (int)p_client->main_scale_cfg.output_height,
    (int)p_client->main_scale_cfg.h_offset, (int)p_client->main_scale_cfg.v_offset,
    (int)rect_type_in.nPortIndex);

  ret = OMX_SetConfig(p_client->p_handle, OMX_IndexConfigCommonOutputCrop,
    &recttype);
  if (ret) {
    ALOGE("%s:%d] Error in setting output crop params", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_read_file(const char *filename,
  buffer_t *p_buffer,
  omx_image_t *p_image __unused)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  FILE *fp = NULL;
  size_t file_size = 0;
  fp = fopen(filename, "rb");
  if (!fp) {
    ALOGE("%s:%d] error", __func__, __LINE__);
    return OMX_ErrorUndefined;
  }
  fseek(fp, 0, SEEK_END);
  file_size = (size_t)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  ALOGE("%s:%d] input file size is %zu buf_size %zu",
    __func__, __LINE__, file_size, p_buffer->size);

  if (p_buffer->size < file_size) {
    ALOGE("%s:%d] error %d %d", __func__, __LINE__,
      p_buffer->size,
      file_size);
    fclose(fp);
    return ret;
  }
  fread(p_buffer->addr, 1, p_buffer->size, fp);
  fclose(fp);
  return ret;
}

OMX_BOOL omx_enc_check_for_completion(omx_enc_t *p_client)
{
  if ((p_client->ebd_count == p_client->total_ebd_count) &&
    (p_client->fbd_count == p_client->buf_count)) {
    return OMX_TRUE;
  }
  return OMX_FALSE;
}

OMX_ERRORTYPE omx_enc_ebd(OMX_OUT OMX_HANDLETYPE hComponent __unused,
  OMX_OUT OMX_PTR pAppData, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer __unused)
{
  omx_enc_t *p_client = (omx_enc_t *) pAppData;
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  ALOGD("%s:%d] ebd count %d ", __func__, __LINE__, p_client->ebd_count);
  pthread_mutex_lock(&p_client->lock);
  p_client->ebd_count++;
  if (omx_enc_check_for_completion(p_client) == OMX_TRUE) {
    pthread_cond_signal(&p_client->cond);
  } else if (p_client->ebd_count < p_client->buf_count) {
    ret = OMX_EmptyThisBuffer(p_client->p_handle,
      p_client->p_in_buffers[p_client->ebd_count]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      pthread_mutex_unlock(&p_client->lock);
      return ret;
    }

    ret = OMX_EmptyThisBuffer(p_client->p_handle,
      p_client->p_thumb_buf[p_client->ebd_count]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      pthread_mutex_unlock(&p_client->lock);
      return ret;
    }
  }
  pthread_mutex_unlock(&p_client->lock);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_fbd(OMX_OUT OMX_HANDLETYPE hComponent __unused,
  OMX_OUT OMX_PTR pAppData, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  omx_enc_t *p_client = (omx_enc_t *) pAppData;
  //int rc = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  ALOGD("%s:%d] length = %d", __func__, __LINE__,
    (int)pBuffer->nFilledLen);
  *p_client->jpegLength = (int)pBuffer->nFilledLen;
  ALOGD("%s:%d] file = %s", __func__, __LINE__,
    p_client->output_file[p_client->fbd_count]);
  if(p_client->outputaddr == NULL){
      DUMP_TO_FILE(p_client->output_file[p_client->fbd_count], pBuffer->pBuffer,(size_t)pBuffer->nFilledLen);
  }
  else{
      memcpy(p_client->outputaddr,pBuffer->pBuffer,(size_t)pBuffer->nFilledLen);
  }

  ALOGD("%s:%d] fbd count %d buf_count %d ebd %d %d",
    __func__, __LINE__,
    p_client->fbd_count,
    p_client->buf_count,
    p_client->ebd_count,
    p_client->total_ebd_count);
  pthread_mutex_lock(&p_client->lock);
  p_client->fbd_count++;
  if (omx_enc_check_for_completion(p_client) == OMX_TRUE) {
    pthread_cond_signal(&p_client->cond);
  } else if (p_client->ebd_count < p_client->buf_count) {
    ret = OMX_FillThisBuffer(p_client->p_handle,
      p_client->p_out_buffers[p_client->fbd_count]);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      pthread_mutex_unlock(&p_client->lock);
      return ret;
    }
  }
  pthread_mutex_unlock(&p_client->lock);
  ALOGD("%s:%d] X", __func__, __LINE__);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_event_handler(
  OMX_IN OMX_HANDLETYPE hComponent __unused,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_EVENTTYPE eEvent,
  OMX_IN OMX_U32 nData1,
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData __unused)
{
  omx_enc_t *p_client = (omx_enc_t *)pAppData;

  ALOGD("%s:%d] %d %d %d", __func__, __LINE__, eEvent, (int)nData1,
    (int)nData2);

  if (eEvent == OMX_EventError) {
    pthread_mutex_lock(&p_client->lock);
    p_client->error_flag = OMX_TRUE;
    pthread_cond_signal(&p_client->cond);
    pthread_mutex_unlock(&p_client->lock);
  } else if (eEvent == OMX_EventCmdComplete) {
    pthread_mutex_lock(&p_client->lock);
    if (p_client->state_change_pending == OMX_TRUE) {
      p_client->state_change_pending = OMX_FALSE;
      pthread_cond_signal(&p_client->cond);
    }
    pthread_mutex_unlock(&p_client->lock);
  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_deinit(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t i = 0;
  ALOGD("%s:%d] encoding %d", __func__, __LINE__, p_client->encoding);

  if (p_client->encoding) {
    p_client->encoding = 0;
    //Reseting error flag since we are deining the component
    p_client->error_flag = OMX_FALSE;

    ret = omx_enc_change_state(p_client, OMX_StateIdle, NULL);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      p_client->last_error = (OMX_ERRORTYPE)ret;
      goto error;
    }
    ALOGD("%s:%d] ", __func__, __LINE__);
    ret = omx_enc_change_state(p_client, OMX_StateLoaded,
      omx_enc_free_buffers);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      p_client->last_error =(OMX_ERRORTYPE) ret;
      goto error;
    }
  }

error:

  for (i = 0; i < p_client->buf_count; i++) {
    p_client->in_buffer[i].size = p_client->total_size;
    ret = omx_enc_deallocate_buffer(&p_client->in_buffer[i],
      p_client->use_pmem);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
    }

    p_client->out_buffer[i].size = p_client->total_size;
    ret = omx_enc_deallocate_buffer(&p_client->out_buffer[i],
      1);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
    }
  }

  ret = omx_enc_deallocate_buffer(&p_client->work_buf,
    TRUE);
  if (ret) {
    ALOGE("%s:%d] Error %d", __func__, __LINE__, ret);
  }

  if (p_client->inputPort) {
    free(p_client->inputPort);
    p_client->inputPort = NULL;
  }

  if (p_client->outputPort) {
    free(p_client->outputPort);
    p_client->outputPort = NULL;
  }

  if (p_client->thumbPort) {
    free(p_client->thumbPort);
    p_client->thumbPort = NULL;
  }

  if (p_client->p_handle) {
    OMX_FreeHandle(p_client->p_handle);
    p_client->p_handle = NULL;
  }
  return ret;
}

OMX_ERRORTYPE omx_issue_abort(omx_enc_t *p_client)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  pthread_mutex_lock(&p_client->lock);

  p_client->aborted = OMX_TRUE;

  if (p_client->omx_state == OMX_StateExecuting) {
    // Abort
    //
    ALOGD("%s:%d] **** Abort requested & State is Executing -> ABORTING",
        __func__, __LINE__);
    ALOGD("%s:%d] **** Before abort send command", __func__, __LINE__);
    p_client->state_change_pending = OMX_TRUE;
    ret = OMX_SendCommand(p_client->p_handle, OMX_CommandStateSet,
        OMX_StateIdle, NULL);
    ALOGD("%s:%d] **** After abort send command", __func__, __LINE__);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
  } else {
    ALOGD("%s:%d] **** Abort rquested but state is not Executing",
        __func__, __LINE__);

  }

  pthread_mutex_unlock(&p_client->lock);
  return OMX_ErrorNone;

error:
  pthread_mutex_unlock(&p_client->lock);
  return ret;
}

OMX_BOOL omx_abort_is_pending(omx_enc_t *p_client)
{
  OMX_BOOL ret;

  pthread_mutex_lock(&p_client->lock);

  ret = p_client->aborted;

  pthread_mutex_unlock(&p_client->lock);

  return ret;
}

void *omx_abort_thread(void *data)
{
  omx_enc_t *p_client = (omx_enc_t *)data;
  struct timespec lTs;
  long int aMs = (long int)p_client->abort_time;
  int ret = clock_gettime(CLOCK_REALTIME, &lTs);

  if (ret < 0)
    goto error;

  if (aMs >= 1000) {
    lTs.tv_sec += (aMs / 1000);
    lTs.tv_nsec += ((aMs % 1000) * 1000000);
  } else {
    lTs.tv_nsec += (aMs * 1000000);
  }

  if (lTs.tv_nsec > 1E9) {
    lTs.tv_sec++;
    lTs.tv_nsec -= 1E9L;
  }

  ALOGD("%s:%d] **** ABORT THREAD ****", __func__, __LINE__);

  pthread_mutex_lock(&p_client->abort_mutx);

  ALOGD("%s:%d] before wait", __func__, __LINE__);
  ret = pthread_cond_timedwait(&p_client->abort_cond, &p_client->abort_mutx,
      &lTs);
  ALOGD("%s:%d] after wait", __func__, __LINE__);

  pthread_mutex_unlock(&p_client->abort_mutx);

  if (ret == ETIMEDOUT) {

    ALOGD("%s:%d] ****Issuing abort", __func__, __LINE__);
    ret = omx_issue_abort(p_client);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
  }

  return NULL;


error:
  p_client->last_error = (OMX_ERRORTYPE)ret;
  ALOGE("%s:%d] Error", __func__, __LINE__);
  return NULL;

}

void *omx_enc_encode(void *data)
{
  omx_enc_t *p_client = (omx_enc_t *)data;
  int ret = 0;
  uint32_t i = 0;
  struct timeval time[2];

  char name[40];
  memset(name,0,sizeof(name));
  sprintf(name,"%s","OMX.qcom.image.jpeg.encoder\0");
  ALOGE("%s:%d: E", __func__, __LINE__);
  ret = OMX_GetHandle(&p_client->p_handle,name, (void*)p_client, &p_client->callbacks);

  if ((ret != OMX_ErrorNone) || (p_client->p_handle == NULL)) {
    ALOGE("%s:%d] ", __func__, __LINE__);
    goto error;
  }

  if (p_client->abort_time) {
    ret = pthread_create(&p_client->abort_thread_id, NULL,
        omx_abort_thread,
        p_client);

    if (ret != 0) {
      fprintf(stderr, "Error in thread creation\n");
      return 0;
    }
  }

  ret = omx_enc_configure_buffer_ext(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_set_io_ports(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_set_quality(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  /* set encoding mode */
  omx_encoding_mode(p_client);

  /* set work buffer */
  if (p_client->use_workbuf) {
    p_client->work_buf.size = p_client->total_size;
    ret = omx_enc_allocate_buffer(&p_client->work_buf, TRUE);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
    omx_enc_set_workbuf(p_client);
  }

  /*Allocate input buffer*/
  for (i = 0; i < p_client->buf_count; i++) {
    p_client->in_buffer[i].size = p_client->total_size;
    ret = omx_enc_allocate_buffer(&p_client->in_buffer[i],
      p_client->use_pmem);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }

    p_client->in_buffer[i].size = p_client->total_size;
    if(p_client->inputaddr == NULL)
      ret = omx_enc_read_file(p_client->main.file_name,
      &p_client->in_buffer[i],
      &p_client->main);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
    else{
        memcpy(p_client->in_buffer[i].addr,p_client->inputaddr,p_client->total_size);
    }


    p_client->out_buffer[i].size = p_client->total_size;
    ret = omx_enc_allocate_buffer(&p_client->out_buffer[i],
      1);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
    if (omx_abort_is_pending(p_client))
      goto abort;
  }

  // /*set exif info*/
  // ret = omx_enc_set_exif_info(p_client);
  // if (ret) {
  //   ALOGE("%s:%d] Error", __func__, __LINE__);
  //   goto error;
  // }

  /*Set thumbnail data*/
  ret = omx_enc_set_thumbnail_data(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  if (omx_abort_is_pending(p_client))
      goto abort;

  /*Set rotation angle*/
  ret = omx_enc_set_rotation_angle(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  /*Set scaling parameters*/
  ret = omx_enc_set_scaling_params(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  if (omx_abort_is_pending(p_client))
      goto abort;

  ret = omx_enc_change_state(p_client, OMX_StateIdle,
    omx_enc_send_buffers);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  if (omx_abort_is_pending(p_client))
      goto abort;

  gettimeofday(&time[0], NULL);

  p_client->encoding = 1;
  ret = omx_enc_change_state(p_client, OMX_StateExecuting, NULL);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

#ifdef DUMP_INPUT
  DUMP_TO_FILE("/data/misc/camera/test.yuv",
    p_client->p_in_buffers[p_client->ebd_count]->pBuffer,
    (int)p_client->p_in_buffers[p_client->ebd_count]->nAllocLen);
#endif

#ifdef DUMP_THUMBNAIL
  DUMP_TO_FILE("/data/misc/camera/testThumbnail.yuv",
    p_client->p_thumb_buf[p_client->ebd_count]->pBuffer,
    (int)p_client->p_thumb_buf[p_client->ebd_count]->nAllocLen);
#endif

  gettimeofday(&time[0], NULL);
  ret = OMX_EmptyThisBuffer(p_client->p_handle,
    p_client->p_in_buffers[p_client->ebd_count]);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = OMX_EmptyThisBuffer(p_client->p_handle,
    p_client->p_thumb_buf[p_client->ebd_count]);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = OMX_FillThisBuffer(p_client->p_handle,
    p_client->p_out_buffers[p_client->fbd_count]);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  if (omx_abort_is_pending(p_client))
    goto abort;


  /* wait for the events*/

  pthread_mutex_lock(&p_client->lock);
  if (omx_enc_check_for_completion(p_client) == OMX_FALSE) {
    if (p_client->aborted) {
      goto abort;
    }
    ALOGD("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&p_client->cond, &p_client->lock);
    ALOGD("%s:%d] after wait", __func__, __LINE__);
  }

  pthread_mutex_unlock(&p_client->lock);


abort:

  pthread_mutex_unlock(&p_client->lock);
  gettimeofday(&time[1], NULL);
  p_client->encode_time = (uint64_t)(TIME_IN_US(time[1]) - TIME_IN_US(time[0]));
  p_client->encode_time /= 1000LL;

  ALOGD("%s:%d] ", __func__, __LINE__);
  /*invoke OMX deinit*/
  ret = omx_enc_deinit(p_client);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }
  ALOGD("%s:%d] ", __func__, __LINE__);

  // Signal and join abort thread
  if (p_client->abort_time) {
    pthread_mutex_lock(&p_client->abort_mutx);
    pthread_cond_signal(&p_client->abort_cond);
    pthread_mutex_unlock(&p_client->abort_mutx);
    pthread_join(p_client->abort_thread_id, NULL);
  }

  return NULL;

error:
  p_client->last_error = (OMX_ERRORTYPE)ret;
  ALOGE("%s:%d] Error", __func__, __LINE__);
  return NULL;
}

void omx_enc_init(omx_enc_t *p_client, omx_enc_args_t *p_test,
  int id)
{
  uint32_t i = 0;

  ALOGD("%s:%d: E", __func__, __LINE__);

  memset(p_client, 0x0, sizeof(omx_enc_t));
  p_client->abort_time = p_test->abort_time;
  p_client->main = p_test->main;
  p_client->thumbnail = p_test->thumbnail;
  p_client->rotation = p_test->rotation;
  p_client->encode_thumbnail = p_test->encode_thumbnail;
  p_client->use_pmem = p_test->use_pmem;
  p_client->main_scale_cfg = p_test->main_scale_cfg;
  p_client->tn_scale_cfg = p_test->tn_scale_cfg;
  p_client->buf_count = p_test->burst_count;
  p_client->usePadding = 0;
  p_client->use_workbuf = p_test->use_workbuf;

  if(p_test->encode_thumbnail) {
    p_client->total_ebd_count = p_client->buf_count * 2;
  } else {
    p_client->total_ebd_count = p_client->buf_count;
  }

  if ((p_client->buf_count == 1) && (p_test->instance_cnt == 1)) {
    strlcpy(p_client->output_file[0], p_test->output_file,
      strlen(p_test->output_file)+1);
  } else {
    for (i = 0; i < p_client->buf_count; i++)
      STR_ADD_EXT(p_test->output_file, p_client->output_file[i], id, i);
  }
  /*Set function callbacks*/
  p_client->callbacks.EmptyBufferDone = omx_enc_ebd;
  p_client->callbacks.FillBufferDone = omx_enc_fbd;
  p_client->callbacks.EventHandler = omx_enc_event_handler;

  pthread_mutex_init(&p_client->lock, NULL);
  pthread_cond_init(&p_client->cond, NULL);

  pthread_mutex_init(&p_client->abort_mutx, NULL);
  pthread_cond_init(&p_client->abort_cond, NULL);

  p_client->omx_state = OMX_StateInvalid;
  p_client->aborted = OMX_FALSE;
  ALOGD("%s:%d: X", __func__, __LINE__);
}

void omx_enc_init_args(omx_enc_args_t *p_test)
{
  /*Initialize the test argument structure*/
  memset(p_test, 0, sizeof(omx_enc_args_t));
 // p_test->output_file = output_file;
  //p_test->main.file_name = input_file;
  sprintf(p_test->output_file,"%s","data/misc/camera/dump_test.jpg");
  sprintf(p_test->main.file_name,"%s","data/misc/camera/dump_640_480_001_yuyv");
 // p_test->thumbnail.file_name = "thumbnaildump.yuv";

  p_test->main.width = 640;
  p_test->main.height = 480;
  p_test->thumbnail.width  = 160;
  p_test->thumbnail.height = 120;
  p_test->rotation = 0;
  p_test->encode_thumbnail = 0;
  p_test->main.eColorFormat = col_formats[0].eColorFormat;
  p_test->main.quality = 75;
  p_test->thumbnail.eColorFormat = col_formats[0].eColorFormat;
  p_test->thumbnail.quality = 75;
  p_test->main_scale_cfg.enable = 0;
  p_test->tn_scale_cfg.enable = 0;
  p_test->use_pmem = 1;
  p_test->instance_cnt = 1;
  p_test->burst_count = 1;
  p_test->abort_time = 0;
  p_test->auto_pad = 0;
  p_test->main.chroma_wt = col_formats[0].chroma_wt;
  p_test->use_workbuf = 1;
}

int encodeJpeg(unsigned char* inbuffer,unsigned char* outbuffer,int* length){
  int ret = 0;
  omx_enc_args_t test_args;
  //omx_enc_t client[MAX_INSTANCES];
  omx_enc_t* client;
  int i = 0;

  client = (omx_enc_t*)malloc(sizeof(omx_enc_t));
  ALOGD("%s:%d] enter", __func__, __LINE__);

  /*Initialize OMX Component*/
  if(OMX_ErrorNone !=OMX_Init()){
    ALOGD("OMX_Init failed");
    return -1;
  }

  /*Init test args struct with default values*/
  omx_enc_init_args(&test_args);

  /*Get Command line input and fill test args struct*/

  //for (i = 0; i < test_args.instance_cnt; i++) {
    //omx_enc_init(&client[i], &test_args, i);
  //client->outputaddr = outbuffer;
    omx_enc_init(client, &test_args, 0);
    client->inputaddr = inbuffer;
    client->outputaddr = outbuffer;
    client->jpegLength = length;
    //ret = pthread_create(&client[i].thread_id, NULL, omx_enc_encode,&client[i]);
    ret = pthread_create(&client->thread_id, NULL, omx_enc_encode,client);
    if (ret != 0) {
      fprintf(stderr, "Error in thread creation\n");
      return -1;
    }
 // }

  for (i = 0; i < test_args.instance_cnt; i++) {
    ALOGD("%s:%d] thread id > 0", __func__, __LINE__);
    pthread_join(client->thread_id, NULL);
  }

  free(client);
  OMX_Deinit();
  return 0;
}








