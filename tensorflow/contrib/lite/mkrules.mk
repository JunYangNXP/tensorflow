
# This file expects that OBJ contains a list of all of the object files.
# The directory portion of each object file is used to locate the source
# and should not contain any ..'s but rather be relative to the top of the
# tree.
#
# So for example, py/map.c would have an object file name py/map.o
# The object files will go into the build directory and mantain the same
# directory structure as the source tree. So the final dependency will look
# like this:
#
# build/py/map.o: py/map.c
#
# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.

CROSS_CPP = $(ZEPHYR_SDK_INSTALL_DIR)/arm-zephyr-eabi/bin/arm-zephyr-eabi-g++

vpath %.cc . $(TFLITE_BASE)/tensorflow/contrib/lite/examples/label_image/
vpath %.cc . $(TFLITE_BASE)/tensorflow/contrib/lite/
vpath %.cc . $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/
vpath %.cc . $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/internal/
vpath %.cc . $(TFLITE_BASE)/tensorflow/contrib/lite/kernels/internal/reference/
vpath %.cc . $(TFLITE_MCU_THIRD_PARTY_BASE)/farmhash/src/
vpath %.c . $(TFLITE_MCU_THIRD_PARTY_BASE)/fft2d/
$(TFLITE_BUILD)/%.o : %.cc
	$(ECHO) "CPP $< to $@"
	$(Q)$(CROSS_CPP) $(TF_CPPFLAGS) -c -MD -o $@ $<

$(TFLITE_BUILD)/%.o : %.c
	$(ECHO) "CC $< to $@"
	$(Q)$(CC) $(TF_CFLAGS) -c -MD -o $@ $<

LIBTFLITE = libtflite.a

# We can execute extra commands after library creation using
# LIBMICROPYTHON_EXTRA_CMD. This may be needed e.g. to integrate
# with 3rd-party projects which don't have proper dependency
# tracking. Then LIBMICROPYTHON_EXTRA_CMD can e.g. touch some
# other file to cause needed effect, e.g. relinking with new lib.
$(LIBTFLITE): $(TFLITE_OBJS)
	$(AR) rcs $(BUILD)/$(LIBTFLITE) $^
