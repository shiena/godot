/**************************************************************************/
/*  camera_feed.h                                                         */
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

#pragma once

#include "core/io/image.h"
#include "core/math/transform_2d.h"
#include "servers/camera_server.h"
#include "servers/rendering_server.h"

/**
	The camera server is a singleton object that gives access to the various
	camera feeds that can be used as the background for our environment.
**/

class CameraFeed : public RefCounted {
	GDCLASS(CameraFeed, RefCounted);

public:
	enum FeedDataType {
		FEED_NOIMAGE, // we don't have an image yet
		FEED_RGB, // TEXTURE contains RGB data
		FEED_YCBCR, // TEXTURE contains YCbCr data
		FEED_YCBCR_SEP, // TEXTURE contains Y data, NORMAL_TEXTURE contains Cb data, SPECULAR_TEXTURE contains Cr data
		FEED_EXTERNAL, // specific for android atm, camera feed is managed externally, assumed RGB for now
		FEED_UNSUPPORTED, // unsupported type
		FEED_RGBA, // TEXTURE contains RGBA data
		FEED_NV12 // TEXTURE contains Y data, NORMAL_TEXTURE contains CbCr data
	};

	enum FeedPosition {
		FEED_UNSPECIFIED, // we have no idea
		FEED_FRONT, // this is a camera on the front of the device
		FEED_BACK // this is a camera on the back of the device
	};

private:
	int id; // unique id for this, for internal use in case feeds are removed

	RID texture; // layered texture
	RID channel_texture[3]; // channel textures
	Ref<Image> channel_image[3]; // channel images

protected:
	String name; // name of our camera feed
	FeedDataType datatype; // type of texture data stored
	FeedPosition position; // position of camera on the device
	int width; // width of camera frames
	int height; // height of camera frames

	Transform2D transform; // display transform

	bool active; // only when active do we actually update the camera texture each frame

	static void _bind_methods();

public:
	int get_id() const;
	String get_name() const;
	int get_width() const;
	int get_height() const;
	FeedPosition get_position() const;
	FeedDataType get_datatype() const;

	RID get_texture() const;

	bool is_active() const;
	void set_active(bool p_is_active);

	Transform2D get_transform() const;
	void set_transform(const Transform2D &p_transform);

	Ref<Image> get_image(RenderingServer::CanvasTextureChannel channel);
	void set_image(RenderingServer::CanvasTextureChannel channel, const Ref<Image> &image);
	void set_image(RenderingServer::CanvasTextureChannel channel, uint8_t *data, size_t offset, size_t len);

	CameraFeed();
	virtual ~CameraFeed();

	virtual bool activate_feed();
	virtual void deactivate_feed();

	GDVIRTUAL0R(bool, _activate_feed)
	GDVIRTUAL0(_deactivate_feed)
};

VARIANT_ENUM_CAST(CameraFeed::FeedPosition);
VARIANT_ENUM_CAST(CameraFeed::FeedDataType);
