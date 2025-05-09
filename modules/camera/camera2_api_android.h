/**************************************************************************/
/*  camera2_api_android.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// NDK Camera and Media headers for type definitions
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImageReader.h>

namespace Camera2API {

// Function pointer types - these are just for declaration here
typedef ACameraManager* (*ACameraManager_create_func)();
typedef camera_status_t (*ACameraManager_delete_func)(ACameraManager *manager);
typedef camera_status_t (*ACameraManager_getCameraIdList_func)(ACameraManager *manager, ACameraIdList **cameraIdList);
typedef void (*ACameraManager_deleteCameraIdList_func)(ACameraIdList *cameraIdList);
typedef camera_status_t (*ACameraManager_getCameraCharacteristics_func)(ACameraManager *manager, const char *cameraId, ACameraMetadata **characteristics);
typedef camera_status_t (*ACameraManager_openCamera_func)(ACameraManager *manager, const char *cameraId, ACameraDevice_StateCallbacks *callbacks, ACameraDevice **device);

typedef camera_status_t (*ACameraDevice_close_func)(ACameraDevice *device);
typedef camera_status_t (*ACameraDevice_createCaptureRequest_func)(const ACameraDevice *device, ACameraDevice_request_template templateId, ACaptureRequest **request);
typedef camera_status_t (*ACameraDevice_createCaptureSession_func)(ACameraDevice *device, const ACaptureSessionOutputContainer *outputs, const ACameraCaptureSession_stateCallbacks *callbacks, ACameraCaptureSession **session);

typedef camera_status_t (*ACameraMetadata_getConstEntry_func)(const ACameraMetadata *metadata, uint32_t tag, ACameraMetadata_const_entry *entry);
typedef void (*ACameraMetadata_free_func)(ACameraMetadata *metadata);

typedef camera_status_t (*ACaptureRequest_addTarget_func)(ACaptureRequest *request, const ACameraOutputTarget *output);
typedef void (*ACaptureRequest_free_func)(ACaptureRequest *request);

typedef camera_status_t (*ACaptureSessionOutput_create_func)(ANativeWindow *window, ACaptureSessionOutput **output);
typedef camera_status_t (*ACaptureSessionOutputContainer_create_func)(ACaptureSessionOutputContainer **container);
typedef camera_status_t (*ACaptureSessionOutputContainer_add_func)(ACaptureSessionOutputContainer *container, const ACaptureSessionOutput *output);

typedef camera_status_t (*ACameraCaptureSession_setRepeatingRequest_func)(ACameraCaptureSession *session, ACameraCaptureSession_captureCallbacks *callbacks, int numRequests, ACaptureRequest **requests, int *captureSequenceId);
typedef camera_status_t (*ACameraCaptureSession_stopRepeating_func)(ACameraCaptureSession *session);
typedef void (*ACameraCaptureSession_close_func)(ACameraCaptureSession *session);

typedef camera_status_t (*ACameraOutputTarget_create_func)(ANativeWindow *window, ACameraOutputTarget **target);

typedef media_status_t (*AImageReader_new_func)(int32_t width, int32_t height, int32_t format, int32_t maxImages, AImageReader **reader);
typedef media_status_t (*AImageReader_setImageListener_func)(AImageReader *reader, AImageReader_ImageListener *listener);
typedef media_status_t (*AImageReader_getWindow_func)(AImageReader *reader, ANativeWindow **window);
typedef media_status_t (*AImageReader_acquireNextImage_func)(AImageReader *reader, AImage **image);
typedef void (*AImageReader_delete_func)(AImageReader *reader);

typedef media_status_t (*AImage_getPlaneData_func)(const AImage *image, int planeIdx, uint8_t **data, int *dataLength);
typedef media_status_t (*AImage_getPlanePixelStride_func)(const AImage *image, int planeIdx, int32_t *pixelStride);
typedef media_status_t (*AImage_getPlaneRowStride_func)(const AImage *image, int planeIdx, int32_t *rowStride);
typedef void (*AImage_delete_func)(AImage *image);

// Declare function pointers as extern
extern ACameraManager_create_func ACameraManager_create_ptr;
extern ACameraManager_delete_func ACameraManager_delete_ptr;
extern ACameraManager_getCameraIdList_func ACameraManager_getCameraIdList_ptr;
extern ACameraManager_deleteCameraIdList_func ACameraManager_deleteCameraIdList_ptr;
extern ACameraManager_getCameraCharacteristics_func ACameraManager_getCameraCharacteristics_ptr;
extern ACameraManager_openCamera_func ACameraManager_openCamera_ptr;

extern ACameraDevice_close_func ACameraDevice_close_ptr;
extern ACameraDevice_createCaptureRequest_func ACameraDevice_createCaptureRequest_ptr;
extern ACameraDevice_createCaptureSession_func ACameraDevice_createCaptureSession_ptr;

extern ACameraMetadata_getConstEntry_func ACameraMetadata_getConstEntry_ptr;
extern ACameraMetadata_free_func ACameraMetadata_free_ptr;

extern ACaptureRequest_addTarget_func ACaptureRequest_addTarget_ptr;
extern ACaptureRequest_free_func ACaptureRequest_free_ptr;

extern ACaptureSessionOutput_create_func ACaptureSessionOutput_create_ptr;
extern ACaptureSessionOutputContainer_create_func ACaptureSessionOutputContainer_create_ptr;
extern ACaptureSessionOutputContainer_add_func ACaptureSessionOutputContainer_add_ptr;

extern ACameraCaptureSession_setRepeatingRequest_func ACameraCaptureSession_setRepeatingRequest_ptr;
extern ACameraCaptureSession_stopRepeating_func ACameraCaptureSession_stopRepeating_ptr;
extern ACameraCaptureSession_close_func ACameraCaptureSession_close_ptr;

extern ACameraOutputTarget_create_func ACameraOutputTarget_create_ptr;

extern AImageReader_new_func AImageReader_new_ptr;
extern AImageReader_setImageListener_func AImageReader_setImageListener_ptr;
extern AImageReader_getWindow_func AImageReader_getWindow_ptr;
extern AImageReader_acquireNextImage_func AImageReader_acquireNextImage_ptr;
extern AImageReader_delete_func AImageReader_delete_ptr;

extern AImage_getPlaneData_func AImage_getPlaneData_ptr;
extern AImage_getPlanePixelStride_func AImage_getPlanePixelStride_ptr;
extern AImage_getPlaneRowStride_func AImage_getPlaneRowStride_ptr;
extern AImage_delete_func AImage_delete_ptr;

extern bool library_loaded;

// Declare load/unload functions
bool load_library();
void unload_library();

} // namespace Camera2API
