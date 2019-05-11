/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <unistd.h>  // NOLINT(build/include_order)

#include "tensorflow/contrib/lite/examples/label_image/bitmap_helpers.h"

#define LOG(x) std::cerr

namespace tflite {
namespace label_image {

std::vector<uint8_t> decode_bmp(const uint8_t* input, int row_size, int width,
		int height, int channels, bool top_down)
{
	std::vector<uint8_t> output(height * width * channels);
	int i, j;

	for (i = 0; i < height; i++) {
		int src_pos;
		int dst_pos;

		for (j = 0; j < width; j++) {
			if (!top_down) {
				src_pos = ((height - 1 - i) * row_size) + j * channels;
			} else {
				src_pos = i * row_size + j * channels;
			}

			dst_pos = (i * width + j) * channels;

			switch (channels) {
			case 1:
				output[dst_pos] = input[src_pos];
			break;
			case 3:
				// BGR -> RGB
				output[dst_pos] = input[src_pos + 2];
				output[dst_pos + 1] = input[src_pos + 1];
				output[dst_pos + 2] = input[src_pos];
			break;
			case 4:
				// BGRA -> RGBA
				output[dst_pos] = input[src_pos + 2];
				output[dst_pos + 1] = input[src_pos + 1];
				output[dst_pos + 2] = input[src_pos];
				output[dst_pos + 3] = input[src_pos + 3];
			break;
			default:
				LOG(FATAL) << "Unexpected number of channels: " << channels;
			break;
			}
		}
	}
	return output;
}

#ifndef TFLITE_MCU

std::vector<uint8_t> read_bmp(const std::string& input_bmp_name, int* width,
                              int* height, int* channels, Settings* s) {
  int begin, end;

  std::ifstream file(input_bmp_name, std::ios::in | std::ios::binary);
  if (!file) {
    LOG(FATAL) << "input file " << input_bmp_name << " not found\n";
    exit(-1);
  }

  begin = file.tellg();
  file.seekg(0, std::ios::end);
  end = file.tellg();
  size_t len = end - begin;

  if (s->verbose) LOG(INFO) << "len: " << len << "\n";

  std::vector<uint8_t> img_bytes(len);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char*>(img_bytes.data()), len);
  const int32_t header_size =
      *(reinterpret_cast<const int32_t*>(img_bytes.data() + 10));
  *width = *(reinterpret_cast<const int32_t*>(img_bytes.data() + 18));
  *height = *(reinterpret_cast<const int32_t*>(img_bytes.data() + 22));
  const int32_t bpp =
      *(reinterpret_cast<const int32_t*>(img_bytes.data() + 28));
  *channels = bpp / 8;

  if (s->verbose)
    LOG(INFO) << "width, height, channels: " << *width << ", " << *height
              << ", " << *channels << "\n";

  // there may be padding bytes when the width is not a multiple of 4 bytes
  // 8 * channels == bits per pixel
  const int row_size = (8 * *channels * *width + 31) / 32 * 4;

  // if height is negative, data layout is top down
  // otherwise, it's bottom up
  bool top_down = (*height < 0);

  // Decode image, allocating tensor once the image size is known
  const uint8_t* bmp_pixels = &img_bytes[header_size];
  return decode_bmp(bmp_pixels, row_size, *width, abs(*height), *channels,
                    top_down);
}
#else

#include "autoconf.h"
#include "ff.h"
std::vector<uint8_t> read_bmp(const std::string& input_bmp_name, int* width,
                              int* height, int* channels, Settings* s)
{
	int begin, end, row_size;
	char *file_name = (char *)input_bmp_name.c_str();
	FIL fil;
	unsigned int len, read_len;
	FRESULT rc = f_open(&fil, &file_name[1], FA_READ);
	std::vector<uint8_t> err_ret(1);
	int32_t header_size, bpp;
	bool top_down;
	uint8_t* bmp_pixels;
	FILINFO finfo;

	err_ret[0] = 0;
	if (rc != FR_OK) {
		LOG(FATAL) << "input file " << file_name << "rc:" << rc << " ,not found\n";
		return err_ret;
	}

	rc = f_stat(&file_name[1], &finfo);
	len = finfo.fsize;

	bmp_pixels = new uint8_t[len];
	if (!bmp_pixels)
		return err_ret;

	if (s->verbose)
		LOG(INFO) << "len: " << len << "\n";

	rc = f_read(&fil, bmp_pixels, len, &read_len);
	if (read_len != len)
		return err_ret;

	header_size = *((int32_t *)(bmp_pixels + 10));
	*width = *((int *)(bmp_pixels + 18));
	*height = *((int *)(bmp_pixels + 22));
	bpp = *((int32_t *)(bmp_pixels + 28));
	*channels = bpp / 8;

	if (s->verbose)
		LOG(INFO) << "width, height, channels: " << *width << ", " << *height
			<< ", " << *channels << "\n";

	row_size = (8 * (*channels) * (*width) + 31) / 32 * 4;

	top_down = ((*height) < 0);

	std::vector<uint8_t> decode_ret = decode_bmp(bmp_pixels + header_size, row_size, *width, abs(*height), *channels,
		top_down);
	delete bmp_pixels;
	return decode_ret;
}

#endif
}  // namespace label_image
}  // namespace tflite
