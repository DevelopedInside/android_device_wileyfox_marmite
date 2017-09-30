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
 /*#error uncomment this for compiler test!*/
//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QualcommUsbCamera"

#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <pthread.h>
#include <linux/uvcvideo.h>
#include <dirent.h>

//#include "QCameraHAL.h"
#include "QualcommUsbCamera.h"
#include "QCameraUsbPriv.h"
#include "QCameraMjpegDecode.h"
#include "QCameraUsbParm.h"
#include <gralloc_priv.h>
//#include <genlock.h>
#include <media/hardware/HardwareAPI.h>

#include "mm_jpeg_interface.h"
extern "C" {
#include <sys/time.h>
}

#include "QCameraPerf.h"

camera_device_ops_t usbcam_camera_ops = {
  .set_preview_window =         android::usbcam_set_preview_window,
  .set_callbacks =              android::usbcam_set_CallBacks,
  .enable_msg_type =            android::usbcam_enable_msg_type,
  .disable_msg_type =           android::usbcam_disable_msg_type,
  .msg_type_enabled =           android::usbcam_msg_type_enabled,

  .start_preview =              android::usbcam_start_preview,
  .stop_preview =               android::usbcam_stop_preview,
  .preview_enabled =            android::usbcam_preview_enabled,
  .store_meta_data_in_buffers = android::usbcam_store_meta_data_in_buffers,

  .start_recording =            android::usbcam_start_recording,
  .stop_recording =             android::usbcam_stop_recording,
  .recording_enabled =          android::usbcam_recording_enabled,
  .release_recording_frame =    android::usbcam_release_recording_frame,

  .auto_focus =                 android::usbcam_auto_focus,
  .cancel_auto_focus =          android::usbcam_cancel_auto_focus,

  .take_picture =               android::usbcam_take_picture,
  .cancel_picture =             android::usbcam_cancel_picture,

  .set_parameters =             android::usbcam_set_parameters,
  .get_parameters =             android::usbcam_get_parameters,
  .put_parameters =             android::usbcam_put_parameters,
  .send_command =               android::usbcam_send_command,

  .release =                    android::usbcam_release,
  .dump =                       android::usbcam_dump,
};

#define CAPTURE                 1
#define DISPLAY                 1
#define CALL_BACK               1
#define MEMSET                  0
#define FREAD_JPEG_PICTURE      0
#define JPEG_ON_USB_CAMERA      0
#define FILE_DUMP_CAMERA        0
#define FILE_DUMP_B4_DISP       0

namespace android {

static int detectDeviceResolutions(     camera_hardware_t* camHal);

static int initUsbCameraPreview(            camera_hardware_t *camHal, int capture = 0);
static int initUsbCameraRecording(          camera_hardware_t *camHal);

static int startUsbCamCapture(          camera_hardware_t *camHal);
static int startUsbCamRecording(        camera_hardware_t *camHal);
static int stopUsbCamCapture(           camera_hardware_t *camHal);
static int stopUsbCamRecording(           camera_hardware_t *camHal);
static int stopRecordingInternal(camera_hardware_t *camHal);
static int initV4L2mmap(                camera_hardware_t *camHal);
static int initV4L2mmapH264(            camera_hardware_t *camHal);
static int unInitV4L2mmap(              camera_hardware_t *camHal);
static int unInitV4L2mmapH264(              camera_hardware_t *camHal);
static int launch_preview_thread(       camera_hardware_t *camHal);
static int launch_recording_thread(     camera_hardware_t *camHal);
static int launch_picture_thread(       camera_hardware_t *camHal);

static int stopPreviewInternal(         camera_hardware_t *camHal);
static int get_buf_from_cam(            camera_hardware_t *camHal);
static int get_h264buf_from_cam(            camera_hardware_t *camHal);
static int put_buf_to_cam(              camera_hardware_t *camHal);
static int put_h264buf_to_cam(              camera_hardware_t *camHal);
//static int prvwThreadTakePictureInternal(camera_hardware_t *camHal);
static camera_memory_t* convert_data_frm_cam_to_disp(camera_hardware_t *camHal, int buffer_id);
static void * previewloop(void *);
static void * recordingloop(void *);
static void * takePictureThread(void *hcamHal);

//static void * takePictureThread( camera_hardware_t *camHal);
static int convert_YUYV_to_420_NV12(char *in_buf, char *out_buf, int wd, int ht);
static int detect_uvc_device(camera_hardware_t *camHal);
//static int allocate_ion_memory(QCameraHalMemInfo_t *mem_info, int ion_type);
//static int deallocate_ion_memory(QCameraHalMemInfo_t *mem_info);
static int ioctlLoop(int fd, int ioctlCmd, void *args);
int allocatevideobuffer(camera_hardware_t *camHal,int buffenum);
int freevideobuffer(camera_hardware_t *camHal,int buffenum);
int getmetadatafreeHandle(camera_hardware_t *camHal,int buffer_id,int count,int timestamp);
int mStoreMetaDataInBuffers = 0;
void write_image(void *data, const int size, int width, int height,const char *name);
typedef struct VideoNativeHandleMetadata media_metadata_buffer;
// camera_memory_t *mMetadata[MM_CAMERA_MAX_NUM_FRAMES];
// native_handle_t *mNativeHandle[MM_CAMERA_MAX_NUM_FRAMES];
int mMetaBufCount = 0;

void jpegEncodeCb   (jpeg_job_status_t status,
                       uint8_t thumbnailDroppedFlag,
                       uint32_t client_hdl,
                       uint32_t jobId,
                       uint8_t* out_data,
                       uint32_t data_size,
                       void *userData);

/* HAL function implementation goes here*/
static int openloop(const char* name, int flag)
{
    int fd = -1;
    int i = 0;
    for(i = 0; i < 5; i++) {
        fd = open(name, flag);
        if(fd > 0 || errno != EBUSY) {
            return fd;
        }
        ALOGD("try open %s count: %d", name, i + 1);
        usleep(100000);
    }
    return fd;
}

/**
 * The functions need to be provided by the camera HAL.
 *
 * If getNumberOfCameras() returns N, the valid cameraId for getCameraInfo()
 * and openCameraHardware() is 0 to N-1.
 */

extern "C" int usbcam_get_number_of_cameras()
{
    /* TBR: This is hardcoded currently to 1 USB camera */
    ALOGI("%s: usb camera number: 1", __func__);
    return 1;
}

extern "C" int usbcam_get_camera_info(int camera_id, struct camera_info *info)
{
    int rc = -1;
    ALOGI("%s: camera id: %d E", __func__, camera_id);

    /* TBR: This info is hardcoded currently irrespective of camera_id */
    if(info) {
        struct CameraInfo camInfo;
        memset((char*)&camInfo, -1, sizeof (struct CameraInfo));

        info->facing = CAMERA_FACING_EXTERNAL;  //CAMERA_FACING_FRONT;//CAMERA_FACING_BACK;
        info->orientation = 0;
        info->device_version = CAMERA_DEVICE_API_VERSION_1_0;
        rc = 0;
    }
    ALOGI("%s: X", __func__);
    return rc;
}

/* HAL should return NULL handle if it fails to open camera hardware. */
extern "C" int  usbcam_camera_device_open(int id,
          struct hw_device_t** hw_device)
{
    camera_device       *device = NULL;
    camera_hardware_t   *camHal;

    ALOGI("%s: camera id: %d E", __func__, id);

    /* initialize return handle value to NULL */
    *hw_device = NULL;

    camHal = new camera_hardware_t();
    if(!camHal) {
        ALOGE("%s:  end in no mem", __func__);
        goto error;
    }
    memset(camHal->dev_name, 0, sizeof(camHal->dev_name));
    memset(camHal->h264_dev_name, 0, sizeof(camHal->h264_dev_name));
    camHal->fd = camHal->h264_fd = 0;

    detect_uvc_device(camHal);
    if(camHal->dev_name[0] == 0) {
        ALOGE("%s do not detect the uvc device", __func__);
        goto error;
    }

    camHal->fd = openloop(camHal->dev_name, O_RDWR /* required */ | O_NONBLOCK);
    if (camHal->fd <  0) {
        ALOGE("%s: Cannot open '%s'", __func__, camHal->dev_name);
        goto error;
    }

    if(camHal->h264_dev_name[0] != 0) {
        camHal->h264_fd = openloop(camHal->h264_dev_name, O_RDWR /* required */ | O_NONBLOCK);
        if(camHal->h264_fd < 0) {
            ALOGE("%s: Cannot open '%s'", __func__, camHal->h264_dev_name);
            goto error;
        }
    }
    detectDeviceResolutions(camHal);
    usbCamInitDefaultParameters(camHal);
    close(camHal->fd);
    camHal->fd = 0;
    if(camHal->h264_fd > 0) {
        close(camHal->h264_fd);
        camHal->h264_fd = 0;
    }

