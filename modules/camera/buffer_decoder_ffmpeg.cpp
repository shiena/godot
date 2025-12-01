/**************************************************************************/
/*  buffer_decoder_ffmpeg.cpp                                             */
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

#include "buffer_decoder_ffmpeg.h"

#ifdef LINUXBSD_ENABLED
#ifdef FFMPEG_ENABLED

#include "servers/camera/camera_feed.h"

#include <cstring>

#ifdef SOWRAP_ENABLED
#include "drivers/ffmpeg/avcodec-so_wrap.h"
#include "drivers/ffmpeg/avutil-so_wrap.h"
#include "drivers/ffmpeg/swscale-so_wrap.h"
#else
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

FFmpegBufferDecoder::FFmpegBufferDecoder(CameraFeed *p_camera_feed, int p_codec_id) :
		BufferDecoder(p_camera_feed) {
	initialized = _init_codec(p_codec_id);
	if (!initialized) {
		ERR_PRINT("FFmpeg: Failed to initialize codec");
	}
}

FFmpegBufferDecoder::~FFmpegBufferDecoder() {
	_cleanup();
}

bool FFmpegBufferDecoder::_init_codec(int p_codec_id) {
	// 1. Find decoder
	codec = avcodec_find_decoder((enum AVCodecID)p_codec_id);
	if (!codec) {
		ERR_PRINT("FFmpeg: Codec not found");
		return false;
	}

	// 2. Allocate codec context
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		ERR_PRINT("FFmpeg: Could not allocate codec context");
		return false;
	}

	// 3. Set codec parameters
	codec_ctx->width = width;
	codec_ctx->height = height;

	// 4. Open codec
	if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
		ERR_PRINT("FFmpeg: Could not open codec");
		avcodec_free_context(&codec_ctx);
		codec_ctx = nullptr;
		return false;
	}

	// 5. Allocate packet and frames
	packet = av_packet_alloc();
	frame = av_frame_alloc();
	rgb_frame = av_frame_alloc();

	if (!packet || !frame || !rgb_frame) {
		ERR_PRINT("FFmpeg: Could not allocate packet/frames");
		_cleanup();
		return false;
	}

	// 6. Allocate RGB frame buffer
	int rgb_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
	if (rgb_size < 0) {
		ERR_PRINT("FFmpeg: Could not get buffer size");
		_cleanup();
		return false;
	}
	image_data.resize(rgb_size);

	av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize,
			image_data.ptrw(), AV_PIX_FMT_RGB24, width, height, 1);

	return true;
}

