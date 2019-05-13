# where py object files go (they have a name prefix to prevent filename clashes)
TFLITE_BUILD = $(BUILD)/tflite

TFLITE_FARMHASH_SRC =      $(wildcard $(TFLITE_MCU_THIRD_PARTY_BASE)/farmhash/src/*.cc)

TFLITE_FARMHASH_TEST_SRC =       $(wildcard $(TFLITE_MCU_THIRD_PARTY_BASE)/farmhash/src/*test*.cc)

TFLITE_FARMHASH_SRC := $(filter-out $(TFLITE_FARMHASH_TEST_SRC),$(TFLITE_FARMHASH_SRC))

TFLITE_FARMHASH_BASE_SRC = $(notdir $(TFLITE_FARMHASH_SRC))

TFLITE_FFT2D_SRC =      $(wildcard $(TFLITE_MCU_THIRD_PARTY_BASE)/fft2d/*.c)

TFLITE_FFT2D_BASE_SRC = $(notdir $(TFLITE_FFT2D_SRC))

TFLITE_APP_SRC =      $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/examples/label_image/*.cc)

TFLITE_APP_TEST_SRC =      $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/examples/label_image/*test*.cc)

TFLITE_APP_SRC := $(filter-out $(TFLITE_APP_TEST_SRC),$(TFLITE_APP_SRC))

TFLITE_APP_BASE_SRC = $(notdir $(TFLITE_APP_SRC))

TFLITE_MAIN_SRC =	$(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/*.cc)

TFLITE_MAIN_TEST_SRC =       $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/*test*.cc)

TFLITE_MAIN_SRC := $(filter-out $(TFLITE_MAIN_TEST_SRC),$(TFLITE_MAIN_SRC))

TFLITE_MAIN_BASE_SRC = $(notdir $(TFLITE_MAIN_SRC))

TFLITE_KERNELS_SRC = $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/*.cc)

TFLITE_KERNELS_TEST_SRC = $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/*test*.cc)

TFLITE_KERNELS_SRC := $(filter-out $(TFLITE_KERNELS_TEST_SRC),$(TFLITE_KERNELS_SRC))

TFLITE_KERNELS_BASE_SRC = $(notdir $(TFLITE_KERNELS_SRC))

TFLITE_KERNELS_INTERNAL_SRC = $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/internal/*.cc)

TFLITE_KERNELS_INTERNAL_TEST_SRC = $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/internal/*test*.cc)

TFLITE_KERNELS_INTERNAL_SRC := $(filter-out $(TFLITE_KERNELS_INTERNAL_TEST_SRC),$(TFLITE_KERNELS_INTERNAL_SRC))

TFLITE_KERNELS_INTERNAL_BASE_SRC = $(notdir $(TFLITE_KERNELS_INTERNAL_SRC))

TFLITE_KERNELS_INTERNAL_RFC_SRC = $(wildcard $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/internal/reference/*.cc)

TFLITE_KERNELS_INTERNAL_RFC_BASE_SRC = $(notdir $(TFLITE_KERNELS_INTERNAL_RFC_SRC))

TFLITE_BASE_SRCS = $(TFLITE_MAIN_BASE_SRC) $(TFLITE_KERNELS_BASE_SRC) $(TFLITE_KERNELS_INTERNAL_BASE_SRC) $(TFLITE_APP_BASE_SRC)

TFLITE_BASE_SRCS += $(TFLITE_FARMHASH_BASE_SRC)

TFLITE_BASE_SRCS += $(TFLITE_KERNELS_INTERNAL_RFC_BASE_SRC)

TFLITE_OBJS = $(addprefix $(TFLITE_BUILD)/, $(TFLITE_BASE_SRCS:.cc=.o))

TFLITE_OBJS += $(addprefix $(TFLITE_BUILD)/, $(TFLITE_FFT2D_BASE_SRC:.c=.o))

# Anything that depends on FORCE will be considered out-of-date
FORCE:
.PHONY: FORCE