    device                  = &camHal->hw_dev;
    device->common.close    = usbcam_close_camera_device;
    device->ops             = &usbcam_camera_ops;
    device->priv            = (void *)camHal;
    *hw_device              = &(device->common);
    camHal->prvwStoppedForPicture = 0;
    camHal->preview_thread_enable = 0;
    ALOGD("%s: camHal: %p", __func__, camHal);
    return 0;
error:
    if(camHal != NULL) {
        if(camHal->h264_fd > 0) {
            close(camHal->h264_fd);
        }
        if(camHal->fd > 0) {
            close(camHal->fd);
        }
        free(camHal);
    }
    return -1;
}

extern "C"  int usbcam_close_camera_device( hw_device_t *hw_dev)
{
    ALOGI("%s: device =%p E", __func__, hw_dev);
    int rc =  -1;
    camera_device_t *device     = (camera_device_t *)hw_dev;

    if(device) {
        camera_hardware_t *camHal   = (camera_hardware_t *)device->priv;
        if(camHal) {
            if(camHal->h264_fd > 0) {
                close(camHal->h264_fd);
            }
            if(camHal->fd > 0) {
                close(camHal->fd);
            }
            delete camHal;
        }else{
                ALOGE("%s: camHal is NULL pointer ", __func__);
        }
    }
    ALOGI("%s: X device =%p, rc = %d", __func__, hw_dev, rc);
    return rc;
}

int usbcam_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);
    Mutex::Autolock autoLock(camHal->lock);
    camHal->window = window;
    ALOGI("%s: X. rc = %d", __func__, rc);
    return rc;
}

void usbcam_set_CallBacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGI("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->notify_cb           = notify_cb;
    camHal->data_cb             = data_cb;
    camHal->data_cb_timestamp   = data_cb_timestamp;
    camHal->get_memory          = get_memory;
    camHal->cb_ctxt             = user;

    ALOGI("%s: X", __func__);
}

void usbcam_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);
    ALOGI("%s: msg_type: %d", __func__, msg_type);

    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->msgEnabledFlag |= msg_type;

    ALOGI("%s camHal->msgEnabledFlag: 0x%x X", __func__, camHal->msgEnabledFlag);
}

void usbcam_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);
    ALOGI("%s: msg_type: %d", __func__, msg_type);

    camera_hardware_t *camHal;
    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->msgEnabledFlag &= ~msg_type;

    ALOGI("%s camHal->msgEnabledFlag: 0x%x X", __func__, camHal->msgEnabledFlag);
}

int usbcam_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;
    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }

    Mutex::Autolock autoLock(camHal->lock);

    ALOGI("%s: X", __func__);
    return (camHal->msgEnabledFlag & msg_type);
}

int usbcam_start_preview(struct camera_device * device)
{
    ALOGI("%s: E", __func__);

    int rc = -1;
    camera_hardware_t *camHal = NULL;

    VALIDATE_DEVICE_HDL(camHal, device, -1);
    Mutex::Autolock autoLock(camHal->lock);
    if(camHal->fd <= 0) {
        camHal->fd = openloop(camHal->dev_name, O_RDWR /* required */ | O_NONBLOCK);
        if (camHal->fd <  0) {
            ALOGE("%s: Cannot open '%s'", __func__, camHal->dev_name);
            return -1;
        }
    }

    /* If preivew is already running, nothing to be done */
    if(camHal->previewEnabledFlag){
        ALOGI("%s: Preview is already running", __func__);
        return 0;
    }

    rc = initUsbCameraPreview(camHal);
    if(rc < 0) {
        ALOGE("%s: Failed to intialize the device", __func__);
    }else{
        rc = startUsbCamCapture(camHal);
        if(rc < 0) {
            ALOGE("%s: Failed to startUsbCamCapture", __func__);
        }else{
            if(!camHal->preview_thread_enable){
                rc = launch_preview_thread(camHal);
                if(rc < 0) {
                    ALOGE("%s: Failed to launch_preview_thread", __func__);
                }
                camHal->preview_thread_enable = 1;
            }
            camHal->snapshotEnabledFlag = 0;
        }
    }
    if(!rc)
        camHal->previewEnabledFlag = 1;

    ALOGD("%s: X", __func__);
    return rc;
}

void usbcam_stop_preview(struct camera_device * device)
{
    ALOGD("%s: E", __func__);

    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);
    rc = stopPreviewInternal(camHal);
    if(rc)
        ALOGE("%s: stopPreviewInternal returned error", __func__);
    ALOGI("%s: X", __func__);
    return;
}

/* This function is equivalent to is_preview_enabled */
int usbcam_preview_enabled(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    Mutex::Autolock autoLock(camHal->lock);

    ALOGI("%s: X", __func__);
    return camHal->previewEnabledFlag;
}

