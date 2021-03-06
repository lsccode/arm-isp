#-----------------------------------------------------------------------------
#   The confidential and proprietary information contained in this file may
#   only be used by a person authorised under and to the extent permitted
#   by a subsisting licensing agreement from ARM Limited or its affiliates.
#
#          (C) COPYRIGHT 2018 ARM Limited or its affiliates
#              ALL RIGHTS RESERVED
#
#   This entire notice must be reproduced on all copies of this file
#   and copies of this file may only be made by a person if such person is
#   permitted to do so under the terms of a subsisting license agreement
#   from ARM Limited or its affiliates.
#-----------------------------------------------------------------------------

#
# autogenerated makefile
# please do not edit
#

SYSTEM ?= linux
GCC ?= $(shell which gcc)
GXX ?= $(shell which g++)
GST ?= $(shell which strip)

override BUILD = build

# cross-platform compiler options for C language
override GCC_FLAGS = -c -MMD -pipe -Wall -O2 -DACT_SUPPORT_LISTEN_COMMAND -DACT_SUPPORT_DIRECT_COMMAND -DACT_DEFAULT_COMMAND=listen -Isrc/main/cpp/include -Isrc/main/include
override GCC_FLAGS += -I../../include

# cross-platform compiler options for C++ language
override GXX_FLAGS = -c -MMD -pipe -Wall -O2 -DACT_SUPPORT_LISTEN_COMMAND -DACT_SUPPORT_DIRECT_COMMAND -DACT_DEFAULT_COMMAND=listen -Isrc/main/cpp/include -Isrc/main/include
override GXX_FLAGS += -I../../include

# cross-platform linker options
override LINKER_FLAGS = -static-libstdc++

# common sources to be compiled for any platform
override GCC_SOURCES = 
override GXX_SOURCES = src/main/cpp/ARMCPP11/Thread.cpp src/main/cpp/ARMCPP11/ConditionVariable.cpp src/main/cpp/ARMCPP11/Mutex.cpp src/main/cpp/HTTPServer/HTTPServer.cpp src/main/cpp/HTTPServer/EventEngine.cpp src/main/cpp/AccessManager/BufferManager.cpp src/main/cpp/AccessManager/AccessBase.cpp src/main/cpp/AccessManager/FWDebugAccess.cpp src/main/cpp/AccessManager/AccessManager.cpp src/main/cpp/AccessManager/AccessRequest.cpp src/main/cpp/AccessManager/FWStreamAccess.cpp src/main/cpp/AccessManager/RegisterDirectAccess.cpp src/main/cpp/BusManager/Driver/I2CDriver.cpp src/main/cpp/BusManager/Driver/UARTDriver.cpp src/main/cpp/BusManager/Driver/FTDIDriver.cpp src/main/cpp/BusManager/Driver/BaseDriverInterface.cpp src/main/cpp/BusManager/Driver/CharDeviceDriver.cpp src/main/cpp/BusManager/Driver/OfflineDriver.cpp src/main/cpp/BusManager/Driver/SocketDriver.cpp src/main/cpp/BusManager/Driver/DevMemDriver.cpp src/main/cpp/BusManager/Driver/MdcMdioDriver.cpp src/main/cpp/BusManager/Driver/SharedMemoryDriver.cpp src/main/cpp/BusManager/TransferPacket.cpp src/main/cpp/BusManager/DriverFactory.cpp src/main/cpp/BusManager/BusManager.cpp src/main/cpp/CommandManager/RequestParser.cpp src/main/cpp/CommandManager/ClientCommand.cpp src/main/cpp/CommandManager/ResponseWriter.cpp src/main/cpp/CommandManager/AccessManagerConfig.cpp src/main/cpp/CommandManager/CommandManager.cpp src/main/cpp/ATL/ATLObject.cpp src/main/cpp/ATL/ATLLogger.cpp src/main/cpp/ATL/ATLApplication.cpp src/main/cpp/ATL/ATLPlatform.cpp src/main/cpp/ATL/ATLTypes.cpp src/main/cpp/ATL/ATLConfig.cpp src/main/cpp/ATL/ATLError.cpp src/main/cpp/ACTServerApp/ACTListenCommand.cpp src/main/cpp/ACTServerApp/ACTServerMain.cpp src/main/cpp/ACTServerApp/ACTServerApp.cpp src/main/cpp/ACTServerApp/ACTDirectCommand.cpp

# taking into account system-based variations:

# for linux
ifeq ($(SYSTEM),linux)
# flags
override GCC_FLAGS += -fPIC
override GXX_FLAGS += -fPIC
override LINKER_FLAGS += -ldl -lrt -lpthread -Wl,-rpath,'$$ORIGIN':'$$ORIGIN'/lib
# sources
override GCC_SOURCES += 
override GXX_SOURCES += src/main/sys/linux/ATLNetwork.cpp src/main/sys/linux/ATLLibrary.cpp

