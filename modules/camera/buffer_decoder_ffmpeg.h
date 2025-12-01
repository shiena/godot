/**************************************************************************/
/*  buffer_decoder_ffmpeg.h                                               */
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

#ifdef LINUXBSD_ENABLED
#ifdef FFMPEG_ENABLED

#include "buffer_decoder.h"

// Forward declarations for FFmpeg types
struct AVCodec;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct SwsContext;

class FFmpegBufferDecoder : public BufferDecoder {
protected:
	const AVCodec *codec = nullptr;
	AVCodecContext *codec_ctx = nullptr;
	AVPacket *packet = nullptr;
	AVFrame *frame = nullptr;
	AVFrame *rgb_frame = nullptr;
	SwsContext *sws_ctx = nullptr;
	int sws_src_width = 0;
	int sws_src_height = 0;
	Vector<uint8_t> image_data;
	bool initialized = false;

	bool _init_codec(int p_codec_id);
	void _cleanup();

public:
	FFmpegBufferDecoder(CameraFeed *p_camera_feed, int p_codec_id);
	virtual ~FFmpegBufferDecoder();
	virtual void decode(StreamingBuffer p_buffer) override;
};

class FFmpegMjpegBufferDecoder : public FFmpegBufferDecoder {
public:
	FFmpegMjpegBufferDecoder(CameraFeed *p_camera_feed);
};

class FFmpegH264BufferDecoder : public FFmpegBufferDecoder {
public:
	FFmpegH264BufferDecoder(CameraFeed *p_camera_feed);
};

#endif // FFMPEG_ENABLED
#endif // LINUXBSD_ENABLED