/* TBD */
int usbcam_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    ALOGI("%s: enable = %d E", __func__, enable);
    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    mStoreMetaDataInBuffers = enable;
    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_start_recording(struct camera_device * device)
{
    int rc = 0;
    ALOGD("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    Mutex::Autolock autoLock(camHal->lock);
    if(camHal->h264_fd <= 0 && camHal->h264_dev_name[0] != 0) {
        camHal->h264_fd = openloop(camHal->h264_dev_name, O_RDWR /* required */ | O_NONBLOCK);
        if (camHal->h264_fd <  0) {
            ALOGE("%s: Cannot open '%s'", __func__, camHal->h264_dev_name);
            goto error;
        }
    }
    camHal->videorecordingEnableFlag = 1;
    if(camHal->h264_fd > 0) {
        ALOGI("usb camera has h264 data for recording");
        rc = initUsbCameraRecording(camHal);
        if(rc < 0) {
            ALOGE("%s init usb camera for recording error", __func__);
            goto error;
        }
        rc = startUsbCamRecording(camHal);
        if(rc < 0) {
            ALOGE("%s init usb camera for recording error", __func__);
            goto error;
        }
        launch_recording_thread(camHal);
    }
    ALOGD("%s: X", __func__);
    return rc;
error:
    if(camHal->h264_fd > 0) {
        close(camHal->h264_fd);
        camHal->h264_fd = 0;
    }
    return -1;
}

/* TBD */
void usbcam_stop_recording(struct camera_device * device)
{
    ALOGD("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    if(camHal->h264_fd > 0) {
        Mutex::Autolock autoLock(camHal->lock);
        stopRecordingInternal(camHal);
        close(camHal->h264_fd);
        camHal->h264_fd = 0;
    }
    camHal->videorecordingEnableFlag = 0;
    ALOGD("%s: X", __func__);
}


/* TBD */
int usbcam_recording_enabled(struct camera_device * device)
{
    //int rc = 0;
    ALOGD("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGD("%s: X", __func__);

    return camHal->videorecordingEnableFlag;
}

/* TBD */
void usbcam_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    ALOGD("%s: opaque = %p E", __func__, opaque);
    camera_hardware_t *camHal;
    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    ALOGD("%s: X", __func__);
}

/* TBD */
int usbcam_auto_focus(struct camera_device * device)
{
    ALOGD("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGD("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_cancel_auto_focus(struct camera_device * device)
{
    int rc = 0;
    ALOGD("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGD("%s: X", __func__);
    return rc;
}

int usbcam_take_picture(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);

    Mutex::Autolock autoLock(camHal->lock);

    /* If take picture is already in progress, nothing t be done */
    if(camHal->takePictInProgress){
        ALOGI("%s: Take picture already in progress", __func__);
        return 0;
    }
    if(camHal->videorecordingEnableFlag == 0){
        camHal->takePictInProgress = 1;
        launch_picture_thread(camHal);
    } else {
        camHal->snapshotEnabledFlag = 1;
        ALOGD("camHal->snapshotEnabledFlag = %d", camHal->snapshotEnabledFlag);
    }

    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_cancel_picture(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGI("%s: X", __func__);
    return rc;
}

int usbcam_set_parameters(struct camera_device * device, const char *params)

{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);

    Mutex::Autolock autoLock(camHal->lock);

    rc = usbCamSetParameters(camHal, params);

    ALOGI("%s: X", __func__);
    return rc;
}

char* usbcam_get_parameters(struct camera_device * device)
{
    char *parms;
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;
    VALIDATE_DEVICE_HDL(camHal, device, NULL);

    Mutex::Autolock autoLock(camHal->lock);
    parms = usbCamGetParameters(camHal);
    ALOGI("%s: X", __func__);
    return parms;
}

void usbcam_put_parameters(struct camera_device * device, char *parm)

{
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    usbCamPutParameters(camHal, parm);

    ALOGI("%s: X", __func__);
    return;
}

/* TBD */
int usbcam_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int rc = 0;
    ALOGI("%s: E", __func__);
    ALOGI("%d, %d, %d", cmd, arg1, arg2);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
void usbcam_release(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }
#if 0
    Mutex::Autolock l(&mLock);

    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
        break;
    case QCAMERA_HAL_PREVIEW_START:
        break;
    case QCAMERA_HAL_PREVIEW_STARTED:
        stopPreviewInternal();
    break;
    case QCAMERA_HAL_RECORDING_STARTED:
        stopRecordingInternal();
        stopPreviewInternal();
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
        cancelPictureInternal();
        break;
    default:
        break;
    }
    mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
#endif
    ALOGI("%s: X", __func__);
}

/* TBD */
int usbcam_dump(struct camera_device * device, int fd)
{
    ALOGI("%s: fd = %d E", __func__, fd);
    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    ALOGI("%s: X", __func__);
    return rc;
}
/*****************************************************************************
*  Static function definitions below
*****************************************************************************/

/******************************************************************************/
/* No in place conversion supported. Output buffer and input MUST should be   */
/* different input buffer for a 4x4 pixel video                             ***/
/******                  YUYVYUYV          00 01 02 03 04 05 06 07 ************/
/******                  YUYVYUYV          08 09 10 11 12 13 14 15 ************/
/******                  YUYVYUYV          16 17 18 19 20 21 22 23 ************/
/******                  YUYVYUYV          24 25 26 27 28 29 30 31 ************/
/******************************************************************************/
/* output generated by this function ******************************************/
/************************** YYYY            00 02 04 06            ************/
/************************** YYYY            08 10 12 14            ************/
/************************** YYYY            16 18 20 22            ************/
/************************** YYYY            24 26 28 30            ************/
/************************** VUVU            03 01 07 05            ************/
/************************** VUVU            19 17 23 21            ************/
/******************************************************************************/

static int convert_YUYV_to_420_NV12(char *in_buf, char *out_buf, int wd, int ht)
{
    int rc =0;
    int row, col, uv_row;

    ALOGD("%s: E", __func__);
    /* Arrange Y */
    for(row = 0; row < ht; row++)
        for(col = 0; col < wd * 2; col += 2)
        {
            out_buf[row * wd + col / 2] = in_buf[row * wd * 2 + col];
        }

    /* Arrange UV */
    for(row = 0, uv_row = ht; row < ht; row += 2, uv_row++)
        for(col = 1; col < wd * 2; col += 4)
        {
            out_buf[uv_row * wd + col / 2]= in_buf[row * wd * 2 + col + 2];
            out_buf[uv_row * wd + col / 2 + 1]  = in_buf[row * wd * 2 + col];
        }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: getMjpegdOutputFormat
 * Description: This function maps display pixel format enum to JPEG output
 *              format enum
 *
 * Input parameters:
 *   dispFormat              - Display pixel format
 *
 * Return values:
 *      (int)mjpegOutputFormat
 *
 * Notes: none
 *****************************************************************************/
/*static int getMjpegdOutputFormat(int dispFormat)
{
    int mjpegOutputFormat = MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;

    if(HAL_PIXEL_FORMAT_YCrCb_420_SP == dispFormat)
        mjpegOutputFormat = MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;

    return mjpegOutputFormat;
}*/

/******************************************************************************
 * Function: ioctlLoop
 * Description: This function is a blocking call around ioctl
 *
 * Input parameters:
 *   fd             - IOCTL fd
 *   ioctlCmd       - IOCTL command
 *   args           - IOCTL arguments
 *
 * Return values:
 *      (int)mjpegOutputFormat
 *
 * Notes: none
 *****************************************************************************/
static int ioctlLoop(int fd, int ioctlCmd, void *args)
{
    int rc = -1;

    while(1)
    {
        rc = ioctl(fd, ioctlCmd, args);
        if(!((-1 == rc) && (EINTR == errno)))
            break;
    }
    return rc;
}

static int initV4L2mmapH264(camera_hardware_t *camHal)
{
    struct v4l2_requestbuffers  reqBufs;
    struct v4l2_buffer          tempBuf;

    ALOGD("%s: E", __func__);
    memset(&reqBufs, 0, sizeof(v4l2_requestbuffers));
    reqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBufs.memory  = V4L2_MEMORY_MMAP;
    reqBufs.count   = PRVW_CAP_BUF_CNT;

    if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_REQBUFS, &reqBufs)) {
        if (EINVAL == errno) {
            ALOGE("%s: does not support memory mapping\n", __func__);
        } else {
            ALOGE("%s: VIDIOC_REQBUFS failed", __func__);
        }
    }
    ALOGD("%s: VIDIOC_REQBUFS success", __func__);

    if (reqBufs.count < PRVW_CAP_BUF_CNT) {
        ALOGE("%s: Insufficient buffer memory on\n", __func__);
    }

    camHal->h264Buffers =
        ( bufObj* ) calloc(reqBufs.count, sizeof(bufObj));

    if (!camHal->h264Buffers) {
        ALOGE("%s: Out of memory\n", __func__);
    }

    /* Store the indexes in the context. Useful during releasing */
    for (camHal->n_h264_buffers = 0;
         camHal->n_h264_buffers < reqBufs.count;
         camHal->n_h264_buffers++) {

        memset(&tempBuf, 0, sizeof(tempBuf));

        tempBuf.index       = camHal->n_h264_buffers;
        tempBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory      = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_QUERYBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QUERYBUF failed", __func__);

        ALOGD("%s: VIDIOC_QUERYBUF success", __func__);

        camHal->h264Buffers[camHal->n_h264_buffers].len = tempBuf.length;
        camHal->h264Buffers[camHal->n_h264_buffers].data =
        mmap(NULL /* start anywhere */,
                  tempBuf.length,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  camHal->h264_fd, tempBuf.m.offset);

        if (MAP_FAILED == camHal->h264Buffers[camHal->n_h264_buffers].data)
            ALOGE("%s: mmap failed", __func__);
    }
    ALOGD("%s: X", __func__);

    return 0;
}

/******************************************************************************
 * Function: initV4L2mmap
 * Description: This function requests for V4L2 driver allocated buffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int initV4L2mmap(camera_hardware_t *camHal)
{
    struct v4l2_requestbuffers  reqBufs;
    struct v4l2_buffer          tempBuf;

    ALOGD("%s: E", __func__);
    memset(&reqBufs, 0, sizeof(v4l2_requestbuffers));
    reqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBufs.memory  = V4L2_MEMORY_MMAP;
    reqBufs.count   = PRVW_CAP_BUF_CNT;

    if (-1 == ioctlLoop(camHal->fd, VIDIOC_REQBUFS, &reqBufs)) {
        if (EINVAL == errno) {
            ALOGE("%s: does not support memory mapping\n", __func__);
        } else {
            ALOGE("%s: VIDIOC_REQBUFS failed", __func__);
        }
    }
    ALOGD("%s: VIDIOC_REQBUFS success", __func__);

    if (reqBufs.count < PRVW_CAP_BUF_CNT) {
        ALOGE("%s: Insufficient buffer memory on\n", __func__);
    }

    camHal->buffers =
        ( bufObj* ) calloc(reqBufs.count, sizeof(bufObj));

    if (!camHal->buffers) {
        ALOGE("%s: Out of memory\n", __func__);
    }

    /* Store the indexes in the context. Useful during releasing */
    for (camHal->n_buffers = 0;
         camHal->n_buffers < reqBufs.count;
         camHal->n_buffers++) {

        memset(&tempBuf, 0, sizeof(tempBuf));

        tempBuf.index       = camHal->n_buffers;
        tempBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory      = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_QUERYBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QUERYBUF failed", __func__);

        ALOGD("%s: VIDIOC_QUERYBUF success", __func__);

        camHal->buffers[camHal->n_buffers].len = tempBuf.length;
        camHal->buffers[camHal->n_buffers].data =
        mmap(NULL /* start anywhere */,
                  tempBuf.length,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  camHal->fd, tempBuf.m.offset);

        if (MAP_FAILED == camHal->buffers[camHal->n_buffers].data)
            ALOGE("%s: mmap failed", __func__);
    }
    ALOGD("%s: X", __func__);

    return 0;
}

