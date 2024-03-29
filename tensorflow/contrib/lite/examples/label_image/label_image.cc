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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <fcntl.h>      // NOLINT(build/include_order)
#include <getopt.h>     // NOLINT(build/include_order)
#include <sys/time.h>   // NOLINT(build/include_order)
#include <sys/types.h>  // NOLINT(build/include_order)
#ifndef TFLITE_MCU
#include <sys/uio.h>    // NOLINT(build/include_order)
#endif
#include <unistd.h>     // NOLINT(build/include_order)

#include "tensorflow/contrib/lite/kernels/register.h"
#include "tensorflow/contrib/lite/model.h"
#include "tensorflow/contrib/lite/optional_debug_tools.h"
#include "tensorflow/contrib/lite/string_util.h"

#include "tensorflow/contrib/lite/examples/label_image/bitmap_helpers.h"
#include "tensorflow/contrib/lite/examples/label_image/get_top_n.h"

#define LOG(x) std::cerr

namespace tflite {
namespace label_image {

#ifndef TFLITE_MCU
double get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

// Takes a file name, and loads a list of labels from it, one per line, and
// returns a vector of the strings. It pads with empty strings so the length
// of the result is a multiple of 16, because our model expects that.
TfLiteStatus ReadLabelsFile(const string& file_name,
                            std::vector<string>* result,
                            size_t* found_label_count) {
  std::ifstream file(file_name);
  if (!file) {
    LOG(FATAL) << "Labels file " << file_name << " not found\n";
    return kTfLiteError;
  }
  result->clear();
  string line;
  while (std::getline(file, line)) {
    result->push_back(line);
  }
  *found_label_count = result->size();
  const int padding = 16;
  while (result->size() % padding) {
    result->emplace_back();
  }
  return kTfLiteOk;
}

void PrintProfilingInfo(const profiling::ProfileEvent* e, uint32_t op_index,
                        TfLiteRegistration registration) {
  // output something like
  // time (ms) , Node xxx, OpCode xxx, symblic name
  //      5.352, Node   5, OpCode   4, DEPTHWISE_CONV_2D

  LOG(INFO) << std::fixed << std::setw(10) << std::setprecision(3)
            << (e->end_timestamp_us - e->begin_timestamp_us) / 1000.0
            << ", Node " << std::setw(3) << std::setprecision(3) << op_index
            << ", OpCode " << std::setw(3) << std::setprecision(3)
            << registration.builtin_code << ", "
            << EnumNameBuiltinOperator(
                   static_cast<BuiltinOperator>(registration.builtin_code))
            << "\n";
}

void RunInference(Settings* s) {
  if (!s->model_name.c_str()) {
    LOG(ERROR) << "no model file name\n";
    exit(-1);
  }

  std::unique_ptr<tflite::FlatBufferModel> model;
  std::unique_ptr<tflite::Interpreter> interpreter;
  model = tflite::FlatBufferModel::BuildFromFile(s->model_name.c_str());
  if (!model) {
    LOG(FATAL) << "\nFailed to mmap model " << s->model_name << "\n";
    exit(-1);
  }
  LOG(INFO) << "Loaded model " << s->model_name << "\n";
  model->error_reporter();
  LOG(INFO) << "resolved reporter\n";

  tflite::ops::builtin::BuiltinOpResolver resolver;

  tflite::InterpreterBuilder(*model, resolver)(&interpreter);
  if (!interpreter) {
    LOG(FATAL) << "Failed to construct interpreter\n";
    exit(-1);
  }

  interpreter->UseNNAPI(s->accel);

  if (s->verbose) {
    LOG(INFO) << "tensors size: " << interpreter->tensors_size() << "\n";
    LOG(INFO) << "nodes size: " << interpreter->nodes_size() << "\n";
    LOG(INFO) << "inputs: " << interpreter->inputs().size() << "\n";
    LOG(INFO) << "input(0) name: " << interpreter->GetInputName(0) << "\n";

    int t_size = interpreter->tensors_size();
    for (int i = 0; i < t_size; i++) {
      if (interpreter->tensor(i)->name)
        LOG(INFO) << i << ": " << interpreter->tensor(i)->name << ", "
                  << interpreter->tensor(i)->bytes << ", "
                  << interpreter->tensor(i)->type << ", "
                  << interpreter->tensor(i)->params.scale << ", "
                  << interpreter->tensor(i)->params.zero_point << "\n";
    }
  }

  if (s->number_of_threads != -1) {
    interpreter->SetNumThreads(s->number_of_threads);
  }

  int image_width = 224;
  int image_height = 224;
  int image_channels = 3;
  std::vector<uint8_t> in = read_bmp(s->input_bmp_name, &image_width,
                                     &image_height, &image_channels, s);

  int input = interpreter->inputs()[0];
  if (s->verbose) LOG(INFO) << "input: " << input << "\n";

  const std::vector<int> inputs = interpreter->inputs();
  const std::vector<int> outputs = interpreter->outputs();

  if (s->verbose) {
    LOG(INFO) << "number of inputs: " << inputs.size() << "\n";
    LOG(INFO) << "number of outputs: " << outputs.size() << "\n";
  }

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    LOG(FATAL) << "Failed to allocate tensors!";
  }

