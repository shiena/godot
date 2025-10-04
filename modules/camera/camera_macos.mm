/**************************************************************************/
/*  camera_macos.mm                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

///@TODO this is a near duplicate of CameraIOS, we should find a way to combine those to minimize code duplication!!!!
// If you fix something here, make sure you fix it there as well!

#import "camera_macos.h"

#include "core/math/math_funcs.h"
#include "servers/camera/camera_feed.h"

#import <AVFoundation/AVFoundation.h>
#import <ImageIO/ImageIO.h>
#import <TargetConditionals.h>
#import <math.h>

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
#include "drivers/apple_embedded/display_server_apple_embedded.h"
#import <UIKit/UIKit.h>
#endif

//////////////////////////////////////////////////////////////////////////
// MyCaptureSession - This is a little helper class so we can capture our frames

@interface MyCaptureSession : AVCaptureSession <AVCaptureVideoDataOutputSampleBufferDelegate> {
	Ref<CameraFeed> feed;
	size_t width[2];
	size_t height[2];
	Vector<uint8_t> img_data[2];

	AVCaptureDeviceInput *input;
	AVCaptureVideoDataOutput *output;
#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
	float last_rotation;
	bool last_mirror;
	float base_scale_x;
	float base_scale_y;
	bool base_scale_initialized;
#endif
}

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
- (void)_updateFeedTransform:(CMSampleBufferRef)p_sampleBuffer connection:(AVCaptureConnection *)p_connection;
- (bool)_extractExifOrientation:(CFDictionaryRef)p_attachments rotation:(float *)r_rotation mirror:(bool *)r_mirror;
- (float)_normalizeDegrees:(float)p_degrees;
#endif

@end

@implementation MyCaptureSession

- (id)initForFeed:(Ref<CameraFeed>)p_feed andDevice:(AVCaptureDevice *)p_device {
	if (self = [super init]) {
		NSError *error;
		feed = p_feed;
		width[0] = 0;
		height[0] = 0;
		width[1] = 0;
		height[1] = 0;
#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
		last_rotation = NAN;
		last_mirror = false;
		base_scale_x = 0.0f;
		base_scale_y = 0.0f;
		base_scale_initialized = false;
#endif

		[self beginConfiguration];

		input = [AVCaptureDeviceInput deviceInputWithDevice:p_device error:&error];
		if (!input) {
			print_line("Couldn't get input device for camera");
		} else {
			[self addInput:input];
		}

		output = [AVCaptureVideoDataOutput new];
		if (!output) {
			print_line("Couldn't get output device for camera");
		} else {
			NSDictionary *settings = @{ (NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) };
			output.videoSettings = settings;

			// discard if the data output queue is blocked (as we process the still image)
			[output setAlwaysDiscardsLateVideoFrames:YES];

			// now set ourselves as the delegate to receive new frames.
			[output setSampleBufferDelegate:self queue:dispatch_get_main_queue()];

			// this takes ownership
			[self addOutput:output];
		}

		[self commitConfiguration];

		// kick off our session..
		[self startRunning];
	};
	return self;
}

- (void)cleanup {
	// stop running
	[self stopRunning];

	// cleanup
	[self beginConfiguration];

	// remove input
	if (input) {
		[self removeInput:input];
		// don't release this
		input = nullptr;
	}

	// free up our output
	if (output) {
		[self removeOutput:output];
		[output setSampleBufferDelegate:nil queue:nullptr];
		output = nullptr;
	}

	[self commitConfiguration];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
	// This gets called every time our camera has a new image for us to process.
	// May need to investigate in a way to throttle this if we get more images then we're rendering frames..

	// For now, version 1, we're just doing the bare minimum to make this work...
	CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
	// int _width = CVPixelBufferGetWidth(pixelBuffer);
	// int _height = CVPixelBufferGetHeight(pixelBuffer);

	// It says that we need to lock this on the documentation pages but it's not in the samples
	// need to lock our base address so we can access our pixel buffers, better safe then sorry?
	CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

	// get our buffers
	unsigned char *dataY = (unsigned char *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
	unsigned char *dataCbCr = (unsigned char *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
	if (dataY == nullptr) {
		print_line("Couldn't access Y pixel buffer data");
	} else if (dataCbCr == nullptr) {
		print_line("Couldn't access CbCr pixel buffer data");
	} else {
		Ref<Image> img[2];

		{
			// do Y
			size_t new_width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
			size_t new_height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);
			size_t row_stride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);

			if ((width[0] != new_width) || (height[0] != new_height)) {
				width[0] = new_width;
				height[0] = new_height;
				img_data[0].resize(new_width * new_height);
			}

			uint8_t *w = img_data[0].ptrw();
			if (new_width == row_stride) {
				memcpy(w, dataY, new_width * new_height);
			} else {
				for (size_t i = 0; i < new_height; i++) {
					memcpy(w, dataY, new_width);
					w += new_width;
					dataY += row_stride;
				}
			}

			img[0].instantiate();
			img[0]->set_data(new_width, new_height, 0, Image::FORMAT_R8, img_data[0]);
		}

		{
			// do CbCr
			size_t new_width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
			size_t new_height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 1);
			size_t row_stride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

			if ((width[1] != new_width) || (height[1] != new_height)) {
				width[1] = new_width;
				height[1] = new_height;
				img_data[1].resize(2 * new_width * new_height);
			}

			uint8_t *w = img_data[1].ptrw();
			if (new_width * 2 == row_stride) {
				memcpy(w, dataCbCr, 2 * new_width * new_height);
			} else {
				for (size_t i = 0; i < new_height; i++) {
					memcpy(w, dataCbCr, new_width * 2);
					w += new_width * 2;
					dataCbCr += row_stride;
				}
			}

			///TODO OpenGL doesn't support FORMAT_RG8, need to do some form of conversion
			img[1].instantiate();
			img[1]->set_data(new_width, new_height, 0, Image::FORMAT_RG8, img_data[1]);
		}

		// set our texture...
		feed->set_ycbcr_images(img[0], img[1]);
	}

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
	if (@available(iOS 11.0, *)) {
		[self _updateFeedTransform:sampleBuffer connection:connection];
	}
#endif

	// and unlock
	CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST

- (void)_updateFeedTransform:(CMSampleBufferRef)p_sampleBuffer connection:(AVCaptureConnection *)p_connection {
	if (!p_connection) {
		return;
	}

	float rotation_degrees = 0.0f;
	bool rotation_valid = false;
	bool mirror = false;

	// Get camera sensor orientation
	AVCaptureDevice *active_device = input ? input.device : nullptr;
	int sensor_orientation = 90; // Default for back camera
	if (active_device) {
		if (active_device.position == AVCaptureDevicePositionFront) {
			sensor_orientation = 270; // Front camera
		}
	}

#if TARGET_OS_IOS && !TARGET_OS_VISION
	// Get app interface orientation
	UIInterfaceOrientation interfaceOrientation = UIInterfaceOrientationPortrait;
	UIWindowScene *scene = (UIWindowScene *)[UIApplication sharedApplication].connectedScenes.allObjects.firstObject;
	if (scene && [scene isKindOfClass:[UIWindowScene class]]) {
		interfaceOrientation = scene.interfaceOrientation;
	}

	// Convert UIInterfaceOrientation to degrees
	int app_orientation = 0;
	switch (interfaceOrientation) {
		case UIInterfaceOrientationPortrait:
			app_orientation = 0;
			break;
		case UIInterfaceOrientationLandscapeRight:
			app_orientation = 90;
			break;
		case UIInterfaceOrientationPortraitUpsideDown:
			app_orientation = 180;
			break;
		case UIInterfaceOrientationLandscapeLeft:
			app_orientation = 270;
			break;
		default:
			app_orientation = 0;
			break;
	}

	// Calculate rotation angle: sensorOrientation - appOrientation
	// This matches the Android formula from android-camera-rotate-spec.md
	rotation_degrees = sensor_orientation - app_orientation;
	while (rotation_degrees < 0) {
		rotation_degrees += 360;
	}
	rotation_degrees = fmodf(rotation_degrees, 360.0f);
	rotation_valid = true;

	print_line(vformat("Camera: sensor=%d, app=%d, position=%d -> rotation=%f",
			sensor_orientation, app_orientation, (int)active_device.position, rotation_degrees));
#else
	// On macOS/visionOS, try to extract rotation from sample buffer
	CFDictionaryRef attachments = CMCopyDictionaryOfAttachments(nullptr, p_sampleBuffer, kCMAttachmentMode_ShouldPropagate);
	if (attachments) {
		rotation_valid = [self _extractExifOrientation:attachments rotation:&rotation_degrees mirror:&mirror];
		CFRelease(attachments);
	}

	if (!rotation_valid) {
		if (@available(iOS 17.0, *)) {
			if ([p_connection respondsToSelector:@selector(videoRotationAngle)]) {
				rotation_degrees = [self _normalizeDegrees:p_connection.videoRotationAngle];
				rotation_valid = true;
			}
		}
	}
#endif

	if (p_connection.isVideoMirroringSupported) {
		mirror = p_connection.videoMirrored;
	} else if (active_device && active_device.position == AVCaptureDevicePositionFront) {
		mirror = true;
	}

	if (!rotation_valid) {
		return;
	}

	float rotation_radians = Math::deg_to_rad(rotation_degrees);
	if (!Math::is_nan(last_rotation) && Math::is_equal_approx(rotation_radians, last_rotation) && mirror == last_mirror) {
		return;
	}

	Transform2D current_transform = feed->get_transform();
	if (!base_scale_initialized) {
		Vector2 initial_scale = current_transform.get_scale();
		base_scale_x = initial_scale.x;
		base_scale_y = initial_scale.y;
		base_scale_initialized = true;
	} else {
		Vector2 current_scale = current_transform.get_scale();
		float scale_x_magnitude = Math::abs(current_scale.x);
		float scale_y_magnitude = Math::abs(current_scale.y);
		base_scale_x = (base_scale_x < 0.0f) ? -scale_x_magnitude : scale_x_magnitude;
		base_scale_y = (base_scale_y < 0.0f) ? -scale_y_magnitude : scale_y_magnitude;
	}
	Vector2 final_scale(base_scale_x, base_scale_y);
	if (mirror) {
		final_scale.x = -final_scale.x;
	}

	Transform2D updated_transform;
	updated_transform.set_rotation(rotation_radians);
	updated_transform.set_scale(final_scale);
	updated_transform.set_origin(current_transform.get_origin());

	feed->set_transform(updated_transform);
	last_rotation = rotation_radians;
	last_mirror = mirror;
}

- (bool)_extractExifOrientation:(CFDictionaryRef)p_attachments rotation:(float *)r_rotation mirror:(bool *)r_mirror {
	CFNumberRef orientation_value = (CFNumberRef)CFDictionaryGetValue(p_attachments, kCGImagePropertyOrientation);
	if (!orientation_value) {
		return false;
	}

	int exif_orientation = 0;
	if (!CFNumberGetValue(orientation_value, kCFNumberIntType, &exif_orientation)) {
		return false;
	}

	switch (exif_orientation) {
		case 1:
			*r_rotation = 0.0f;
			*r_mirror = false;
			return true;
		case 2:
			*r_rotation = 0.0f;
			*r_mirror = true;
			return true;
		case 3:
			*r_rotation = 180.0f;
			*r_mirror = false;
			return true;
		case 4:
			*r_rotation = 180.0f;
			*r_mirror = true;
			return true;
		case 5:
			*r_rotation = 270.0f;
			*r_mirror = true;
			return true;
		case 6:
			*r_rotation = 90.0f;
			*r_mirror = false;
			return true;
		case 7:
			*r_rotation = 90.0f;
			*r_mirror = true;
			return true;
		case 8:
			*r_rotation = 270.0f;
			*r_mirror = false;
			return true;
		default:
			return false;
	}
}

- (float)_normalizeDegrees:(float)p_degrees {
	float normalized = fmodf(p_degrees, 360.0f);
	if (normalized < 0.0f) {
		normalized += 360.0f;
	}
	return normalized;
}

#endif

@end

//////////////////////////////////////////////////////////////////////////
// CameraFeedMacOS - Subclass for camera feeds in macOS

class CameraFeedMacOS : public CameraFeed {
	GDSOFTCLASS(CameraFeedMacOS, CameraFeed);

private:
	AVCaptureDevice *device;
	MyCaptureSession *capture_session;
	bool device_locked;

public:
	AVCaptureDevice *get_device() const;

	CameraFeedMacOS();

	void set_device(AVCaptureDevice *p_device);

	bool activate_feed() override;
	void deactivate_feed() override;

	bool set_format(int p_index, const Dictionary &p_parameters) override;
	Array get_formats() const override;
};

AVCaptureDevice *CameraFeedMacOS::get_device() const {
	return device;
}

CameraFeedMacOS::CameraFeedMacOS() {
	device = nullptr;
	capture_session = nullptr;
	device_locked = false;
}

void CameraFeedMacOS::set_device(AVCaptureDevice *p_device) {
	device = p_device;

	// get some info
	NSString *device_name = p_device.localizedName;
	name = String::utf8(device_name.UTF8String);
	position = CameraFeed::FEED_UNSPECIFIED;
	if ([p_device position] == AVCaptureDevicePositionBack) {
		position = CameraFeed::FEED_BACK;
	} else if ([p_device position] == AVCaptureDevicePositionFront) {
		position = CameraFeed::FEED_FRONT;
	};
}

bool CameraFeedMacOS::activate_feed() {
	if (capture_session) {
		// Already recording!
	} else {
		// Configure device format if specified.
		if (selected_format != -1) {
			NSError *error;
			if (!device_locked) {
				device_locked = [device lockForConfiguration:&error];
				ERR_FAIL_COND_V_MSG(!device_locked, false, error.localizedFailureReason.UTF8String);
			}
			[device setActiveFormat:device.formats[selected_format]];
		}
		// Start camera capture, check permission.
		if (@available(macOS 10.14, *)) {
			AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
			if (status == AVAuthorizationStatusAuthorized) {
				capture_session = [[MyCaptureSession alloc] initForFeed:this andDevice:device];
			} else if (status == AVAuthorizationStatusNotDetermined) {
				// Request permission.
				[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
										 completionHandler:^(BOOL granted) {
											 if (granted) {
												 capture_session = [[MyCaptureSession alloc] initForFeed:this andDevice:device];
											 }
										 }];
			}
		} else {
			capture_session = [[MyCaptureSession alloc] initForFeed:this andDevice:device];
		}
	};

	return true;
}

void CameraFeedMacOS::deactivate_feed() {
	// end camera capture if we have one
	if (capture_session) {
		[capture_session cleanup];
		capture_session = nullptr;
	};
	if (device_locked) {
		[device unlockForConfiguration];
		device_locked = false;
	}
}

bool CameraFeedMacOS::set_format(int p_index, const Dictionary &p_parameters) {
	if (p_index == -1) {
		selected_format = p_index;
		if (is_active()) {
			[capture_session beginConfiguration];
		}
		if (device_locked) {
			[device unlockForConfiguration];
			device_locked = false;
		}
		if (is_active()) {
			[capture_session commitConfiguration];
		}
		return true;
	}
	ERR_FAIL_INDEX_V((unsigned int)p_index, device.formats.count, false);
	if (is_active()) {
		if (!device_locked) {
			NSError *error;
			device_locked = [device lockForConfiguration:&error];
			ERR_FAIL_COND_V_MSG(!device_locked, false, error.localizedFailureReason.UTF8String);
		}
		[capture_session beginConfiguration];
		[device setActiveFormat:device.formats[p_index]];
	}
	selected_format = p_index;
	if (is_active()) {
		[capture_session commitConfiguration];
	}
	return true;
}

Array CameraFeedMacOS::get_formats() const {
	Array result;
	for (AVCaptureDeviceFormat *format in device.formats) {
		Dictionary dictionary;
		CMFormatDescriptionRef formatDescription = format.formatDescription;
		CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(formatDescription);
		dictionary["width"] = dimension.width;
		dictionary["height"] = dimension.height;
		FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(formatDescription);
		dictionary["format"] =
				String::chr((char)(fourcc >> 24) & 0xFF) +
				String::chr((char)(fourcc >> 16) & 0xFF) +
				String::chr((char)(fourcc >> 8) & 0xFF) +
				String::chr((char)(fourcc >> 0) & 0xFF);
		result.push_back(dictionary);
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////
// MyDeviceNotifications - This is a little helper class gets notifications
// when devices are connected/disconnected

@interface MyDeviceNotifications : NSObject {
	CameraMacOS *camera_server;
}

@end

@implementation MyDeviceNotifications

- (void)devices_changed:(NSNotification *)notification {
	camera_server->update_feeds();
}

- (id)initForServer:(CameraMacOS *)p_server {
	if (self = [super init]) {
		camera_server = p_server;

		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(devices_changed:) name:AVCaptureDeviceWasConnectedNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(devices_changed:) name:AVCaptureDeviceWasDisconnectedNotification object:nil];
	};
	return self;
}

- (void)dealloc {
	// remove notifications
	[[NSNotificationCenter defaultCenter] removeObserver:self name:AVCaptureDeviceWasConnectedNotification object:nil];
	[[NSNotificationCenter defaultCenter] removeObserver:self name:AVCaptureDeviceWasDisconnectedNotification object:nil];
}

@end

MyDeviceNotifications *device_notifications = nil;

//////////////////////////////////////////////////////////////////////////
// CameraMacOS - Subclass for our camera server on macOS

void CameraMacOS::update_feeds() {
	NSArray<AVCaptureDevice *> *devices = nullptr;
#if defined(__x86_64__)
	if (@available(macOS 10.15, *)) {
#endif
		AVCaptureDeviceDiscoverySession *session;
		if (@available(macOS 14.0, iOS 17.0, *)) {
			session = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:[NSArray arrayWithObjects:AVCaptureDeviceTypeExternal, AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeContinuityCamera, nil] mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified];
		} else {
#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
			session = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:[NSArray arrayWithObjects:AVCaptureDeviceTypeBuiltInWideAngleCamera, nil] mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified];
#else
		session = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:[NSArray arrayWithObjects:AVCaptureDeviceTypeExternalUnknown, AVCaptureDeviceTypeBuiltInWideAngleCamera, nil] mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionUnspecified];
#endif
		}
		devices = session.devices;
#if defined(__x86_64__)
	} else {
		devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
	}
#endif

	// remove devices that are gone..
	for (int i = feeds.size() - 1; i >= 0; i--) {
		Ref<CameraFeedMacOS> feed = (Ref<CameraFeedMacOS>)feeds[i];
		if (feed.is_null()) {
			continue;
		}

		if (![devices containsObject:feed->get_device()]) {
			// remove it from our array, this will also destroy it ;)
			remove_feed(feed);
		};
	};

	for (AVCaptureDevice *device in devices) {
		bool found = false;
		for (int i = 0; i < feeds.size() && !found; i++) {
			Ref<CameraFeedMacOS> feed = (Ref<CameraFeedMacOS>)feeds[i];
			if (feed.is_null()) {
				continue;
			}
			if (feed->get_device() == device) {
				found = true;
			};
		};

		if (!found) {
			Ref<CameraFeedMacOS> newfeed;
			newfeed.instantiate();
			newfeed->set_device(device);

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
			if (@available(iOS 11.0, *)) {
				// Keep default transform (vertical flip) for dynamic orientation handling.
			} else
#endif
			{
				// assume display camera so inverse when running on macOS
				Transform2D transform = Transform2D(-1.0, 0.0, 0.0, -1.0, 1.0, 1.0);
				newfeed->set_transform(transform);
			}

			add_feed(newfeed);
		};
	};
	emit_signal(SNAME(CameraServer::feeds_updated_signal_name));
}

void CameraMacOS::set_monitoring_feeds(bool p_monitoring_feeds) {
	if (p_monitoring_feeds == monitoring_feeds) {
		return;
	}

	CameraServer::set_monitoring_feeds(p_monitoring_feeds);
	if (p_monitoring_feeds) {
		// Find available cameras we have at this time.
		update_feeds();

		// Get notified on feed changes.
		device_notifications = [[MyDeviceNotifications alloc] initForServer:this];
	} else {
		// Stop monitoring feed changes.
		device_notifications = nil;
	}
}