/******************************************************************************
 * Function: unInitV4L2mmap
 * Description: This function unmaps the V4L2 driver buffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int unInitV4L2mmap(camera_hardware_t *camHal)
{
    int i, rc = 0;
    ALOGD("%s: E", __func__);

    for (i = 0; i < (int)camHal->n_buffers; i++)
        if (-1 == munmap(camHal->buffers[i].data, camHal->buffers[i].len)){
            ALOGE("%s: munmap failed for buffer: %d", __func__, i);
            rc = -1;
        }

    ALOGD("%s: X", __func__);
    return rc;
}

static int unInitV4L2mmapH264(camera_hardware_t *camHal)
{
    int i, rc = 0;
    ALOGD("%s: E", __func__);

    for (i = 0; i < (int)camHal->n_h264_buffers; i++)
        if (-1 == munmap(camHal->h264Buffers[i].data, camHal->h264Buffers[i].len)){
            ALOGE("%s: munmap failed for buffer: %d", __func__, i);
            rc = -1;
        }

    ALOGD("%s: X", __func__);
    return rc;
}


static int initUsbCameraRecording(camera_hardware_t* camHal)
{
    int     rc = -1;
    unsigned int     i = 0;
    struct  v4l2_capability     cap;
    struct  v4l2_format         v4l2format;

    ALOGI("%s: E", __func__);
    if(camHal == NULL || camHal->h264_fd <= 0) {
        ALOGE("%s the camera hava not h264 data", __func__);
        return -1;
    }

    if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            ALOGE( "%s: This is not V4L2 device\n", __func__);
            return -1;
        } else {
            ALOGE("%s: VIDIOC_QUERYCAP errno: %d", __func__, errno);
        }
    }
    ALOGD("%s: VIDIOC_QUERYCAP success", __func__);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("%s: This is not video capture device\n", __func__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s: This does not support streaming i/o\n", __func__);
        return -1;
    }

    memset(&v4l2format, 0, sizeof(v4l2format));
    ALOGI("%s: Capture format chosen: 0x%x. 0x%x:YUYV. 0x%x:MJPEG. 0x%x: H264, width = %d, height = %d",
        __func__, camHal->recordingFormat, V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_H264, camHal->videoWidth, camHal->videoHeight);
    v4l2format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    {
        v4l2format.fmt.pix.field       = V4L2_FIELD_NONE;
        v4l2format.fmt.pix.pixelformat = camHal->recordingFormat;
        v4l2format.fmt.pix.width       = camHal->videoWidth;
        v4l2format.fmt.pix.height      = camHal->videoHeight;

        if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_S_FMT, &v4l2format))
        {
            ALOGE("%s: VIDIOC_S_FMT failed: %s", __func__, strerror(errno));
            return -1;
        }
        ALOGD("%s: VIDIOC_S_FMT success", __func__);

        /* Note VIDIOC_S_FMT may change width and height. */
    }

    rc = initV4L2mmapH264(camHal);
    int tmpPreviewBuffSize = camHal->prevWidth * camHal->prevHeight * 3 / 2;
    for(i = 0; i < camHal->n_h264_buffers; i++) {
        camera_memory_t* tmp = camHal->get_memory(-1, tmpPreviewBuffSize, 1, camHal->cb_ctxt);
        camHal->returnFreeVideoBuffer(tmp);
    }
    return 0;
}

static int initUsbCameraPreview(camera_hardware_t *camHal, int capture)
{
    int     rc = -1;
    unsigned int     i = 0;
    struct  v4l2_capability     cap;
    struct  v4l2_cropcap        cropcap;
    struct  v4l2_crop           crop;
    struct  v4l2_format         v4l2format;
    int width;
    int height;

    ALOGI("%s: E", __func__);

    if (-1 == ioctlLoop(camHal->fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            ALOGE( "%s: This is not V4L2 device\n", __func__);
            return -1;
        } else {
            ALOGE("%s: VIDIOC_QUERYCAP errno: %d", __func__, errno);
        }
    }
    ALOGD("%s: VIDIOC_QUERYCAP success", __func__);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("%s: This is not video capture device\n", __func__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s: This does not support streaming i/o\n", __func__);
        return -1;
    }

    /* Select video input, video standard and tune here. */
    memset(&cropcap, 0, sizeof(cropcap));

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == ioctlLoop(camHal->fd, VIDIOC_CROPCAP, &cropcap)) {

        /* reset to default */
        crop.c = cropcap.defrect;
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ALOGD("%s: VIDIOC_CROPCAP success", __func__);
        if (-1 == ioctlLoop(camHal->fd, VIDIOC_S_CROP, &crop)) {
        switch (errno) {
            case EINVAL:
            /* Cropping not supported. */
                break;
            default:
            /* Errors ignored. */
                break;
            }
        }
        ALOGD("%s: VIDIOC_S_CROP success", __func__);

    } else {
        /* Errors ignored. */
        ALOGE("%s: VIDIOC_S_CROP failed", __func__);
    }

    width = (capture == 0 ? camHal->prevWidth : camHal->pictWidth);
    height = (capture == 0 ? camHal->prevHeight : camHal->pictHeight);
    memset(&v4l2format, 0, sizeof(v4l2format));
    ALOGI("%s: Capture format chosen: 0x%x. 0x%x:YUYV. 0x%x:MJPEG. 0x%x: H264, width = %d, height = %d",
        __func__, camHal->previewFormat, V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_H264,
        width,
        height);
    v4l2format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    {
        v4l2format.fmt.pix.field       = V4L2_FIELD_NONE;
        v4l2format.fmt.pix.pixelformat = camHal->previewFormat;
        v4l2format.fmt.pix.width       = width;
        v4l2format.fmt.pix.height      = height;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_S_FMT, &v4l2format))
        {
            ALOGE("%s: VIDIOC_S_FMT failed", __func__);
            return -1;
        }
        ALOGD("%s: VIDIOC_S_FMT success", __func__);

        /* Note VIDIOC_S_FMT may change width and height. */
    }

    /* TBR: In case of user pointer buffers, v4l2format.fmt.pix.sizeimage */
    /* might have to be calculated as per V4L2 sample application due to */
    /* open source driver bug */

    rc = initV4L2mmap(camHal);
    if(capture == 0) {
        int tmpPreviewBuffSize = width * height * 3 / 2;
        for(i = 0; i < camHal->n_buffers; i++) {
            camera_memory_t* tmp = camHal->get_memory(-1, tmpPreviewBuffSize, 1, camHal->cb_ctxt);
            camHal->returnFreePrevBuffer(tmp);
        }
    }
    ALOGI("%s: X", __func__);
    return rc;
}

