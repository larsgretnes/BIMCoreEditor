cmake_minimum_required(VERSION 3.20)
project(BIMCoreEditor VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(BIM_ENABLE_DEBUG_LOG)

# Force binaries to land in build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# 0. PLATFORM DETECTION (CRITICAL FOR WEBGPU)
# ==============================================================================
if(WIN32)
    message(STATUS "[BIMCore] Target platform: Windows")
    add_compile_definitions(BIM_PLATFORM_WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX VC_EXTRA_LEAN)
elseif(APPLE)
    message(STATUS "[BIMCore] Target platform: macOS")
    add_compile_definitions(BIM_PLATFORM_MACOS)
elseif(UNIX)
    message(STATUS "[BIMCore] Target platform: Linux")
    add_compile_definitions(BIM_PLATFORM_LINUX)
endif()

include(FetchContent)

# ==============================================================================
# 1. FETCH VENDOR DEPENDENCIES
# ==============================================================================
# WebGPU
FetchContent_Declare(webgpu GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git GIT_TAG main)
FetchContent_MakeAvailable(webgpu)

# GLFW WebGPU Bridge
FetchContent_Declare(glfw3webgpu GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu.git GIT_TAG main)
FetchContent_MakeAvailable(glfw3webgpu)

# Dear ImGui (Fetches the raw repository directly from GitHub)
FetchContent_Declare(imgui_fetch GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG master)
FetchContent_MakeAvailable(imgui_fetch)

# ==============================================================================
# 2. SYSTEM PACKAGES
# ==============================================================================
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(IfcOpenShell REQUIRED)

# ==============================================================================
# 3. IMGUI MANUAL TARGET SETUP
# ==============================================================================
set(IMGUI_DIR ${imgui_fetch_SOURCE_DIR})

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
target_link_libraries(imgui PUBLIC webgpu glfw)

# ==============================================================================
# 4. BIMCORE SDK LIBRARY
# ==============================================================================
set(SDK_SOURCES
    sdk/src/graphics/GraphicsContext.cpp
    sdk/src/platform/Window.cpp
    sdk/src/scene/Camera.cpp
    sdk/src/scene/Raycaster.cpp
    sdk/src/scene/BimDocument.cpp
    sdk/src/scene/IfcLoader.cpp
    sdk/src/scene/IfcExporter.cpp
)

add_library(BimCore_SDK STATIC ${SDK_SOURCES})
target_include_directories(BimCore_SDK PUBLIC sdk/src)

target_link_libraries(BimCore_SDK PUBLIC
    imgui
    glfw
    glm
    webgpu
    glfw3webgpu
    IfcOpenShell::IfcParse
    IfcOpenShell::IfcGeom
)

# ==============================================================================
# 5. THE EDITOR APPLICATION
# ==============================================================================
set(EDITOR_SOURCES
    apps/editor/main.cpp
    apps/editor/AppUI.cpp
    apps/editor/InputController.cpp
)

add_executable(BIMCoreEditor ${EDITOR_SOURCES})
target_include_directories(BIMCoreEditor PRIVATE apps/editor sdk/src)
target_link_libraries(BIMCoreEditor PRIVATE BimCore_SDK)

if(UNIX AND NOT APPLE)
    target_link_libraries(BIMCoreEditor PRIVATE X11 dl pthread)
endif()

# ==============================================================================
# 6. ASSET DEPLOYMENT (Dynamic Post-Build Copy)
# ==============================================================================
# This guarantees that EVERY time you run 'make', any changes you made to
# apps/editor/settings.ini will be instantly pushed to build/bin/settings.ini
add_custom_command(TARGET BIMCoreEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/fa-solid-900.ttf"
        "$<TARGET_FILE_DIR:BIMCoreEditor>/fa-solid-900.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/apps/editor/settings.ini"
        "$<TARGET_FILE_DIR:BIMCoreEditor>/settings.ini"
    COMMENT "Copying assets (fonts and settings) to output directory..."
)
