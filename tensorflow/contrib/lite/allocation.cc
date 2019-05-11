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
#include <iostream>
#include <string>
#include <vector>

#include "tensorflow/contrib/lite/allocation.h"

#ifndef TFLITE_MCU
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <utility>

#include "tensorflow/contrib/lite/context.h"
#include "tensorflow/contrib/lite/error_reporter.h"

namespace tflite {

#ifndef TFLITE_MCU
FileCopyAllocation::FileCopyAllocation(const char* filename,
		ErrorReporter* error_reporter)
	: Allocation(error_reporter)
{
	// Obtain the file size, using an alternative method that is does not
	// require fstat for more compatibility.
	std::unique_ptr<FILE, decltype(&fclose)> file(fopen(filename, "rb"), fclose);
	if (!file) {
		error_reporter_->Report("Could not open '%s'.", filename);
		return;
	}
	// TODO(ahentz): Why did you think using fseek here was better for finding
	// the size?
	struct stat sb;
	if (fstat(fileno(file.get()), &sb) != 0) {
		error_reporter_->Report("Failed to get file size of '%s'.", filename);
		return;
	}
	buffer_size_bytes_ = sb.st_size;
	std::unique_ptr<char[]> buffer(new char[buffer_size_bytes_]);
	if (!buffer) {
		error_reporter_->Report("Malloc of buffer to hold copy of '%s' failed.",
			filename);
		return;
	}
	size_t bytes_read =
		fread(buffer.get(), sizeof(char), buffer_size_bytes_, file.get());
	if (bytes_read != buffer_size_bytes_) {
		error_reporter_->Report("Read of '%s' failed (too few bytes read).",
			filename);
		return;
	}
	// Versions of GCC before 6.2.0 don't support std::move from non-const
	// char[] to const char[] unique_ptrs.
	copied_buffer_.reset(const_cast<char const*>(buffer.release()));
}
#else
#include "autoconf.h"
#include "ff.h"

FileCopyAllocation::FileCopyAllocation(const char* filename,
	ErrorReporter* error_reporter)
	: Allocation(error_reporter)
{
	// Obtain the file size, using an alternative method that is does not
	// require fstat for more compatibility.
	//struct fs_file_t fs_file;
	FIL fil;
	FILINFO finfo;
	unsigned int read_len;
	FRESULT rc = f_open(&fil, &filename[1], FA_READ);

	if (rc != FR_OK) {
		error_reporter_->Report("Could not open '%s'. rc:", filename, rc);
		return;
	}

	rc = f_stat(&filename[1], &finfo);
	buffer_size_bytes_ = f_size(&fil);

	std::unique_ptr<char[]> buffer(new char[buffer_size_bytes_]);
	if (!buffer) {
		error_reporter_->Report("Malloc of buffer to hold copy of '%s' failed.",
			filename);
		return;
	}
	rc = f_read(&fil, buffer.get(), buffer_size_bytes_, &read_len);
	if (read_len != buffer_size_bytes_) {
		error_reporter_->Report("Read of '%s' failed (too few bytes read).readout:%d, file size:%d, rc:%d",
			filename, read_len, (int)buffer_size_bytes_, rc);
		return;
	}
	// Versions of GCC before 6.2.0 don't support std::move from non-const
	// char[] to const char[] unique_ptrs.
	copied_buffer_.reset(const_cast<char const*>(buffer.release()));
}

#endif

FileCopyAllocation::~FileCopyAllocation() {}

const void* FileCopyAllocation::base() const { return copied_buffer_.get(); }

size_t FileCopyAllocation::bytes() const { return buffer_size_bytes_; }

bool FileCopyAllocation::valid() const { return copied_buffer_ != nullptr; }

MemoryAllocation::MemoryAllocation(const void* ptr, size_t num_bytes,
                                   ErrorReporter* error_reporter)
    : Allocation(error_reporter) {
  buffer_ = ptr;
  buffer_size_bytes_ = num_bytes;
}

MemoryAllocation::~MemoryAllocation() {}

const void* MemoryAllocation::base() const { return buffer_; }

size_t MemoryAllocation::bytes() const { return buffer_size_bytes_; }

bool MemoryAllocation::valid() const { return buffer_ != nullptr; }

}  // namespace tflite