static int startUsbCamRecording(camera_hardware_t *camHal)
{
    int         rc = -1;
    unsigned    int i;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    for (i = 0; i < camHal->n_h264_buffers; ++i) {
        struct v4l2_buffer tempBuf;

        memset(&tempBuf, 0, sizeof(tempBuf));
        tempBuf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory  = V4L2_MEMORY_MMAP;
        tempBuf.index   = i;

        if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_QBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QBUF for %d buffer failed", __func__, i);
        else
            ALOGD("%s: VIDIOC_QBUF for %d buffer success", __func__, i);
    }

    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_STREAMON, &v4l2BufType))
        ALOGE("%s: VIDIOC_STREAMON failed", __func__);
    else
    {
        ALOGD("%s: VIDIOC_STREAMON success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}


/******************************************************************************
 * Function: startUsbCamCapture
 * Description: This function queues buffer objects to the driver and sends
 *              STREAM ON command to the USB camera driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int startUsbCamCapture(camera_hardware_t *camHal)
{
    int         rc = -1;
    unsigned    int i;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    for (i = 0; i < camHal->n_buffers; ++i) {
        struct v4l2_buffer tempBuf;

        memset(&tempBuf, 0, sizeof(tempBuf));
        tempBuf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory  = V4L2_MEMORY_MMAP;
        tempBuf.index   = i;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_QBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QBUF for %d buffer failed", __func__, i);
        else
            ALOGD("%s: VIDIOC_QBUF for %d buffer success", __func__, i);
    }

    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->fd, VIDIOC_STREAMON, &v4l2BufType))
        ALOGE("%s: VIDIOC_STREAMON failed", __func__);
    else
    {
        ALOGD("%s: VIDIOC_STREAMON success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: stopUsbCamCapture
 * Description: This function sends STREAM OFF command to the USB camera driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int stopUsbCamCapture(camera_hardware_t *camHal)
{
    int         rc = -1;
    //unsigned    int i;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    if(!camHal->fd){
        ALOGE("%s: camHal->fd = NULL ", __func__);
        return -1;
    }
    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->fd, VIDIOC_STREAMOFF, &v4l2BufType)){
        ALOGE("%s: VIDIOC_STREAMOFF failed", __func__);
        rc = -1;
    }else{
        ALOGD("%s: VIDIOC_STREAMOFF success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}

static int stopUsbCamRecording(camera_hardware_t *camHal)
{
    int         rc = -1;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    if(!camHal->h264_fd){
        ALOGE("%s: camHal->fd = NULL ", __func__);
        return -1;
    }
    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_STREAMOFF, &v4l2BufType)){
        ALOGE("%s: VIDIOC_STREAMOFF failed", __func__);
        rc = -1;
    }else{
        ALOGD("%s: VIDIOC_STREAMOFF success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}


static int stopRecordingInternal(camera_hardware_t *camHal)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    if(camHal->videorecordingEnableFlag)
    {
        camHal->lock.unlock();
        camHal->videorecordingEnableFlag = 0;
         if(pthread_join(camHal->recordingThread, NULL)){
             ALOGE("%s: Error in pthread_join recording thread", __func__);
         }
        camHal->lock.lock();

        if(stopUsbCamRecording(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        if(unInitV4L2mmapH264(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        camera_memory_t* tmp;
        while(NULL != (tmp = camHal->getFreeVideoBuffer())) {
            if(tmp->release) {
                tmp->release(tmp);
            }
        }
    }

    ALOGD("%s: X, rc: %d", __func__, rc);
    return rc;
}


/******************************************************************************
 * Function: stopPreviewInternal
 * Description: This function sends EXIT command to prview loop thread,
 *              stops usb camera capture and uninitializes MMAP. This function
 *              assumes that calling function has locked camHal->lock
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int stopPreviewInternal(camera_hardware_t *camHal)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    if(camHal->previewEnabledFlag)
    {
         camHal->prvwCmdPending++;
         camHal->prvwCmd         = USB_CAM_PREVIEW_EXIT;

        /* yield lock while waiting for the preview thread to exit */
        camHal->previewEnabledFlag = 0;
        camHal->lock.unlock();
         if(pthread_join(camHal->previewThread, NULL)){
             ALOGE("%s: Error in pthread_join preview thread", __func__);
         }
        camHal->lock.lock();
        camHal->preview_thread_enable = 0;
        if(stopUsbCamCapture(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        if(unInitV4L2mmap(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        camera_memory_t* tmp;
        while(NULL != (tmp = camHal->getFreePrevBuffer())) {
            if(tmp->release) {
                tmp->release(tmp);
            }
        }
        close(camHal->fd);
        camHal->fd = 0;
    }

    ALOGD("%s: X, rc: %d", __func__, rc);
    return rc;
}
#if 0
/******************************************************************************
 * Function: prvwThreadTakePictureInternal
 * Description: This function processes one camera frame to get JPEG encoded
 *              picture.
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int prvwThreadTakePictureInternal(camera_hardware_t *camHal)
{
    int     rc = 0;
    QCameraHalMemInfo_t     *mem_info;
    ALOGD("%s: E", __func__);

    /************************************************************************/
    /* - If requested for shutter notfication, callback                     */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/

    /************************************************************************/
    /* - If requested for shutter notfication, callback                     */
    /************************************************************************/
    if (camHal->msgEnabledFlag & CAMERA_MSG_SHUTTER){
        camHal->lock.unlock();
        camHal->notify_cb(CAMERA_MSG_SHUTTER, 0, 0, camHal->cb_ctxt);
        camHal->lock.lock();
    }

#if CAPTURE
    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
    if (0 == get_buf_from_cam(camHal))
        ALOGD("%s: get_buf_from_cam success", __func__);
    else
        ALOGE("%s: get_buf_from_cam error", __func__);
#endif

    /************************************************************************/
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /************************************************************************/
    /* Optimization: If camera capture is JPEG format, need not compress! */
    /* instead, just data copy from capture buffer to picture buffer */
    if(V4L2_PIX_FMT_MJPEG == camHal->captureFormat){
        /* allocate heap memory for JPEG output */
        mem_info = &camHal->pictMem.mem_info[0];
        mem_info->size = camHal->curCaptureBuf.bytesused;
        /* TBD: allocate_ion_memory
        rc = QCameraHardwareInterface::allocate_ion_memory(mem_info,
                            ((0x1 << CAMERA_ZSL_ION_HEAP_ID) |
                            (0x1 << CAMERA_ZSL_ION_FALLBACK_HEAP_ID)));
        */
        if(rc)
            ALOGE("%s: ION memory allocation failed", __func__);

        camHal->pictMem.camera_memory[0] = camHal->get_memory(
                            mem_info->fd, mem_info->size, 1, camHal->cb_ctxt);
        if(!camHal->pictMem.camera_memory[0])
            ALOGE("%s: get_mem failed", __func__);

        memcpy( camHal->pictMem.camera_memory[0]->data,
                (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
                camHal->curCaptureBuf.bytesused);
    }

    /************************************************************************/
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /************************************************************************/
    if ((camHal->msgEnabledFlag & CAMERA_MSG_COMPRESSED_IMAGE) &&
            (camHal->data_cb)){
        camHal->lock.unlock();
        camHal->data_cb(CAMERA_MSG_COMPRESSED_IMAGE,
                        camHal->pictMem.camera_memory[0],
                        0, NULL, camHal->cb_ctxt);
        camHal->lock.lock();
    }
    /* release heap memory after the call back */
    if(camHal->pictMem.camera_memory[0])
        camHal->pictMem.camera_memory[0]->release(
            camHal->pictMem.camera_memory[0]);

    /* TBD: deallocate_ion_memory */
    //rc = QCameraHardwareInterface::deallocate_ion_memory(mem_info);
    if(rc)
        ALOGE("%s: ION memory de-allocation failed", __func__);

#if CAPTURE
    /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
       if(0 == put_buf_to_cam(camHal)) {
            ALOGD("%s: put_buf_to_cam success", __func__);
        }
        else
            ALOGE("%s: put_buf_to_cam error", __func__);
#endif

    ALOGD("%s: X, rc: %d", __func__, rc);
    return rc;
}
#endif //#if 0
/******************************************************************************
 * Function: cache_ops
 * Description: This function calls ION ioctl for cache related operations
 *
 * Input parameters:
 *  mem_info                - QCameraHalMemInfo_t structure with ION info
 *  buf_ptr                 - Buffer pointer that needs to be cache operated
 *  cmd                     - Cache command - clean/invalidate
 *
 * Return values:
 *   MM_CAMERA_OK       No error
 *   -1                 Error
 *
 * Notes: none
 *****************************************************************************/
int cache_ops(QCameraHalMemInfo_t *mem_info,
                                    void *buf_ptr,
                                    unsigned int cmd)
{
    //struct ion_flush_data cache_inv_data;
    //struct ion_custom_data custom_data;
    int ret = 0;
    if (NULL == mem_info || NULL == buf_ptr) {
        ALOGE("%s: mem_info is NULL, cmd = %d return here", __func__, cmd);
        return -1;
    }
#ifdef USE_ION

    memset(&cache_inv_data, 0, sizeof(cache_inv_data));
    memset(&custom_data, 0, sizeof(custom_data));
    cache_inv_data.vaddr = buf_ptr;
    cache_inv_data.fd = mem_info->fd;
    cache_inv_data.handle = mem_info->handle;
    cache_inv_data.length = mem_info->size;
    custom_data.cmd = cmd;
    custom_data.arg = (unsigned long)&cache_inv_data;

    ALOGD("%s: addr = %p, fd = %d, handle = %p length = %d, ION Fd = %d",
         __func__, cache_inv_data.vaddr, cache_inv_data.fd,
         cache_inv_data.handle, cache_inv_data.length,
         mem_info->main_ion_fd);
    if(mem_info->main_ion_fd > 0) {
        if(ioctl(mem_info->main_ion_fd, ION_IOC_CUSTOM, &custom_data) < 0) {
            ALOGE("%s: Cache Invalidate failed\n", __func__);
            ret = -1;
        }
    }
#endif

    return ret;
}


static int get_h264buf_from_cam(camera_hardware_t *camHal)
{
    int rc = -1;
    ALOGD("%s: E", __func__);
    {
        memset(&camHal->curVideoBuf, 0, sizeof(camHal->curVideoBuf));
        camHal->curVideoBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        camHal->curVideoBuf.memory = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_DQBUF, &camHal->curVideoBuf)){
            switch (errno) {
            case EAGAIN:
                ALOGE("%s: EAGAIN error", __func__);
                return 1;
            case EIO:
            default:
                ALOGE("%s: VIDIOC_DQBUF error", __func__);
            }
        } else {
            rc = 0;
            ALOGD("%s: VIDIOC_DQBUF: %d successful, %d bytes",
                   __func__, camHal->curVideoBuf.index,
                   camHal->curVideoBuf.bytesused);
        }
   }
   ALOGD("%s: X", __func__);
   return rc;
}


static int put_h264buf_to_cam(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);

    camHal->curVideoBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camHal->curVideoBuf.memory      = V4L2_MEMORY_MMAP;


    if (-1 == ioctlLoop(camHal->h264_fd, VIDIOC_QBUF, &camHal->curVideoBuf))
    {
        ALOGE("%s: VIDIOC_QBUF failed ", __func__);
        return 1;
    }
    ALOGD("%s: X", __func__);
    return 0;
}


/******************************************************************************
 * Function: get_buf_from_cam
 * Description: This funtions gets/acquires 1 capture buffer from the camera
 *              driver. The fetched buffer is stored in curCaptureBuf
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int get_buf_from_cam(camera_hardware_t *camHal)
{
    int rc = -1;

    ALOGD("%s: E", __func__);
    {
        memset(&camHal->curCaptureBuf, 0, sizeof(camHal->curCaptureBuf));

        camHal->curCaptureBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        camHal->curCaptureBuf.memory = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_DQBUF, &camHal->curCaptureBuf)){
            switch (errno) {
            case EAGAIN:
                ALOGE("%s: EAGAIN error", __func__);
                return 1;

            case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

            default:
            ALOGE("%s: VIDIOC_DQBUF error", __func__);
            }
        }
        else
        {
            rc = 0;
            ALOGD("%s: VIDIOC_DQBUF: %d successful, %d bytes",
                 __func__, camHal->curCaptureBuf.index,
                 camHal->curCaptureBuf.bytesused);
        }
    }
    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: put_buf_to_cam
 * Description: This funtion puts/releases 1 capture buffer back to the camera
 *              driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int put_buf_to_cam(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);

    camHal->curCaptureBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camHal->curCaptureBuf.memory      = V4L2_MEMORY_MMAP;


    if (-1 == ioctlLoop(camHal->fd, VIDIOC_QBUF, &camHal->curCaptureBuf))
    {
        ALOGE("%s: VIDIOC_QBUF failed ", __func__);
        return 1;
    }
    ALOGD("%s: X", __func__);
    return 0;
}

/******************************************************************************
 * Function: put_buf_to_display
 * Description: This funtion transfers the content from capture buffer to
 *              preiew display buffer after appropriate conversion
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *  buffer_id               - id of the buffer that needs to be enqueued
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static camera_memory_t* convert_data_frm_cam_to_disp(camera_hardware_t *camHal, int buffer_id)
{
    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL;
    }
    camera_memory_t *tmp = camHal->getFreePrevBuffer();
    if(tmp == NULL) {
        ALOGE("%s: camHal there is no free prev buffer", __func__);
        return NULL;
    }
    /* If input and output are raw formats, but different color format, */
    /* call color conversion routine                                    */
    if( (V4L2_PIX_FMT_YUYV == camHal->previewFormat) &&
        (HAL_PIXEL_FORMAT_YCrCb_420_SP == camHal->dispFormat))
    {
        convert_YUYV_to_420_NV12(
            (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
            (char *)tmp->data,
            camHal->prevWidth,
            camHal->prevHeight);
        ALOGD("%s: Copied %d bytes from camera buffer %d to display buffer: %d",
             __func__, camHal->curCaptureBuf.bytesused,
             camHal->curCaptureBuf.index, buffer_id);
    }
    return tmp;
}

static int launch_recording_thread(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&camHal->recordingThread, &attr, recordingloop, camHal);

    ALOGD("%s: X", __func__);
    return rc;
}


