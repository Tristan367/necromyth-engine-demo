cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(VulkanCppApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(VCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Vulkan-C-Engine" CACHE PATH "Vulkan C++ Engine repository root")
if(NOT EXISTS "${VCE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "VCE_ROOT does not point to Vulkan-C-Engine: ${VCE_ROOT}")
endif()

if(NOT VULKAN_SDK_ROOT AND DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK root directory")
endif()

add_subdirectory("${VCE_ROOT}" "${CMAKE_BINARY_DIR}/vce")

add_executable(VulkanCppApp
    src/main.cpp
    src/app.cpp
    src/demo_scene.cpp
)

target_link_libraries(VulkanCppApp PRIVATE VCE::Engine)

target_compile_definitions(VulkanCppApp PRIVATE
    APP_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    APP_VCE_ASSETS="${VCE_ROOT}/assets"
)

set_target_properties(VulkanCppApp PROPERTIES
    DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(_run_env_vars "")
if(VULKAN_SDK_ROOT AND IS_DIRECTORY "${VULKAN_SDK_ROOT}/lib")
    list(APPEND _run_env_vars "VULKAN_SDK=${VULKAN_SDK_ROOT}")
    list(APPEND _run_env_vars "LD_LIBRARY_PATH=${VULKAN_SDK_ROOT}/lib")
    list(APPEND _run_env_vars "VK_ADD_LAYER_PATH=${VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
endif()

add_custom_target(run
    COMMAND "${CMAKE_COMMAND}" -E env ${_run_env_vars} "$<TARGET_FILE:VulkanCppApp>"
    DEPENDS VulkanCppApp
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)