void FFmpegBufferDecoder::decode(StreamingBuffer p_buffer) {
	if (!initialized || !codec_ctx) {
		return;
	}

	// Prepare packet with compressed data
	packet->data = static_cast<uint8_t *>(p_buffer.start);
	packet->size = p_buffer.length;

	// Send packet to decoder
	int ret = avcodec_send_packet(codec_ctx, packet);
	if (ret < 0) {
		// Don't print error for EAGAIN, just skip
		if (ret != AVERROR(EAGAIN)) {
			ERR_PRINT("FFmpeg: Error sending packet to decoder");
		}
		return;
	}

	// Receive decoded frame
	ret = avcodec_receive_frame(codec_ctx, frame);
	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
		return; // Need more data or end of stream
	} else if (ret < 0) {
		ERR_PRINT("FFmpeg: Error receiving frame from decoder");
		return;
	}

	// Skip incomplete/corrupt frames
	if (frame->decode_error_flags != 0) {
		print_verbose("FFmpeg: Skipping corrupt frame");
		av_frame_unref(frame);
		return;
	}

	// Check if frame data pointers are valid
	if (!frame->data[0] || !frame->data[1] || !frame->data[2]) {
		print_verbose("FFmpeg: Frame data is null");
		av_frame_unref(frame);
		return;
	}

	// Use decoded frame dimensions for output
	int out_width = frame->width;
	int out_height = frame->height;

	// Debug: print frame info on first frame
	if (!sws_ctx) {
		print_verbose(vformat("FFmpeg: Decoded frame %dx%d, format %d, expected %dx%d",
				out_width, out_height, frame->format, width, height));
		print_verbose(vformat("FFmpeg: Input linesize[0]=%d, [1]=%d, [2]=%d",
				frame->linesize[0], frame->linesize[1], frame->linesize[2]));
	}

	// Reinitialize if frame dimensions changed
	if (!sws_ctx || out_width != sws_src_width || out_height != sws_src_height) {
		if (sws_ctx) {
			sws_freeContext(sws_ctx);
			sws_ctx = nullptr;
		}

		// Reallocate RGB buffer for new dimensions
		int rgb_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, out_width, out_height, 1);
		if (rgb_size < 0) {
			ERR_PRINT("FFmpeg: Could not get buffer size");
			av_frame_unref(frame);
			return;
		}
		image_data.resize(rgb_size);
		memset(image_data.ptrw(), 0, rgb_size);

		av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize,
				image_data.ptrw(), AV_PIX_FMT_RGB24, out_width, out_height, 1);

		sws_ctx = sws_getContext(
				out_width, out_height, (enum AVPixelFormat)frame->format,
				out_width, out_height, AV_PIX_FMT_RGB24,
				SWS_BILINEAR, nullptr, nullptr, nullptr);

		if (!sws_ctx) {
			ERR_PRINT("FFmpeg: Could not initialize swscale context");
			av_frame_unref(frame);
			return;
		}

		sws_src_width = out_width;
		sws_src_height = out_height;

		print_verbose(vformat("FFmpeg: Output linesize=%d, expected=%d, buffer_size=%d",
				rgb_frame->linesize[0], out_width * 3, rgb_size));
	}

	// Convert to RGB
	int scaled_height = sws_scale(sws_ctx,
			frame->data, frame->linesize, 0, out_height,
			rgb_frame->data, rgb_frame->linesize);

	// Debug: check if sws_scale processed all lines
	static bool scale_checked = false;
	if (!scale_checked) {
		print_verbose(vformat("FFmpeg: sws_scale returned %d, expected %d", scaled_height, out_height));
		scale_checked = true;
	}

	// Update Godot image
	if (image.is_valid()) {
		image->set_data(out_width, out_height, false, Image::FORMAT_RGB8, image_data);
	} else {
		image.instantiate(out_width, out_height, false, Image::FORMAT_RGB8, image_data);
	}

	// Send to camera feed
	camera_feed->set_rgb_image(image);

	// Unref frame for next decode
	av_frame_unref(frame);
}

void FFmpegBufferDecoder::_cleanup() {
	if (sws_ctx) {
		sws_freeContext(sws_ctx);
		sws_ctx = nullptr;
	}
	if (rgb_frame) {
		av_frame_free(&rgb_frame);
		rgb_frame = nullptr;
	}
	if (frame) {
		av_frame_free(&frame);
		frame = nullptr;
	}
	if (packet) {
		av_packet_free(&packet);
		packet = nullptr;
	}
	if (codec_ctx) {
		avcodec_free_context(&codec_ctx);
		codec_ctx = nullptr;
	}
	initialized = false;
}

// MJPEG decoder
FFmpegMjpegBufferDecoder::FFmpegMjpegBufferDecoder(CameraFeed *p_camera_feed) :
		FFmpegBufferDecoder(p_camera_feed, AV_CODEC_ID_MJPEG) {
}

// H.264 decoder
FFmpegH264BufferDecoder::FFmpegH264BufferDecoder(CameraFeed *p_camera_feed) :
		FFmpegBufferDecoder(p_camera_feed, AV_CODEC_ID_H264) {
}

#endif // FFMPEG_ENABLED
#endif // LINUXBSD_ENABLED