/******************************************************************************
 * Function: launch_preview_thread
 * Description: This is a wrapper function to start preview thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int launch_preview_thread(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&camHal->previewThread, &attr, previewloop, camHal);

    ALOGD("%s: X", __func__);
    return rc;
}


void write_image(void *data, const int size, int width, int height,const char *name)
{
    char filename[80];
    static unsigned int count = 0;
    //unsigned int i;
    size_t bytes;
    FILE *fp;
   ALOGE(" Write dump write_image");
    snprintf(filename, sizeof(filename), "/data/misc/camera/dump_%d_%d_%03u.%s", width,
              height, count, name);
    fp = fopen (filename, "w+");
    if (fp == NULL) {
        ALOGE ("open file %s failed %s", filename, strerror (errno));
        return ;
    }
    if ((bytes = fwrite (data, size, 1, fp)) < (size_t)size)
        ALOGE (" Write less raw bytes to %s: %d, %d", filename, size, bytes);

    count++;

    fclose (fp);
}

static void * recordingloop(void *hcamHal)
{
    pid_t               tid         = 0;
    camera_hardware_t   *camHal     = NULL;
    camera_memory_t     *data       = NULL;

    camHal = (camera_hardware_t *)hcamHal;
    ALOGD("%s: E", __func__);

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL ;
    }

    tid  = gettid();
    /* TBR: Set appropriate thread priority */
    androidSetThreadPriority(tid, ANDROID_PRIORITY_NORMAL);
    prctl(PR_SET_NAME, (unsigned long)"Camera HAL preview thread", 0, 0, 0);

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /* - Check if any preview thread commands are set. If set, process      */
    /* - Dequeue display buffer from surface                                */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Convert capture format to display format                           */
    /* - If preview frames callback is requested, callback with prvw buffers*/
    /* - Enqueue display buffer back to surface                             */
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
    while(camHal->videorecordingEnableFlag) {
        fd_set fds;
        struct timeval tv;
        int r = 0;

        FD_ZERO(&fds);
        FD_SET(camHal->h264_fd, &fds);

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /************************************************************************/
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        r = select(camHal->h264_fd + 1, &fds, NULL, NULL, &tv);
        ALOGD("%s: after select : %d", __func__, camHal->h264_fd);
        if (0 >= r) {
            if(r == 0) {
                ALOGD("%s: select timeout\n", __func__);
            }
            continue;
        }
        nsecs_t timestamp;
        timestamp = systemTime();
        /* Protect the context for one iteration of preview loop */
        /* this gets unlocked at the end of the while */
        Mutex::Autolock autoLock(camHal->lock);

    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
        if (0 == get_h264buf_from_cam(camHal))
            ALOGD("%s: get_buf_from_cam success", __func__);
        else
            ALOGE("%s: get_buf_from_cam error", __func__);
        data = camHal->getFreeVideoBuffer();
        if(data != NULL) {
            ALOGD("memcpy %p, %p, %d", data->data, camHal->h264Buffers[camHal->curVideoBuf.index].data, camHal->curVideoBuf.bytesused);
            memcpy(data->data, &camHal->curVideoBuf.bytesused, sizeof(camHal->curVideoBuf.bytesused));
            memcpy((char*)data->data + sizeof(camHal->curVideoBuf.bytesused), camHal->h264Buffers[camHal->curVideoBuf.index].data, camHal->curVideoBuf.bytesused);
        }
     /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
       if(0 == put_h264buf_to_cam(camHal)) {
            ALOGD("%s: put_buf_to_cam success", __func__);
        }
        else
            ALOGE("%s: put_buf_to_cam error", __func__);
        if(data == NULL) {
            continue;
        }
        camHal->lock.unlock();
        camHal->data_cb_timestamp(timestamp,CAMERA_MSG_VIDEO_FRAME, data, 0, camHal->cb_ctxt);
        camHal->lock.lock();
        camHal->returnFreeVideoBuffer(data);
    }//while(1)
    ALOGD("%s: X", __func__);
    return (void *)0;
}


