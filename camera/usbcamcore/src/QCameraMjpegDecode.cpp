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


void omx_enc_get_buffer_offset(
  OMX_U32 width,
  OMX_U32 height,
  float chroma_wt,
  OMX_U32* p_y_offset,
  OMX_U32* p_cbcr_offset,
  OMX_U32 *p_cbcrStartOffset,
  OMX_U32* p_buf_size)
{
  ALOGD("omx_enc_get_buffer_offset width=%d,height=%d,chroma_wt=%f",width,height,chroma_wt);
  *p_y_offset = 0;
  *p_cbcr_offset = 0;
  *p_buf_size = PAD_TO_4K((uint32_t)((float)(width*height) * chroma_wt));
  *p_cbcrStartOffset = PAD_TO_WORD((uint32_t)(width*height));
  ALOGD("p_buf_size=%d,p_cbcrStartOffset =%d",*p_buf_size,*p_cbcrStartOffset);
}

OMX_ERRORTYPE omx_enc_configure_buffer_ext(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_INDEXTYPE buffer_index;

  ALOGD("%s:%d: E", __func__, __LINE__);

  omx_enc_get_buffer_offset(task->img_width,
    task->img_height,
    task->img_chroma_wt,
    &task->frame_info.yOffset,
    &task->frame_info.cbcrOffset[0],
    &task->frame_info.cbcrStartOffset[0],
    &task->total_size);
  ret = OMX_GetExtensionIndex(task->omx_handle,
    (OMX_STRING)"OMX.QCOM.image.exttype.bufferOffset", &buffer_index);
  if (ret != OMX_ErrorNone) {
    ALOGE("%s: %d] Failed", __func__, __LINE__);
    return ret;
  }
  ALOGD("%s:%d] yOffset = %u, cbcrOffset = %u, totalSize = %u,"
    "cbcrStartOffset = %u", __func__, __LINE__,
    (uint32_t)(task->frame_info.yOffset),
    (uint32_t)(task->frame_info.cbcrOffset[0]),
    (uint32_t)(task->total_size),
    (uint32_t)(task->frame_info.cbcrStartOffset[0]));

  ret = OMX_SetParameter(task->omx_handle, buffer_index,
    &task->frame_info);
  if (ret != OMX_ErrorNone) {
    ALOGE("%s: %d] Failed", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_set_io_ports(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  ALOGD("%s:%d: E", __func__, __LINE__);

  task->inputPort = (OMX_PARAM_PORTDEFINITIONTYPE *)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  if (NULL == task->inputPort) {
    ALOGE("%s: Error in malloc for inputPort",__func__);
    return OMX_ErrorInsufficientResources;
  }

  task->outputPort = (OMX_PARAM_PORTDEFINITIONTYPE *)malloc(sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  if (NULL == task->outputPort) {
    free(task->inputPort);
    ALOGE("%s: Error in malloc for outputPort ",__func__);
    return OMX_ErrorInsufficientResources;
  }
  task->inputPort->nPortIndex = 0;
  task->outputPort->nPortIndex = 1;

  ret = OMX_GetParameter(task->omx_handle, OMX_IndexParamPortDefinition,
    task->inputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  ret = OMX_GetParameter(task->omx_handle, OMX_IndexParamPortDefinition,
    task->outputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  task->inputPort->format.image.nFrameWidth =
    task->img_width;
  task->inputPort->format.image.nFrameHeight =
    task->img_height;
  task->inputPort->format.image.nStride =
    task->img_width;
  task->inputPort->format.image.nSliceHeight =
    task->img_height;
  task->inputPort->format.image.eColorFormat =
    (OMX_COLOR_FORMATTYPE) task->img_eColorFormat;
  task->inputPort->nBufferSize = task->total_size;
  task->inputPort->nBufferCountActual = 1;
  ret = OMX_SetParameter(task->omx_handle, OMX_IndexParamPortDefinition,
    task->inputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  task->outputPort->nBufferSize = task->total_size;
  task->outputPort->nBufferCountActual = 1;
  ret = OMX_SetParameter(task->omx_handle, OMX_IndexParamPortDefinition,
    task->outputPort);
  if (ret) {
    ALOGE("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  return ret;
}

OMX_ERRORTYPE omx_enc_set_quality(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_IMAGE_PARAM_QFACTORTYPE quality;

  quality.nPortIndex = 0;
  quality.nQFactor = task->jpg_quality;
  ALOGD("%s:%d] Setting main image quality %d",
    __func__, __LINE__, task->jpg_quality);
  ret = OMX_SetParameter(task->omx_handle, OMX_IndexParamQFactor,
    &quality);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_encoding_mode(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_INDEXTYPE indextype;
  QOMX_ENCODING_MODE encoding_mode;

  rc = OMX_GetExtensionIndex(task->omx_handle,
    (OMX_STRING)QOMX_IMAGE_EXT_ENCODING_MODE_NAME, &indextype);
  if (rc != OMX_ErrorNone) {
    ALOGE("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  encoding_mode = OMX_Parallel_Encoding;
  ALOGE("%s:%d] encoding mode = %d ", __func__, __LINE__,
    (int)encoding_mode);
  rc = OMX_SetParameter(task->omx_handle, indextype, &encoding_mode);
  if (rc != OMX_ErrorNone) {
    ALOGE("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}

OMX_ERRORTYPE omx_enc_allocate_buffer(buffer_t *p_buffer)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  p_buffer->addr = (uint8_t *)buffer_allocate(p_buffer);
  if (NULL == p_buffer->addr) {
    ALOGE("%s:%d] Error",__func__, __LINE__);
    return OMX_ErrorUndefined;
  }
  return ret;
}

static OMX_ERRORTYPE omx_enc_set_workbuf(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_WORK_BUFFER work_buffer;
  OMX_INDEXTYPE work_buffer_index;

  if (!task->work_buf.addr || !task->work_buf.size) {
    ALOGE("%s:%d] Error invalid input %d",
      __func__, __LINE__, ret);
    return OMX_ErrorUndefined;
  }
  //Pass the ION buffer to be used as o/p for HW
  memset(&work_buffer, 0x0, sizeof(QOMX_WORK_BUFFER));
  ret = OMX_GetExtensionIndex(task->omx_handle,
    (OMX_STRING)QOMX_IMAGE_EXT_WORK_BUFFER_NAME,
    &work_buffer_index);
  if (ret) {
    ALOGE("%s:%d] Error getting work buffer index %d",
      __func__, __LINE__, ret);
    return ret;
  }
  work_buffer.fd = task->work_buf.p_pmem_fd;
  work_buffer.vaddr = task->work_buf.addr;
  work_buffer.length = (uint32_t)task->work_buf.size;
  ALOGE("%s:%d] Work buffer %d %p WorkBufSize: %d", __func__, __LINE__,
    work_buffer.fd, work_buffer.vaddr, work_buffer.length);

  ret = OMX_SetConfig(task->omx_handle, work_buffer_index,
    &work_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  return ret;
}

OMX_ERRORTYPE omx_enc_send_buffers(void *data)
{
  jpeg_encoder_task *task = (jpeg_encoder_task *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_BUFFER_INFO lbuffer_info;

  memset(&lbuffer_info, 0x0, sizeof(QOMX_BUFFER_INFO));
  lbuffer_info.fd = (uint32_t)task->in_buffer.p_pmem_fd;
  ret = OMX_UseBuffer(task->omx_handle, &(task->p_in_buffers), 0,
      &lbuffer_info, task->total_size,
      task->in_buffer.addr);
  if(ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }

  ret = OMX_UseBuffer(task->omx_handle, &(task->p_out_buffers),
      1, NULL, task->total_size, task->out_buffer.addr);
  if(ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_change_state(jpeg_encoder_task *task,
  OMX_STATETYPE new_state, omx_pending_func_t p_exec)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  ALOGD("%s:%d] new_state %d p_exec %p", __func__, __LINE__,
    new_state, p_exec);
  pthread_mutex_lock(&task->lock);
  if(task->omx_state != new_state) {
    task->state_change_pending = OMX_TRUE;
  } else {
    ALOGD("%s:%d] new_state is the same: %d ", __func__, __LINE__,
    new_state);
    pthread_mutex_unlock(&task->lock);
    return OMX_ErrorNone;
  }
  ALOGD("%s:%d] **** Before send command", __func__, __LINE__);
  ret = OMX_SendCommand(task->omx_handle, OMX_CommandStateSet,
    new_state, NULL);
  ALOGD("%s:%d] **** After send command", __func__, __LINE__);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    pthread_mutex_unlock(&task->lock);
    return OMX_ErrorIncorrectStateTransition;
  }
  ALOGD("%s:%d] ", __func__, __LINE__);
  if (task->error_flag) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    pthread_mutex_unlock(&task->lock);
    return OMX_ErrorIncorrectStateTransition;
  }
  if (p_exec) {
    ret = p_exec(task);
    if (ret) {
      ALOGE("%s:%d] Error", __func__, __LINE__);
      pthread_mutex_unlock(&task->lock);
      return OMX_ErrorIncorrectStateTransition;
    }
  }
  ALOGD("%s:%d] ", __func__, __LINE__);
  if (task->state_change_pending) {
    ALOGD("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&task->cond, &task->lock);
    ALOGD("%s:%d] after wait", __func__, __LINE__);
    task->omx_state = new_state;
  }
  pthread_mutex_unlock(&task->lock);
  ALOGD("%s:%d] ", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_event_handler(
  OMX_IN OMX_HANDLETYPE hComponent __unused,
  OMX_IN OMX_PTR pAppData,
  OMX_IN OMX_EVENTTYPE eEvent,
  OMX_IN OMX_U32 nData1,
  OMX_IN OMX_U32 nData2,
  OMX_IN OMX_PTR pEventData __unused)
{
  jpeg_encoder_task *task = (jpeg_encoder_task *)pAppData;

  ALOGD("%s:%d] %d, %d, %d", __func__, __LINE__, eEvent, (int)nData1,
    (int)nData2);

  if(eEvent == OMX_EventError) {
    pthread_mutex_lock(&task->lock);
    task->error_flag = OMX_TRUE;
    pthread_cond_signal(&task->cond);
    pthread_mutex_unlock(&task->lock);
  } else if(eEvent == OMX_EventCmdComplete) {
    pthread_mutex_lock(&task->lock);
    if (task->state_change_pending == OMX_TRUE) {
      task->state_change_pending = OMX_FALSE;
      pthread_cond_signal(&task->cond);
    }
    pthread_mutex_unlock(&task->lock);
  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_ebd(OMX_OUT OMX_HANDLETYPE hComponent __unused,
  OMX_OUT OMX_PTR pAppData, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer __unused)
{
  jpeg_encoder_task *task = (jpeg_encoder_task *) pAppData;
  ALOGD("%s:%d] ebd %p", __func__, __LINE__, task);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_fbd(OMX_OUT OMX_HANDLETYPE hComponent __unused,
  OMX_OUT OMX_PTR pAppData, OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
  jpeg_encoder_task *task = (jpeg_encoder_task *) pAppData;
  ALOGD("%s:%d] length = %d", __func__, __LINE__,
    (int)pBuffer->nFilledLen);
  //memcpy(p_client->outputaddr,pBuffer->pBuffer,(size_t)pBuffer->nFilledLen);
  pthread_mutex_lock(&task->lock);
  task->fill_buffer_done = OMX_TRUE;
  task->output_size = pBuffer->nFilledLen;
  pthread_cond_signal(&task->cond);
  pthread_mutex_unlock(&task->lock);
  ALOGD("%s:%d] X", __func__, __LINE__);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_enc_free_buffers(void *data)
{
  jpeg_encoder_task *task = (jpeg_encoder_task *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  ret = OMX_FreeBuffer(task->omx_handle, 0, task->p_in_buffers);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
  }
  ret = OMX_FreeBuffer(task->omx_handle, 1 , task->p_out_buffers);
  if (ret) {
     ALOGE("%s:%d] Error", __func__, __LINE__);
  }
  ALOGD("%s:%d]", __func__, __LINE__);
  return ret;
}

OMX_ERRORTYPE omx_enc_deallocate_buffer(buffer_t *p_buffer)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  int rc = 0;
  if (!p_buffer->addr) {
    return ret;
  }

  rc = buffer_deallocate(p_buffer);
  memset(p_buffer, 0x0, sizeof(buffer_t));
  return ret;
}

OMX_ERRORTYPE omx_enc_deinit(jpeg_encoder_task *task)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  ALOGD("%s:%d] encoding", __func__, __LINE__);
  omx_enc_change_state(task, OMX_StateIdle, NULL);
  omx_enc_change_state(task, OMX_StateLoaded,
      omx_enc_free_buffers);

  ret = omx_enc_deallocate_buffer(&task->work_buf);
  if (ret) {
    ALOGE("%s:%d] Error %d", __func__, __LINE__, ret);
  }

  if (task->inputPort) {
    free(task->inputPort);
    task->inputPort = NULL;
  }

  if (task->outputPort) {
    free(task->outputPort);
    task->outputPort = NULL;
  }

  if (task->omx_handle) {
    OMX_FreeHandle(task->omx_handle);
    task->omx_handle = NULL;
  }
  return ret;
}

void* encoder_task_run(void* data)
{
  int ret = 0;
  char component_name[128];
  memset(component_name, 0, sizeof(component_name));
  jpeg_encoder_task* task = (jpeg_encoder_task *)data;

  sprintf(component_name,"%s","OMX.qcom.image.jpeg.encoder\0");
  ALOGE("%s:%d: E", __func__, __LINE__);
  task->callbacks.EmptyBufferDone = omx_enc_ebd;
  task->callbacks.FillBufferDone = omx_enc_fbd;
  task->callbacks.EventHandler = omx_enc_event_handler;
  ret = OMX_GetHandle(&task->omx_handle, component_name, (void*)task, &task->callbacks);
  if ((ret != OMX_ErrorNone) || (task->omx_handle == NULL)) {
    ALOGE("%s:%d] ", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_configure_buffer_ext(task);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_set_io_ports(task);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_set_quality(task);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }
  omx_encoding_mode(task);

  task->work_buf.size = task->total_size;
  ret = omx_enc_allocate_buffer(&task->work_buf);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }
  omx_enc_set_workbuf(task);

  ret = omx_enc_change_state(task, OMX_StateIdle,
    omx_enc_send_buffers);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = omx_enc_change_state(task, OMX_StateExecuting, NULL);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = OMX_EmptyThisBuffer(task->omx_handle, task->p_in_buffers);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  ret = OMX_FillThisBuffer(task->omx_handle, task->p_out_buffers);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  /* wait for the events*/
  pthread_mutex_lock(&task->lock);
  if (!task->fill_buffer_done) {
    ALOGD("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&task->cond, &task->lock);
    ALOGD("%s:%d] after wait", __func__, __LINE__);
  }
  pthread_mutex_unlock(&task->lock);
  ret = omx_enc_deinit(task);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
    goto error;
  }
  ALOGD("%s:%d] ", __func__, __LINE__);
  return NULL;

error:
  ALOGE("%s:%d] Error", __func__, __LINE__);
  return NULL;
}

jpeg_encoder_task* createEncodeJpegTask(int width, int height, int quality)
{
  int ret = 0;
  jpeg_encoder_task* tmp = (jpeg_encoder_task*) malloc(sizeof(jpeg_encoder_task));
  if(tmp == NULL) {
    return tmp;
  }
  tmp->img_width = width;
  tmp->img_height = height;
  tmp->img_eColorFormat = OMX_QCOM_IMG_COLOR_FormatYVU420SemiPlanar;
  tmp->img_chroma_wt = 1.5;
  tmp->jpg_quality = quality;
  tmp->total_size = PAD_TO_4K((uint32_t)((float)(width * height) * tmp->img_chroma_wt));
  tmp->in_buffer.size = tmp->total_size;
  ret = omx_enc_allocate_buffer(&tmp->in_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
  }
  tmp->out_buffer.size = tmp->total_size;
  ret = omx_enc_allocate_buffer(&tmp->out_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
  }
  tmp->omx_state = OMX_StateInvalid;
  tmp->error_flag = OMX_FALSE;
  tmp->state_change_pending = OMX_FALSE;
  tmp->output_size = 0;
  tmp->fill_buffer_done = OMX_FALSE;
  pthread_mutex_init(&tmp->lock, NULL);
  pthread_cond_init(&tmp->cond, NULL);
  return tmp;
}

int encodeJpeg(jpeg_encoder_task* task)
{
  int ret;
  /*Initialize OMX Component*/
  if(OMX_ErrorNone !=OMX_Init()){
    ALOGD("OMX_Init failed");
    return -1;
  }
  ret = pthread_create(&task->thread_id, NULL, encoder_task_run, task);
  if (ret != 0) {
    return -1;
  }
  pthread_join(task->thread_id, NULL);
  OMX_Deinit();
  return 0;
}

void releaseEncodeJpegTask(jpeg_encoder_task* task)
{
  int ret = 0;
  if(task == NULL) {
    return;
  }
  ret = omx_enc_deallocate_buffer(&task->in_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
  }

  ret = omx_enc_deallocate_buffer(&task->out_buffer);
  if (ret) {
    ALOGE("%s:%d] Error", __func__, __LINE__);
  }

  free(task);
}