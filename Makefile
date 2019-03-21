# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

GRPC_SRC_PATH ?= ./grpc
GOOGLEAPIS_SRC_PATH ?= ./googleapis
GOOGLEAPIS_GENS_PATH ?= $(GOOGLEAPIS_SRC_PATH)/gens
GOOGLEAPIS_ASSISTANT_PATH = google/assistant/embedded/v1alpha2

MATRIX_SRC_PATH ?= /home/pi/matrix-creator-hal/cpp

PROTOC ?= protoc
PROTOC_PLUGINS_PATH ?= /usr/local/bin

GOOGLEAPIS_API_CCS = $(shell find $(GOOGLEAPIS_GENS_PATH)/google/api \
	-name '*.pb.cc')
GOOGLEAPIS_ASSISTANT_CCS = $(shell find $(GOOGLEAPIS_GENS_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH) \
	-name '*.pb.cc')
GOOGLEAPIS_TYPE_CCS = $(shell find $(GOOGLEAPIS_GENS_PATH)/google/type \
	-name '*.pb.cc')
GOOGLEAPIS_RPC_CCS = $(shell find $(GOOGLEAPIS_GENS_PATH)/google/rpc \
	-name '*.pb.cc')

GOOGLEAPIS_CCS = $(GOOGLEAPIS_API_CCS) $(GOOGLEAPIS_RPC_CCS) $(GOOGLEAPIS_TYPE_CCS)

GOOGLEAPIS_ASSISTANT_CCS = $(GOOGLEAPIS_GENS_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH)/embedded_assistant.pb.cc \
                           $(GOOGLEAPIS_GENS_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH)/embedded_assistant.grpc.pb.cc

MATRIX_SRC_CCS = $(shell find $(MATRIX_SRC_PATH) \
	-name '*.pb.cpp')

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)

PKG_CONFIG ?= pkg-config
HAS_PKG_CONFIG ?= $(shell command -v $(PKG_CONFIG) >/dev/null 2>&1 && echo true || echo false)
ifeq ($(HAS_PKG_CONFIG),true)
GRPC_GRPCPP_CFLAGS=`pkg-config --cflags grpc++ grpc`
GRPC_GRPCPP_LDLAGS=`pkg-config --libs grpc++ grpc`
ALSA_CFLAGS=`pkg-config --cflags alsa`
ALSA_LDFLAGS=`pkg-config --libs alsa`
else
GRPC_GRPCPP_CFLAGS ?=
GRPC_GRPCPP_LDLAGS ?= 
ALSA_CFLAGS ?=
ALSA_LDFLAGS ?=
endif

CPPFLAGS += -I$(GOOGLEAPIS_GENS_PATH) \
            -I$(GRPC_SRC_PATH) \
	    -I$(MATRIX_SRC_PATH) \
            -I./src \
	    -I/usr/local/include 

# 

CXXFLAGS += -std=c++11 $(GRPC_GRPCPP_CFLAGS) \
	    
LDFLAGS += -L/usr/lib \
	   -L/usr/lib/arm-linux-gnueabihf

LDLIBS += -lwiringPi -lwiringPiDev \
	  -lfftw3 -lfftw3f 

# grpc_cronet is for JSON functions in gRPC library.
ifeq ($(SYSTEM),Darwin)
LDFLAGS += $(GRPC_GRPCPP_LDLAGS) \
           -lgrpc++_reflection -lgrpc_cronet \
           -lprotobuf -lpthread -ldl
else
LDFLAGS += $(GRPC_GRPCPP_LDLAGS) \
           -lgrpc_cronet -Wl,--no-as-needed -lgrpc++_reflection \
           -Wl,--as-needed -lprotobuf -lpthread -ldl
endif

AUDIO_SRCS =
ifeq ($(SYSTEM),Linux)
AUDIO_SRCS += ./src/assistant/audio_input_alsa.cc ./src/assistant/audio_output_alsa.cc
CXXFLAGS += $(ALSA_CFLAGS)
LDFLAGS += $(ALSA_LDFLAGS)
endif

CORE_SRCS = ./src/assistant/base64_encode.cc ./src/assistant/json_util.cc
AUDIO_INPUT_FILE_SRCS = ./src/assistant/audio_input_file.cc
ASSISTANT_AUDIO_SRCS = ./src/assistant/run_assistant_audio.cc
ASSISTANT_FILE_SRCS = ./src/assistant/run_assistant_file.cc
ASSISTANT_TEXT_SRCS = ./src/assistant/run_assistant_text.cc

MATRIX_GPIO_SRC = ../matrix-creator-hal/cpp/driver/gpio_control.cpp
MATRIX_IMUSENS_SRC = ../matrix-creator-hal/cpp/driver/imu_sensor.cpp
MATRIX_IOBUS_SRC = ../matrix-creator-hal/cpp/driver/matrixio_bus.cpp
MATRIX_BUSKRNL_SRC = ../matrix-creator-hal/cpp/driver/bus_kernel.cpp
MATRIX_BUSDRCT_SRC = ../matrix-creator-hal/cpp/driver/bus_direct.cpp
MATRIX_DRIVER_SRC = ../matrix-creator-hal/cpp/driver/matrix_driver.cpp
MATRIX_DOA_SRC = ../matrix-creator-hal/cpp/driver/direction_of_arrival.cpp
MATRIX_CROSSCOR_SRC = ../matrix-creator-hal/cpp/driver/cross_correlation.cpp
MATRIX_EVLOOP_SRC = ../matrix-creator-hal/cpp/driver/everloop.cpp
MATRIX_MICARRAY_SRC = ../matrix-creator-hal/cpp/driver/microphone_array.cpp
MATRIX_MICCORE_SRC = ../matrix-creator-hal/cpp/driver/microphone_core.cpp