/******************************************************************************
 * Function: launch_preview_thread
 * Description: This is thread funtion for preivew loop
 *
 * Input parameters:
 *  hcamHal                 - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static void * previewloop(void *hcamHal)
{
    int                 rc;
    int                 buffer_id   = 0;
    pid_t               tid         = 0;
    camera_hardware_t   *camHal     = NULL;
    camera_memory_t     *data       = NULL;
    //camera_memory_t     *videodata  = NULL;
    //camera_frame_metadata_t *metadata= NULL;
    //camera_memory_t     *previewMem = NULL;
    //int video_buffer_id = 0;
    camHal = (camera_hardware_t *)hcamHal;
    ALOGD("%s: E", __func__);

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL ;
    }

    tid  = gettid();
    /* TBR: Set appropriate thread priority */
    androidSetThreadPriority(tid, ANDROID_PRIORITY_NORMAL);
    prctl(PR_SET_NAME, (unsigned long)"Camera HAL preview thread", 0, 0, 0);

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /* - Check if any preview thread commands are set. If set, process      */
    /* - Dequeue display buffer from surface                                */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Convert capture format to display format                           */
    /* - If preview frames callback is requested, callback with prvw buffers*/
    /* - Enqueue display buffer back to surface                             */
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
    while(camHal->previewEnabledFlag) {
        fd_set fds;
        struct timeval tv;
        int r = 0;

        FD_ZERO(&fds);
        FD_SET(camHal->fd, &fds);

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /************************************************************************/
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        r = select(camHal->fd + 1, &fds, NULL, NULL, &tv);
        ALOGD("%s: after select : %d", __func__, camHal->fd);
        if (0 >= r) {
            if(r == 0) {
                ALOGD("%s: select timeout\n", __func__);
            }
            continue;
        }
        nsecs_t timestamp;
        timestamp = systemTime();
        /* Protect the context for one iteration of preview loop */
        /* this gets unlocked at the end of the while */
        Mutex::Autolock autoLock(camHal->lock);

    /************************************************************************/
    /* - Check if any preview thread commands are set. If set, process      */
    /************************************************************************/
        /*if(camHal->prvwCmdPending)
        {
            camHal->prvwCmdPending--;
            //sempost(ack)
            if(USB_CAM_PREVIEW_EXIT == camHal->prvwCmd){
                camHal->lock.unlock();
                ALOGI("%s: Exiting coz USB_CAM_PREVIEW_EXIT", __func__);
                return (void *)0;
            }else if(USB_CAM_PREVIEW_TAKEPIC == camHal->prvwCmd){
                rc = prvwThreadTakePictureInternal(camHal);
                if(rc)
                    ALOGE("%s: prvwThreadTakePictureInternal returned error",
                    __func__);
            }
        }*/

        /* Null check on preview window. If null, sleep */
        if(!camHal->window) {
            ALOGD("%s: sleeping coz camHal->window = NULL",__func__);
            camHal->lock.unlock();
            sleep(2);
            continue;
        }

    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
        if (0 == get_buf_from_cam(camHal))
            ALOGD("%s: get_buf_from_cam success", __func__);
        else
            ALOGE("%s: get_buf_from_cam error", __func__);

#if FILE_DUMP_CAMERA
        write_image(camHal->buffers[camHal->curCaptureBuf.index].data,
            camHal->prevWidth * camHal->prevHeight * 2,
            camHal->prevWidth,
            camHal->prevHeight,
            "yuyv");
#endif

        data = convert_data_frm_cam_to_disp(camHal, buffer_id);
        ALOGD("%s: Copied data to buffer_id: %d, precv buffer count: %d ", __func__, buffer_id, camHal->prevFreeBufs.size());
        ALOGD("camHal->snapshotEnabledFlag = %d", camHal->snapshotEnabledFlag);
        if(camHal->snapshotEnabledFlag) {
            jpeg_encoder_task* task = createEncodeJpegTask(camHal->prevWidth, camHal->prevHeight, 80);
            rc = convert_YUYV_to_420_NV12(
                (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
                (char *)task->in_buffer.addr, camHal->prevWidth, camHal->prevHeight);
            add_time_water_marking_to_usbcameraframe((char*)task->in_buffer.addr, camHal->prevWidth,camHal->prevHeight,camHal->prevWidth);
            encodeJpeg(task);
            camHal->pictMem.camera_memory[0] = camHal->get_memory(
                    -1, task->output_size, 1, camHal->cb_ctxt);
            memcpy(camHal->pictMem.camera_memory[0]->data, task->out_buffer.addr, task->output_size);
            releaseEncodeJpegTask(task);
            /************************************************************************/
            /* - If jpeg frames callback is requested, callback with jpeg buffers   */
            /************************************************************************/
            /* TBD: CAMERA_MSG_RAW_IMAGE data call back */

            if ((camHal->msgEnabledFlag & CAMERA_MSG_COMPRESSED_IMAGE) &&
                    (camHal->data_cb)){
                /* Unlock temporarily, callback might call HAL api in turn */
                camHal->lock.unlock();
                camHal->data_cb(CAMERA_MSG_COMPRESSED_IMAGE,
                                camHal->pictMem.camera_memory[0],
                                0, NULL, camHal->cb_ctxt);
                camHal->lock.lock();
            }

            /* release heap memory after the call back */
            if(camHal->pictMem.camera_memory[0])
                camHal->pictMem.camera_memory[0]->release(
                    camHal->pictMem.camera_memory[0]);
            camHal->takePictInProgress = 0;
            camHal->snapshotEnabledFlag = 0;
        }

     /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
       if(0 == put_buf_to_cam(camHal)) {
            ALOGD("%s: put_buf_to_cam success", __func__);
        }
        else
            ALOGE("%s: put_buf_to_cam error", __func__);

        if(data == NULL) {
            continue;
        }
        ALOGI("%s camHal->msgEnabledFlag: 0x%x X", __func__, camHal->msgEnabledFlag);
        camHal->lock.unlock();
        if((camHal->msgEnabledFlag & CAMERA_MSG_PREVIEW_FRAME) &&
            camHal->data_cb){
            ALOGD("%s: CAMERA_MSG_PREVIEW_FRAME data callback", __func__);
            camHal->data_cb(CAMERA_MSG_PREVIEW_FRAME, data, 0, NULL, camHal->cb_ctxt);
            ALOGD("%s: CAMERA_MSG_PREVIEW_FRAME data callback end", __func__);
        }
        if(camHal->h264_dev_name[0] == 0 && camHal->videorecordingEnableFlag && !mStoreMetaDataInBuffers){
            camHal->data_cb_timestamp(timestamp,CAMERA_MSG_VIDEO_FRAME, data, 0, camHal->cb_ctxt);
        }
        camHal->lock.lock();
        /*if(camHal->snapshotEnabledFlag && camHal->pictMem.camera_memory[0] != NULL){
            camHal->takePictInProgress = 0;
            camHal->snapshotEnabledFlag = 0;
            camHal->pictMem.camera_memory[0]->release(camHal->pictMem.camera_memory[0]);
            camHal->pictMem.camera_memory[0] = NULL;
        }*/

        camHal->returnFreePrevBuffer(data);
    }//while(1)
    ALOGD("%s: X", __func__);
    return (void *)0;
}


static int detectDeviceResolutions(     camera_hardware_t* camHal)
{
    int i = 0;
    v4l2_frmsizeenum fs;
    int w, h, area, maxArea = 0;
    String8 tmp;
    if(camHal == NULL || camHal->fd <= 0) {
        return -1;
    }
    ALOGI("%s yuv fd: %d, h264 fd: %d", __func__, camHal->fd, camHal->h264_fd);
    for(i = 0; ; i++) {
        memset(&fs, 0, sizeof(fs));
        fs.index = i;
        fs.pixel_format = camHal->previewFormat;
        if(ioctl(camHal->fd, VIDIOC_ENUM_FRAMESIZES, &fs) < 0) {
            break;
        }
        w = fs.discrete.width;
        h = fs.discrete.height;
        area = w * h;
        tmp = String8::format("%dx%d", w, h);
        ALOGI("%s camera yuv frame size: %d x %d", __func__, w, h);
        camHal->pictSizeValues += String8(camHal->pictSizeValues.size() ? "," : "") + tmp;
        camHal->pictSizes.push_back(Size(w, h));
        if(area <= (640*480)) {
            camHal->prevSizeValues += String8(camHal->prevSizeValues.size() ? "," : "") + tmp;
            camHal->prevSizes.push_back(Size(w, h));
            if(area > (camHal->prevWidth * camHal->prevHeight)) {
                camHal->prevSizeValue = tmp;
                camHal->prevWidth = w;
                camHal->prevHeight = h;
            }
        }
        if(area > maxArea) {
            maxArea = area;
            camHal->pictSizeValue = tmp;
            camHal->pictWidth = w;
            camHal->pictHeight = h;
        }
    }
    if(camHal->h264_fd <= 0) {
        camHal->vidSizeValues = camHal->prevSizeValues;
        camHal->vidSizeValue = camHal->prevSizeValue;
        camHal->videoWidth = camHal->prevWidth;
        camHal->videoHeight = camHal->prevHeight;
        camHal->videoSizes = camHal->prevSizes;
        return 0;
    }
    for(i = 0; ; i++) {
        memset(&fs, 0, sizeof(fs));
        fs.index = i;
        fs.pixel_format = camHal->recordingFormat;
        if(ioctl(camHal->h264_fd, VIDIOC_ENUM_FRAMESIZES, &fs) < 0) {
            break;
        }
        w = fs.discrete.width;
        h = fs.discrete.height;
        area = w * h;
        tmp = String8::format("%dx%d", w, h);
        ALOGI("%s camera h264 frame size: %d x %d", __func__, w, h);
        camHal->vidSizeValues += String8(camHal->vidSizeValues.size() ? "," : "") + tmp;
        camHal->videoSizes.push_back(Size(w, h));
        if(area > maxArea) {
            maxArea = area;
            camHal->vidSizeValue = tmp;
            camHal->videoWidth = w;
            camHal->videoHeight = h;
        }
    }
    return 0;
}