# for android
else ifeq ($(SYSTEM),android)
# flags
override GCC_FLAGS += -fPIC -D_NO_STRTOLL_PLATFORM -D_ANDROID
override GXX_FLAGS += -fPIC -D_NO_STRTOLL_PLATFORM -D_ANDROID
override LINKER_FLAGS += -pie -Wl,-rpath,'$$ORIGIN':'$$ORIGIN'/lib
# sources
override GCC_SOURCES += 
override GXX_SOURCES += src/main/sys/android/ATLNetwork.cpp src/main/sys/android/ATLLibrary.cpp

# fail if system is not provided/supported
else
$(error system $(SYSTEM) is not supported)
endif

# windows system is not supported

override SLIBEXT = a
override DLIBEXT = so

# use C compiler flags provided in command line
override GCC_FLAGS += $(CFLAGS)

# use C++ compiler flags provided in command line
override GXX_FLAGS += $(CXXFLAGS)

# use linker flags provided in command line
override LINKER_FLAGS += $(LDFLAGS)

# filter different extensions of C++ files
override CPP_SOURCES = $(filter %.cpp, $(GXX_SOURCES))
override CC_SOURCES = $(filter %.cc, $(GXX_SOURCES))
override CXX_SOURCES = $(filter %.cxx, $(GXX_SOURCES))
override CP_SOURCES = $(filter %.cp, $(GXX_SOURCES))
override CZZ_SOURCES = $(filter %.c++, $(GXX_SOURCES))

# process sources paths
override OBJPREFIX = $(BUILD)/objects
override OBJECTS = $(addprefix $(OBJPREFIX)/,$(GCC_SOURCES:.c=.o))
override OBJECTS += $(addprefix $(OBJPREFIX)/,$(CPP_SOURCES:.cpp=.o))
override OBJECTS += $(addprefix $(OBJPREFIX)/,$(CC_SOURCES:.cc=.o))
override OBJECTS += $(addprefix $(OBJPREFIX)/,$(CXX_SOURCES:.cxx=.o))
override OBJECTS += $(addprefix $(OBJPREFIX)/,$(CP_SOURCES:.cp=.o))
override OBJECTS += $(addprefix $(OBJPREFIX)/,$(CZZ_SOURCES:.c++=.o))

override OBJDIRS = $(sort $(dir $(OBJECTS)))

override APPTARGET = act-server

# dependencies:

# for include header flags and copying libraries
override HEADER_DEPENDENCIES = libevent
-include $(addprefix ../,$(addsuffix /includes.make,$(HEADER_DEPENDENCIES)))

# for linker flags: dynamic libraries
override DYNAMIC_LIBRARIES = 
override DYNLIBDIRS = $(addprefix ../,$(addsuffix /$(BUILD),$(DYNAMIC_LIBRARIES)))
override LINKER_FLAGS += $(addprefix -L, $(DYNLIBDIRS))
override DYNLIBS = $(sort $(wildcard $(addsuffix /*.$(DLIBEXT)*, $(DYNLIBDIRS))))
override LINKER_FLAGS += $(addprefix -l:, $(notdir $(DYNLIBS)))

-include $(addprefix ../,$(addsuffix /includes.make,$(DYNAMIC_LIBRARIES)))

# for linker flags: static libraries
override STATIC_LIBRARIES = libevent
override STLIBDIRS = $(addprefix ../,$(addsuffix /$(BUILD),$(STATIC_LIBRARIES)))
override LINKER_FLAGS += $(addprefix -L, $(STLIBDIRS))
override STLIBS = $(sort $(wildcard $(addsuffix /*.$(SLIBEXT), $(STLIBDIRS))))
override LINKER_FLAGS += -Wl,-Bstatic
override LINKER_FLAGS += $(addprefix -l:, $(notdir $(STLIBS)))
override LINKER_FLAGS += -Wl,-Bdynamic

-include $(addprefix ../,$(addsuffix /includes.make,$(STATIC_LIBRARIES)))

# recipes:

all: $(BUILD)/$(APPTARGET)
	@$(GST) --strip-debug $(BUILD)/$(APPTARGET)

$(BUILD)/$(APPTARGET): $(OBJECTS)
	$(GXX) $^ $(LINKER_FLAGS) -o $@

$(BUILD):
	@mkdir -p $(OBJDIRS)

$(OBJPREFIX)/%.o: %.c | $(BUILD)
	$(GCC) $(GCC_FLAGS) $< -o $@

$(OBJPREFIX)/%.o: %.cpp | $(BUILD)
	$(GXX) $(GXX_FLAGS) $< -o $@

$(OBJPREFIX)/%.o: %.cc | $(BUILD)
	$(GXX) $(GXX_FLAGS) $< -o $@

$(OBJPREFIX)/%.o: %.cxx | $(BUILD)
	$(GXX) $(GXX_FLAGS) $< -o $@

$(OBJPREFIX)/%.o: %.cp | $(BUILD)
	$(GXX) $(GXX_FLAGS) $< -o $@

$(OBJPREFIX)/%.o: %.c++ | $(BUILD)
	$(GXX) $(GXX_FLAGS) $< -o $@

$(SOURCES): makefile
	@touch $@

.PHONY: clean all

clean:
	rm -rf $(BUILD)

