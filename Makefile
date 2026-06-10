.PHONY: all configure build run clean debug release

BUILD_TYPE ?= Release
VULKAN_SDK_ROOT ?= $(HOME)/opt/vulkan-sdk/default/x86_64
VCE_ROOT ?= ../Vulkan-C-Engine

# "make debug" or "make debug run" -> build-Debug; "make" / "make run" -> build-Release.
ifneq (,$(filter debug,$(MAKECMDGOALS)))
  BUILD_TYPE := Debug
endif

BUILD_DIR := build-$(BUILD_TYPE)
APP := $(BUILD_DIR)/bin/VulkanCppApp

all: build

release:
	@$(MAKE) BUILD_TYPE=Release build

configure:
	cmake -S . -B "$(BUILD_DIR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
		-DVULKAN_SDK_ROOT="$(VULKAN_SDK_ROOT)" \
		-DVCE_ROOT="$(VCE_ROOT)"

build: configure
	cmake --build "$(BUILD_DIR)" -j$$(nproc)

debug:
	@$(MAKE) BUILD_TYPE=Debug build

run: build
	cmake --build "$(BUILD_DIR)" --target run -j$$(nproc)

clean:
	rm -rf build build-Debug build-Release
