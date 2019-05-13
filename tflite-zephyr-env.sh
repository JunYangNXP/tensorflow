export TFLITE_BASE=$( builtin cd "$( dirname "$DIR" )" > /dev/null && pwd ${PWD_OPT})
export TFLITE_MCU_THIRD_PARTY_BASE=$( builtin cd "$( dirname "$DIR" )" > /dev/null && pwd ${PWD_OPT})/tensorflow/contrib/lite/mcu_3rd_party
export TFLITE_THIRD_PARTY_BASE=$( builtin cd "$( dirname "$DIR" )" > /dev/null && pwd ${PWD_OPT})/tensorflow/contrib/lite/tools/make/downloads