  if (s->verbose) PrintInterpreterState(interpreter.get());

  // get input dimension from the input tensor metadata
  // assuming one input only
  TfLiteIntArray* dims = interpreter->tensor(input)->dims;
  int wanted_height = dims->data[1];
  int wanted_width = dims->data[2];
  int wanted_channels = dims->data[3];

  switch (interpreter->tensor(input)->type) {
    case kTfLiteFloat32:
      s->input_floating = true;
      resize<float>(interpreter->typed_tensor<float>(input), in.data(),
                    image_height, image_width, image_channels, wanted_height,
                    wanted_width, wanted_channels, s);
      break;
    case kTfLiteUInt8:
      resize<uint8_t>(interpreter->typed_tensor<uint8_t>(input), in.data(),
                      image_height, image_width, image_channels, wanted_height,
                      wanted_width, wanted_channels, s);
      break;
    default:
      LOG(FATAL) << "cannot handle input type "
                 << interpreter->tensor(input)->type << " yet";
      exit(-1);
  }

  profiling::Profiler* profiler = new profiling::Profiler();
  interpreter->SetProfiler(profiler);

  if (s->profiling) profiler->StartProfiling();

  struct timeval start_time, stop_time;
  gettimeofday(&start_time, nullptr);
  for (int i = 0; i < s->loop_count; i++) {
    if (interpreter->Invoke() != kTfLiteOk) {
      LOG(FATAL) << "Failed to invoke tflite!\n";
    }
  }
  gettimeofday(&stop_time, nullptr);
  LOG(INFO) << "invoked \n";
  LOG(INFO) << "average time: "
            << (get_us(stop_time) - get_us(start_time)) / (s->loop_count * 1000)
            << " ms \n";

  if (s->profiling) {
    profiler->StopProfiling();
    auto profile_events = profiler->GetProfileEvents();
    for (int i = 0; i < profile_events.size(); i++) {
      auto op_index = profile_events[i]->event_metadata;
      const auto node_and_registration =
          interpreter->node_and_registration(op_index);
      const TfLiteRegistration registration = node_and_registration->second;
      PrintProfilingInfo(profile_events[i], op_index, registration);
    }
  }

  const float threshold = 0.001f;

  std::vector<std::pair<float, int>> top_results;

