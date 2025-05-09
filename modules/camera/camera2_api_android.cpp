/**************************************************************************/
/*  camera2_api_android.cpp                                               */
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

#include "camera2_api_android.h"

#include "core/error/error_macros.h" // For ERR_PRINT, etc.
#include "core/os/os.h"              // For OS::get_singleton()
#include "core/string/print_string.h" // For print_verbose
#include "platform/android/os_android.h" // For OS_Android

#include <dlfcn.h> // For dlsym, dlclose

namespace Camera2API {

static void *library_handle = nullptr;
bool library_loaded = false; // Needs to be non-static to be extern in header

// Define function pointer variables
ACameraManager_create_func ACameraManager_create_ptr = nullptr;
ACameraManager_delete_func ACameraManager_delete_ptr = nullptr;
ACameraManager_getCameraIdList_func ACameraManager_getCameraIdList_ptr = nullptr;
ACameraManager_deleteCameraIdList_func ACameraManager_deleteCameraIdList_ptr = nullptr;
ACameraManager_getCameraCharacteristics_func ACameraManager_getCameraCharacteristics_ptr = nullptr;
ACameraManager_openCamera_func ACameraManager_openCamera_ptr = nullptr;

ACameraDevice_close_func ACameraDevice_close_ptr = nullptr;
ACameraDevice_createCaptureRequest_func ACameraDevice_createCaptureRequest_ptr = nullptr;
ACameraDevice_createCaptureSession_func ACameraDevice_createCaptureSession_ptr = nullptr;

ACameraMetadata_getConstEntry_func ACameraMetadata_getConstEntry_ptr = nullptr;
ACameraMetadata_free_func ACameraMetadata_free_ptr = nullptr;

ACaptureRequest_addTarget_func ACaptureRequest_addTarget_ptr = nullptr;
ACaptureRequest_free_func ACaptureRequest_free_ptr = nullptr;

ACaptureSessionOutput_create_func ACaptureSessionOutput_create_ptr = nullptr;
ACaptureSessionOutputContainer_create_func ACaptureSessionOutputContainer_create_ptr = nullptr;
ACaptureSessionOutputContainer_add_func ACaptureSessionOutputContainer_add_ptr = nullptr;

ACameraCaptureSession_setRepeatingRequest_func ACameraCaptureSession_setRepeatingRequest_ptr = nullptr;
ACameraCaptureSession_stopRepeating_func ACameraCaptureSession_stopRepeating_ptr = nullptr;
ACameraCaptureSession_close_func ACameraCaptureSession_close_ptr = nullptr;

ACameraOutputTarget_create_func ACameraOutputTarget_create_ptr = nullptr;

AImageReader_new_func AImageReader_new_ptr = nullptr;
AImageReader_setImageListener_func AImageReader_setImageListener_ptr = nullptr;
AImageReader_getWindow_func AImageReader_getWindow_ptr = nullptr;
AImageReader_acquireNextImage_func AImageReader_acquireNextImage_ptr = nullptr;
AImageReader_delete_func AImageReader_delete_ptr = nullptr;

AImage_getPlaneData_func AImage_getPlaneData_ptr = nullptr;
AImage_getPlanePixelStride_func AImage_getPlanePixelStride_ptr = nullptr;
AImage_getPlaneRowStride_func AImage_getPlaneRowStride_ptr = nullptr;
AImage_delete_func AImage_delete_ptr = nullptr;

#define LOAD_SYMBOL(func_name)                                                           \
	func_name##_ptr = (func_name##_func)dlsym(library_handle, #func_name);               \
	if (!func_name##_ptr) {                                                              \
		ERR_PRINT("Failed to load symbol: " #func_name);                                 \
		dlclose(library_handle);                                                         \
		library_handle = nullptr;                                                        \
		return false;                                                                    \
	}

bool load_library() {
	if (library_loaded) {
		return true;
	}

	Error err = OS_Android::get_singleton()->open_dynamic_library("libcamera2ndk.so", library_handle);
	if (err != OK || !library_handle) {
		ERR_PRINT(vformat("Failed to open libcamera2ndk.so: %d", err));
		library_handle = nullptr; // Ensure it's null if open_dynamic_library failed but set it.
		return false;
	}

	LOAD_SYMBOL(ACameraManager_create);
	LOAD_SYMBOL(ACameraManager_delete);
	LOAD_SYMBOL(ACameraManager_getCameraIdList);
	LOAD_SYMBOL(ACameraManager_deleteCameraIdList);
	LOAD_SYMBOL(ACameraManager_getCameraCharacteristics);
	LOAD_SYMBOL(ACameraManager_openCamera);

	LOAD_SYMBOL(ACameraDevice_close);
	LOAD_SYMBOL(ACameraDevice_createCaptureRequest);
	LOAD_SYMBOL(ACameraDevice_createCaptureSession);

	LOAD_SYMBOL(ACameraMetadata_getConstEntry);
	LOAD_SYMBOL(ACameraMetadata_free);

	LOAD_SYMBOL(ACaptureRequest_addTarget);
	LOAD_SYMBOL(ACaptureRequest_free);

	LOAD_SYMBOL(ACaptureSessionOutput_create);
	LOAD_SYMBOL(ACaptureSessionOutputContainer_create);
	LOAD_SYMBOL(ACaptureSessionOutputContainer_add);

	LOAD_SYMBOL(ACameraCaptureSession_setRepeatingRequest);
	LOAD_SYMBOL(ACameraCaptureSession_stopRepeating);
	LOAD_SYMBOL(ACameraCaptureSession_close);

	LOAD_SYMBOL(ACameraOutputTarget_create);

	LOAD_SYMBOL(AImageReader_new);
	LOAD_SYMBOL(AImageReader_setImageListener);
	LOAD_SYMBOL(AImageReader_getWindow);
	LOAD_SYMBOL(AImageReader_acquireNextImage);
	LOAD_SYMBOL(AImageReader_delete);

	LOAD_SYMBOL(AImage_getPlaneData);
	LOAD_SYMBOL(AImage_getPlanePixelStride);
	LOAD_SYMBOL(AImage_getPlaneRowStride);
	LOAD_SYMBOL(AImage_delete);

	library_loaded = true;
	print_verbose("libcamera2ndk.so loaded successfully.");
	return true;
}

void unload_library() {
	if (library_handle) {
		dlclose(library_handle);
		library_handle = nullptr;
	}

	// Nullify all function pointers
	ACameraManager_create_ptr = nullptr;
	ACameraManager_delete_ptr = nullptr;
	ACameraManager_getCameraIdList_ptr = nullptr;
	ACameraManager_deleteCameraIdList_ptr = nullptr;
	ACameraManager_getCameraCharacteristics_ptr = nullptr;
	ACameraManager_openCamera_ptr = nullptr;
	ACameraDevice_close_ptr = nullptr;
	ACameraDevice_createCaptureRequest_ptr = nullptr;
	ACameraDevice_createCaptureSession_ptr = nullptr;
	ACameraMetadata_getConstEntry_ptr = nullptr;
	ACameraMetadata_free_ptr = nullptr;
	ACaptureRequest_addTarget_ptr = nullptr;
	ACaptureRequest_free_ptr = nullptr;
	ACaptureSessionOutput_create_ptr = nullptr;
	ACaptureSessionOutputContainer_create_ptr = nullptr;
	ACaptureSessionOutputContainer_add_ptr = nullptr;
	ACameraCaptureSession_setRepeatingRequest_ptr = nullptr;
	ACameraCaptureSession_stopRepeating_ptr = nullptr;
	ACameraCaptureSession_close_ptr = nullptr;
	ACameraOutputTarget_create_ptr = nullptr;
	AImageReader_new_ptr = nullptr;
	AImageReader_setImageListener_ptr = nullptr;
	AImageReader_getWindow_ptr = nullptr;
	AImageReader_acquireNextImage_ptr = nullptr;
	AImageReader_delete_ptr = nullptr;
	AImage_getPlaneData_ptr = nullptr;
	AImage_getPlanePixelStride_ptr = nullptr;
	AImage_getPlaneRowStride_ptr = nullptr;
	AImage_delete_ptr = nullptr;

	library_loaded = false;
	print_verbose("libcamera2ndk.so unloaded.");
}

#undef LOAD_SYMBOL

} // namespace Camera2API