cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(NecromythDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT VCE_ROOT)
    set(VCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../necromyth-engine")
endif()
set(VCE_ROOT "${VCE_ROOT}" CACHE PATH "Necromyth Engine repository root")
if(NOT EXISTS "${VCE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "VCE_ROOT does not point to Necromyth Engine: ${VCE_ROOT}")
endif()

if(NOT VULKAN_SDK_ROOT AND DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK root directory")
endif()

add_subdirectory("${VCE_ROOT}" "${CMAKE_BINARY_DIR}/vce")

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(app_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(app_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(app_imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)
target_compile_definitions(app_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_executable(NecromythDemo
    src/main.cpp
    src/app.cpp
    src/demo_scene.cpp
    src/demo_assets.cpp
    src/demo_anim.cpp
    src/debug_ui.cpp
    "$ENV{HOME}/opt/JoltPhysics/Jolt/Renderer/DebugRenderer.cpp"
)

target_compile_definitions(NecromythDemo PRIVATE JPH_DEBUG_RENDERER)

target_link_libraries(NecromythDemo PRIVATE VCE::Engine app_imgui)

target_compile_definitions(NecromythDemo PRIVATE
    APP_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

set_target_properties(NecromythDemo PROPERTIES
    DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

set(_run_env_vars "")
if(VULKAN_SDK_ROOT AND IS_DIRECTORY "${VULKAN_SDK_ROOT}/lib")
    list(APPEND _run_env_vars "VULKAN_SDK=${VULKAN_SDK_ROOT}")
    list(APPEND _run_env_vars "LD_LIBRARY_PATH=${VULKAN_SDK_ROOT}/lib")
    list(APPEND _run_env_vars "VK_ADD_LAYER_PATH=${VULKAN_SDK_ROOT}/share/vulkan/explicit_layer.d")
endif()

add_custom_target(run
    COMMAND "${CMAKE_COMMAND}" -E env ${_run_env_vars} "$<TARGET_FILE:NecromythDemo>"
    DEPENDS NecromythDemo
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    USES_TERMINAL
)