static int detect_uvc_device(camera_hardware_t *camHal)
{
    static char    temp_devname[FILENAME_LENGTH] = {0};
    int i = 0, j = 0, fd;
    bool detectYuv = false;
    bool detectH264 = false;

    ALOGD("%s: E", __func__);
    for(i = 0; i < 6; i++) {
        memset(temp_devname, 0, sizeof(temp_devname));
        sprintf(temp_devname, "/dev/video%d", i);
        fd = openloop(temp_devname, O_RDWR);
        if(fd < 0) {
            continue;
        }
        struct v4l2_fmtdesc fmtdesc;
        for(j = 0; ; j++) {
            memset(&fmtdesc, 0, sizeof(fmtdesc));
            fmtdesc.index = j;
            fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(ioctlLoop(fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
                break;
            }
            if(V4L2_PIX_FMT_YUYV == fmtdesc.pixelformat){
                detectYuv = true;
                camHal->previewFormat = V4L2_PIX_FMT_YUYV;
                strncpy(camHal->dev_name, temp_devname, FILENAME_LENGTH);
                ALOGI("%s: [%s] V4L2_PIX_FMT_YUYV is supported", __func__, temp_devname);
                break;
            }
            if(V4L2_PIX_FMT_H264 == fmtdesc.pixelformat){
                detectH264 = true;
                camHal->recordingFormat = V4L2_PIX_FMT_H264;
                strncpy(camHal->h264_dev_name, temp_devname, FILENAME_LENGTH);
                ALOGI("%s: [%s] V4L2_PIX_FMT_H264 is supported", __func__, temp_devname);
                break;
            }
        }
        close(fd);
        if(detectYuv && detectH264) {
            break;
        }
    }
    if(detectYuv && !detectH264) {
        camHal->recordingFormat = camHal->previewFormat;
    }
    ALOGD("%s: yuv device:[%s], h264 device:[%s] X", __func__, (detectYuv ? camHal->dev_name : "none"), (detectH264 ? camHal->h264_dev_name : "none"));
    return 0;
} /* get_uvc_device */

/******************************************************************************
 * Function: launchTakePictureThread
 * Description: This is a wrapper function to start take picture thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int launch_picture_thread(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&camHal->takePictureThread, &attr, takePictureThread, camHal);

    ALOGD("%s: X", __func__);
    return rc;
}
/******************************************************************************
 * Function: takePictureThread
 * Description: This function is associated with take picture thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static void * takePictureThread(void *hcamHal)
{
    int                 rc = 0;
    camera_hardware_t   *camHal     = NULL;

    camHal = (camera_hardware_t *)hcamHal;
    ALOGI("%s: E", __func__);

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL ;
    }

    Mutex::Autolock autoLock(camHal->lock);

    stopPreviewInternal(camHal);
    if(camHal->fd <= 0) {
        camHal->fd = openloop(camHal->dev_name, O_RDWR /* required */ | O_NONBLOCK);
        if (camHal->fd <  0) {
            ALOGE("%s: Cannot open '%s'", __func__, camHal->dev_name);
            return NULL;
        }
    }
    rc = initUsbCameraPreview(camHal, 1);
    if(rc < 0) {
        ALOGE("%s: Failed to intialize the device", __func__);
    }else{
        startUsbCamCapture(camHal);
    }
    /************************************************************************/
    /* - Time wait (select) on camera fd for camera frame availability      */
    /************************************************************************/
    {
        fd_set fds;
        struct timeval tv;
        int r = 0;

        FD_ZERO(&fds);
        FD_SET(camHal->fd, &fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        do{
            ALOGD("%s: b4 select on camHal->fd : %d", __func__, camHal->fd);
            r = select(camHal->fd + 1, &fds, NULL, NULL, &tv);
            ALOGD("%s: after select", __func__);
        }while((0 == r) || ((-1 == r) && (EINTR == errno)));

        if ((-1 == r) && (EINTR != errno)){
            ALOGE("%s: FDSelect ret = %d error: %d", __func__, r, errno);
            return NULL;
        }

    }
    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
    if (0 == get_buf_from_cam(camHal))
        ALOGD("%s: get_buf_from_cam success", __func__);
    else
        ALOGE("%s: get_buf_from_cam error", __func__);

    jpeg_encoder_task* task = createEncodeJpegTask(camHal->pictWidth, camHal->pictHeight, 80);
    rc = convert_YUYV_to_420_NV12(
        (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
        (char *)task->in_buffer.addr, camHal->pictWidth, camHal->pictHeight);
    add_time_water_marking_to_usbcameraframe((char*)task->in_buffer.addr, camHal->pictWidth,camHal->pictHeight,camHal->pictWidth);
    if(0 == put_buf_to_cam(camHal)) {
        ALOGD("%s: put_buf_to_cam success", __func__);
    }
    else
        ALOGE("%s: put_buf_to_cam error", __func__);

    /************************************************************************/
    /* - Free USB camera resources and close camera                         */
    /************************************************************************/

    camHal->takePictInProgress = 0;
    if(stopUsbCamCapture(camHal)){
        ALOGE("%s: Error in stopUsbCamCapture", __func__);
        rc = -1;
    }
    if(unInitV4L2mmap(camHal)){
        ALOGE("%s: Error in stopUsbCamCapture", __func__);
        rc = -1;
    }
    close(camHal->fd);
    camHal->fd = 0;
    encodeJpeg(task);
    camHal->pictMem.camera_memory[0] = camHal->get_memory(
            -1, task->output_size, 1, camHal->cb_ctxt);
    memcpy(camHal->pictMem.camera_memory[0]->data, task->out_buffer.addr, task->output_size);
    releaseEncodeJpegTask(task);


    /************************************************************************/
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /************************************************************************/
    if ((camHal->msgEnabledFlag & CAMERA_MSG_COMPRESSED_IMAGE) &&
            (camHal->data_cb)){
        /* Unlock temporarily, callback might call HAL api in turn */
        camHal->lock.unlock();
        camHal->data_cb(CAMERA_MSG_COMPRESSED_IMAGE,
                        camHal->pictMem.camera_memory[0],
                        0, NULL, camHal->cb_ctxt);
        camHal->lock.lock();
    }

    if(camHal->pictMem.camera_memory[0])
        camHal->pictMem.camera_memory[0]->release(
            camHal->pictMem.camera_memory[0]);

    /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
    ALOGI("%s: X", __func__);
    return (void *)0;
}

#if 0
/******************************************************************************
 * Function: allocate_ion_memory
 * Description: This function is allocates ION memory
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int allocate_ion_memory(QCameraHalMemInfo_t *mem_info, int ion_type)
{
    int                         rc = 0;
    struct ion_handle_data      handle_data;
    struct ion_allocation_data  alloc;
    struct ion_fd_data          ion_info_fd;
    int                         main_ion_fd = 0;

    main_ion_fd = open("/dev/ion", O_RDONLY);
    if (main_ion_fd <= 0) {
        ALOGE("Ion dev open failed %s\n", strerror(errno));
        goto ION_OPEN_FAILED;
    }

    memset(&alloc, 0, sizeof(alloc));
    alloc.len = mem_info->size;
    /* to make it page size aligned */
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ion_type;
    rc = ioctl(main_ion_fd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        ALOGE("ION allocation failed\n");
        goto ION_ALLOC_FAILED;
    }

    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;
    rc = ioctl(main_ion_fd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        ALOGE("ION map failed %s\n", strerror(errno));
        goto ION_MAP_FAILED;
    }

    mem_info->main_ion_fd = main_ion_fd;
    mem_info->fd = ion_info_fd.fd;
    mem_info->handle = ion_info_fd.handle;
    mem_info->size = alloc.len;
    return 0;

ION_MAP_FAILED:
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = ion_info_fd.handle;
    ioctl(main_ion_fd, ION_IOC_FREE, &handle_data);
ION_ALLOC_FAILED:
    close(main_ion_fd);
ION_OPEN_FAILED:
    return -1;
}

/******************************************************************************
 * Function: deallocate_ion_memory
 * Description: This function de allocates ION memory
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int deallocate_ion_memory(QCameraHalMemInfo_t *mem_info)
{
  struct ion_handle_data handle_data;
  int rc = 0;

  if (mem_info->fd > 0) {
      close(mem_info->fd);
      mem_info->fd = 0;
  }

  if (mem_info->main_ion_fd > 0) {
      memset(&handle_data, 0, sizeof(handle_data));
      handle_data.handle = mem_info->handle;
      ioctl(mem_info->main_ion_fd, ION_IOC_FREE, &handle_data);
      close(mem_info->main_ion_fd);
      mem_info->main_ion_fd = 0;
  }
  return rc;
}
#endif
/******************************************************************************/
}; // namespace android