ROBOT_MOVEMENT_SRC = ./src/assistant/robot_movement.cc


ASSISTANT_O       = $(CORE_SRCS:.cc=.o) \
                    $(AUDIO_SRCS:.cc=.o) \
                    $(AUDIO_INPUT_FILE_SRCS:.cc=.o) \
                    $(ASSISTANT_AUDIO_SRCS:.cc=.o) \
                    $(ASSISTANT_FILE_SRCS:.cc=.o) \
                    $(ASSISTANT_TEXT_SRCS:.cc=.o) \
		    $(MATRIX_GPIO_SRC:.cpp=.o) \
		    $(MATRIX_IMUSENS_SRC:.cpp=.o) \
		    $(MATRIX_IOBUS_SRC:.cpp=.o) \
		    $(MATRIX_BUSKRNL_SRC:.cpp=.o) \
		    $(MATRIX_BUSDRCT_SRC:.cpp=.o) \
		    $(MATRIX_DRIVER_SRC:.cpp=.o) \
		    $(MATRIX_DOA_SRC:.cpp=.o) \
		    $(MATRIX_CROSSCOR_SRC:.cpp=.o) \
		    $(MATRIX_EVLOOP_SRC:.cpp=.o) \
		    $(MATRIX_MICARRAY_SRC:.cpp=.o) \
		    $(MATRIX_MICCORE_SRC:.cpp=.o) \
		    $(ROBOT_MOVEMENT_SRC:.cc=.o)
ASSISTANT_AUDIO_O = $(CORE_SRCS:.cc=.o) \
                    $(AUDIO_SRCS:.cc=.o) \
                    $(AUDIO_INPUT_FILE_SRCS:.cc=.o) \
                    $(ASSISTANT_AUDIO_SRCS:.cc=.o) \
		    $(MATRIX_GPIO_SRC:.cpp=.o) \
		    $(MATRIX_IMUSENS_SRC:.cpp=.o) \
		    $(MATRIX_IOBUS_SRC:.cpp=.o) \
		    $(MATRIX_BUSKRNL_SRC:.cpp=.o) \
		    $(MATRIX_BUSDRCT_SRC:.cpp=.o) \
		    $(MATRIX_DRIVER_SRC:.cpp=.o) \
		    $(MATRIX_DOA_SRC:.cpp=.o) \
		    $(MATRIX_CROSSCOR_SRC:.cpp=.o) \
		    $(MATRIX_EVLOOP_SRC:.cpp=.o) \
		    $(MATRIX_MICARRAY_SRC:.cpp=.o) \
		    $(MATRIX_MICCORE_SRC:.cpp=.o) \
		    $(ROBOT_MOVEMENT_SRC:.cc=.o)
ASSISTANT_FILE_O  = $(CORE_SRCS:.cc=.o) \
                    $(AUDIO_INPUT_FILE_SRCS:.cc=.o) \
                    $(ASSISTANT_FILE_SRCS:.cc=.o)
ASSISTANT_TEXT_O  = $(CORE_SRCS:.cc=.o) \
                    $(ASSISTANT_TEXT_SRCS:.cc=.o)

.PHONY: all
all: run_assistant

googleapis.ar: $(GOOGLEAPIS_CCS:.cc=.o)
	$(AR) r $@ $?

.PHONY: run_assistant
run_assistant: run_assistant_audio run_assistant_file run_assistant_text

run_assistant_audio: $(GOOGLEAPIS_ASSISTANT_CCS:.cc=.o) googleapis.ar \
	$(ASSISTANT_AUDIO_O)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

run_assistant_file: $(GOOGLEAPIS_ASSISTANT_CCS:.cc=.o) googleapis.ar \
	$(ASSISTANT_FILE_O)
	$(CXX) $^ $(LDFLAGS) -o $@

run_assistant_text: $(GOOGLEAPIS_ASSISTANT_CCS:.cc=.o) googleapis.ar \
	$(ASSISTANT_TEXT_O)
	$(CXX) $^ $(LDFLAGS) -o $@

json_util_test: ./src/assistant/json_util.o ./src/assistant/json_util_test.o
	$(CXX) $^ $(LDFLAGS) -o $@

$(GOOGLEAPIS_ASSISTANT_CCS:.cc=.h) $(GOOGLEAPIS_ASSISTANT_CCS):
	$(PROTOC) -I=$(GOOGLEAPIS_SRC_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH) --proto_path=.:$(GOOGLEAPIS_SRC_PATH) \
	--cpp_out=$(GOOGLEAPIS_GENS_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH) \
	--grpc_out=$(GOOGLEAPIS_GENS_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH) \
	--plugin=protoc-gen-grpc=$(PROTOC_PLUGINS_PATH)/grpc_cpp_plugin \
	$(GOOGLEAPIS_SRC_PATH)/$(GOOGLEAPIS_ASSISTANT_PATH)/embedded_assistant.proto $^

.PHONY: protobufs
protobufs: $(GOOGLEAPIS_ASSISTANT_CCS:.cc=.h) $(GOOGLEAPIS_ASSISTANT_CCS)

.PHONY: clean
clean:
	rm -f run_assistant_text run_assistant_audio run_assistant_file googleapis.ar \
		$(GOOGLEAPIS_CCS:.cc=.o) \
		$(GOOGLEAPIS_ASSISTANT_CCS) $(GOOGLEAPIS_ASSISTANT_CCS:.cc=.h) \
		$(GOOGLEAPIS_ASSISTANT_CCS:.cc=.o) \
		$(ASSISTANT_O)