  int output = interpreter->outputs()[0];
  TfLiteIntArray* output_dims = interpreter->tensor(output)->dims;
  // assume output dims to be something like (1, 1, ... ,size)
  auto output_size = output_dims->data[output_dims->size - 1];
  switch (interpreter->tensor(output)->type) {
    case kTfLiteFloat32:
      get_top_n<float>(interpreter->typed_output_tensor<float>(0), output_size,
                       s->number_of_results, threshold, &top_results, true);
      break;
    case kTfLiteUInt8:
      get_top_n<uint8_t>(interpreter->typed_output_tensor<uint8_t>(0),
                         output_size, s->number_of_results, threshold,
                         &top_results, false);
      break;
    default:
      LOG(FATAL) << "cannot handle output type "
                 << interpreter->tensor(input)->type << " yet";
      exit(-1);
  }

  std::vector<string> labels;
  size_t label_count;

  if (ReadLabelsFile(s->labels_file_name, &labels, &label_count) != kTfLiteOk)
    exit(-1);

  for (const auto& result : top_results) {
    const float confidence = result.first;
    const int index = result.second;
    LOG(INFO) << confidence << ": " << index << " " << labels[index] << "\n";
  }
}

void display_usage() {
  LOG(INFO) << "label_image\n"
            << "--accelerated, -a: [0|1], use Android NNAPI or not\n"
            << "--count, -c: loop interpreter->Invoke() for certain times\n"
            << "--input_mean, -b: input mean\n"
            << "--input_std, -s: input standard deviation\n"
            << "--image, -i: image_name.bmp\n"
            << "--labels, -l: labels for the model\n"
            << "--tflite_model, -m: model_name.tflite\n"
            << "--profiling, -p: [0|1], profiling or not\n"
            << "--num_results, -r: number of results to show\n"
            << "--threads, -t: number of threads\n"
            << "--verbose, -v: [0|1] print more information\n"
            << "\n";
}

int Main(int argc, char** argv) {
  Settings s;

  int c;
  while (1) {
    static struct option long_options[] = {
        {"accelerated", required_argument, nullptr, 'a'},
        {"count", required_argument, nullptr, 'c'},
        {"verbose", required_argument, nullptr, 'v'},
        {"image", required_argument, nullptr, 'i'},
        {"labels", required_argument, nullptr, 'l'},
        {"tflite_model", required_argument, nullptr, 'm'},
        {"profiling", required_argument, nullptr, 'p'},
        {"threads", required_argument, nullptr, 't'},
        {"input_mean", required_argument, nullptr, 'b'},
        {"input_std", required_argument, nullptr, 's'},
        {"num_results", required_argument, nullptr, 'r'},
        {nullptr, 0, nullptr, 0}};

    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long(argc, argv, "a:b:c:f:i:l:m:p:r:s:t:v:", long_options,
                    &option_index);

    /* Detect the end of the options. */
    if (c == -1) break;

    switch (c) {
      case 'a':
        s.accel = strtol(optarg, nullptr, 10);  // NOLINT(runtime/deprecated_fn)
        break;
      case 'b':
        s.input_mean = strtod(optarg, nullptr);
        break;
      case 'c':
        s.loop_count =
            strtol(optarg, nullptr, 10);  // NOLINT(runtime/deprecated_fn)
        break;
      case 'i':
        s.input_bmp_name = optarg;
        break;
      case 'l':
        s.labels_file_name = optarg;
        break;
      case 'm':
        s.model_name = optarg;
        break;
      case 'p':
        s.profiling =
            strtol(optarg, nullptr, 10);  // NOLINT(runtime/deprecated_fn)
        break;
      case 'r':
        s.number_of_results =
            strtol(optarg, nullptr, 10);  // NOLINT(runtime/deprecated_fn)
        break;
      case 's':
        s.input_std = strtod(optarg, nullptr);
        break;
      case 't':
        s.number_of_threads = strtol(  // NOLINT(runtime/deprecated_fn)
            optarg, nullptr, 10);
        break;
      case 'v':
        s.verbose =
            strtol(optarg, nullptr, 10);  // NOLINT(runtime/deprecated_fn)
        break;
      case 'h':
      case '?':
        /* getopt_long already printed an error message. */
        display_usage();
        exit(-1);
      default:
        exit(-1);
    }
  }
  RunInference(&s);
  return 0;
}
#else
/* Loads a list of labels, one per line, and returns a vector of the strings.
   It pads with empty strings so the length of the result is a multiple of 16,
   because the model expects that. */
uint8_t *g_label_data;
#include "autoconf.h"
#include "ff.h"

TfLiteStatus ReadLabelsFile(const string& file_name,
		std::vector<string>* result,
		size_t* found_label_count)
{
	char *filename = (char *)file_name.c_str();
	FIL fil;
	FRESULT rc = f_open(&fil, &filename[1], FA_READ);
	FILINFO finfo;
	unsigned int len, ret_len;
	string line;
	const int padding = 16;

	if (rc != FR_OK) {
		LOG(FATAL) << "Labels file " << filename << " not found\n";
		return kTfLiteError;
	}
	result->clear();

	rc = f_stat(&filename[1], &finfo);
	len = finfo.fsize;
	g_label_data = new uint8_t[len];
	if (!g_label_data)
		return kTfLiteError;

	rc = f_read(&fil, g_label_data, len, &ret_len);
	if (rc != FR_OK || len != ret_len) {
		LOG(FATAL) << "Labels file read error" << filename
			<< " ,rc:" << rc << ",len:" << len << ",ret_len:" << ret_len << "\r\n";
		return kTfLiteError;
	}
	line = (char *)g_label_data;
	std::istringstream stream(line);
	while (std::getline(stream, line)) {
		result->push_back(line);
	}

	*found_label_count = result->size();
	while (result->size() % padding) {
		result->emplace_back();
	}
	return kTfLiteOk;
}

extern "C" {
extern signed long long z_impl_k_uptime_get(void);
}


void RunInference(Settings* s)
{
	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;
	int image_width, image_height, image_channels, input;
	int wanted_height, wanted_width, wanted_channels, output, i;
	TfLiteIntArray *dims, *output_dims;
	std::vector<uint8_t> in_vector;
	const std::vector<int> inputs;
	const std::vector<int> outputs;
	const float threshold = 0.001f;
	std::vector<std::pair<float, int>> top_results;
	std::vector<string> labels;
	size_t label_count;
	signed long long time1, time2;

	time1 = z_impl_k_uptime_get();
	model = tflite::FlatBufferModel::BuildFromFile(s->model_name.c_str());
	if (!model) {
		LOG(FATAL) << "Failed to load model\r\n";
		return;
	}
	time2 = z_impl_k_uptime_get();

	std::cout << "Build Model: " << time2 - time1 <<	" milliseconds\r\n";
	model->error_reporter();

	tflite::ops::builtin::BuiltinOpResolver resolver;

	tflite::InterpreterBuilder(*model, resolver)(&interpreter);
	if (!interpreter) {
		LOG(FATAL) << "Failed to construct interpreter\r\n";
		return;
	}

	if (s->verbose) {
		LOG(INFO) << "tensors size: " << interpreter->tensors_size() << "\r\n";
		LOG(INFO) << "nodes size: " << interpreter->nodes_size() << "\r\n";
		LOG(INFO) << "inputs: " << interpreter->inputs().size() << "\r\n";
		LOG(INFO) << "input(0) name: " << interpreter->GetInputName(0) << "\r\n";

		int t_size = interpreter->tensors_size();
		for (int i = 0; i < t_size; i++) {
			if (interpreter->tensor(i)->name)
				LOG(INFO) << i << ": " << interpreter->tensor(i)->name << ", "
					<< interpreter->tensor(i)->bytes << ", "
					<< interpreter->tensor(i)->type << ", "
					<< interpreter->tensor(i)->params.scale << ", "
					<< interpreter->tensor(i)->params.zero_point << "\r\n";
		}
	}

	in_vector = read_bmp(s->input_bmp_name, &image_width,
		&image_height, &image_channels, s);

	input = interpreter->inputs()[0];
	if (s->verbose)
		LOG(INFO) << "input: " << input << "\r\n";

	inputs = interpreter->inputs();
	outputs = interpreter->outputs();

	if (s->verbose) {
		LOG(INFO) << "number of inputs: " << inputs.size() << "\r\n";
		LOG(INFO) << "number of outputs: " << outputs.size() << "\r\n";
	}

	if (interpreter->AllocateTensors() != kTfLiteOk) {
		LOG(FATAL) << "Failed to allocate tensors!";
	}

	if (s->verbose)
		PrintInterpreterState(interpreter.get());

	time1 = z_impl_k_uptime_get();
	/* Get input dimension from the input tensor metadata
	assuming one input only */
	dims = interpreter->tensor(input)->dims;
	wanted_height = dims->data[1];
	wanted_width = dims->data[2];
	wanted_channels = dims->data[3];

	switch (interpreter->tensor(input)->type) {
		case kTfLiteFloat32:
			s->input_floating = true;
			resize<float>(interpreter->typed_tensor<float>(input), in_vector.data(), image_height,
				image_width, image_channels, wanted_height, wanted_width,
				wanted_channels, s);
			break;
		case kTfLiteUInt8:
			resize<uint8_t>(interpreter->typed_tensor<uint8_t>(input), in_vector.data(),
				image_height, image_width, image_channels, wanted_height,
				wanted_width, wanted_channels, s);
			break;
		default:
			LOG(FATAL) << "cannot handle input type "
				<< interpreter->tensor(input)->type << " yet";
			exit(-1);
	}
	time2 = z_impl_k_uptime_get();

	std::cout << "Resize: " << time2 - time1 <<	" milliseconds\r\n";

	time1 = z_impl_k_uptime_get();
	for (i = 0; i < s->loop_count; i++) {
		if (interpreter->Invoke() != kTfLiteOk) {
			LOG(FATAL) << "Failed to invoke tflite!\r\n";
		}
	}
	time2 = z_impl_k_uptime_get();

	std::cout << "Invoke: " << time2 - time1 << " milliseconds\r\n";

	output = interpreter->outputs()[0];
	output_dims = interpreter->tensor(output)->dims;
	/* Assume output dims to be something like (1, 1, ... , size) */
	auto output_size = output_dims->data[output_dims->size - 1];
	switch (interpreter->tensor(output)->type) {
		case kTfLiteFloat32:
			get_top_n<float>(interpreter->typed_output_tensor<float>(0), output_size,
				s->number_of_results, threshold, &top_results, true);
			break;
		case kTfLiteUInt8:
			get_top_n<uint8_t>(interpreter->typed_output_tensor<uint8_t>(0),
				output_size, s->number_of_results, threshold,
				&top_results, false);
			break;
		default:
			LOG(FATAL) << "cannot handle output type "
				<< interpreter->tensor(input)->type << " yet";
			return;
	}

	if (ReadLabelsFile(s->labels_file_name, &labels, &label_count) != kTfLiteOk)
		return;

	LOG(INFO) << "Detected:\r\n";
	for (const auto& result : top_results) {
		const float confidence = result.first;
		const int index = result.second;
		LOG(INFO) << "  " << labels[index] << " (" << (int)(confidence * 100) << "% confidence)\r\n";
	}

	if (g_label_data)
		delete g_label_data;
	g_label_data = 0;
}

#endif

}  // namespace label_image
}  // namespace tflite

#ifndef TFLITE_MCU
int main(int argc, char** argv) {
  return tflite::label_image::Main(argc, argv);
}
#else
extern "C" {
void tf_lite_label_img(const char *model_name,
	const char *img_name, const char *label_name)
{
	tflite::label_image::Settings s;

	std::cout << "Label image example using a TensorFlow Lite model\r\n";
	s.model_name = model_name;
	s.input_bmp_name = img_name;
	s.labels_file_name = label_name;

	tflite::label_image::RunInference(&s);

	std::cout << "Label image example using a TensorFlow Lite model done!\r\n";
}
}

#endif
