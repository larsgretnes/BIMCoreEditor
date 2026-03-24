cmake_minimum_required(VERSION 3.20)
project(BIMCoreEditor VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(BIM_ENABLE_DEBUG_LOG)

# Force binaries to land in build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# 0. LOCAL SDK CONFIGURATION (WINDOWS RUNTIME DLLS)
# ==============================================================================
if(WIN32)
    set(BIMCORE_SDK_BIN_DIR "C:/dev/BIMCore_SDK/bin" CACHE PATH "Path to the SDK bin directory containing runtime DLLs")
    message(STATUS "[BIMCore] Windows Runtime DLL directory set to: ${BIMCORE_SDK_BIN_DIR}")
endif()

# ==============================================================================
# 1. PLATFORM DETECTION
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

# ==============================================================================
# 2. SYSTEM PACKAGES & ROBUST TARGET CREATION
# ==============================================================================
find_package(glfw3 QUIET)
if(NOT TARGET glfw)
    if(TARGET glfw3)
        add_library(glfw ALIAS glfw3)
    else()
        find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h)
        find_library(GLFW_LIB NAMES glfw3dll glfw3 glfw)
        if(GLFW_LIB AND GLFW_INCLUDE_DIR)
            add_library(glfw UNKNOWN IMPORTED GLOBAL)
            set_target_properties(glfw PROPERTIES
                IMPORTED_LOCATION "${GLFW_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR "[BIMCore] Could NOT find GLFW libraries/headers! Check CMAKE_PREFIX_PATH.")
        endif()
    endif()
endif()

find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "[BIMCore] Could NOT find GLM headers! Check CMAKE_PREFIX_PATH.")
endif()

find_package(IfcOpenShell QUIET)
if(NOT IfcOpenShell_FOUND)
    find_path(IFC_INCLUDE_DIR NAMES ifcparse/IfcFile.h)
    find_path(OCC_INCLUDE_DIR NAMES Standard.hxx PATH_SUFFIXES opencascade)
    find_path(EIGEN_INCLUDE_DIR NAMES Eigen/Dense PATH_SUFFIXES eigen3)
    find_library(IFCPARSE_LIB NAMES IfcParse)

    if(IFC_INCLUDE_DIR AND IFCPARSE_LIB)
        set(MISSING_HEADER "${IFC_INCLUDE_DIR}/ifcgeom/kernels/ifc_geomlibrary_api.h")
        if(NOT EXISTS "${MISSING_HEADER}")
            file(WRITE "${MISSING_HEADER}" "#pragma once\n#ifndef IFC_GEOMLIBRARY_API\n#define IFC_GEOMLIBRARY_API\n#endif\n")
        endif()

        set(IFCGEOM_INCLUDES "${IFC_INCLUDE_DIR}")
        if(OCC_INCLUDE_DIR)
            list(APPEND IFCGEOM_INCLUDES "${OCC_INCLUDE_DIR}")
        endif()
        if(EIGEN_INCLUDE_DIR)
            list(APPEND IFCGEOM_INCLUDES "${EIGEN_INCLUDE_DIR}")
        endif()

        get_filename_component(IFC_LIB_DIR "${IFCPARSE_LIB}" DIRECTORY)
        file(GLOB ALL_SDK_LIBS "${IFC_LIB_DIR}/*.lib")

        add_library(IfcOpenShell_All INTERFACE)
        target_include_directories(IfcOpenShell_All INTERFACE "${IFCGEOM_INCLUDES}")
        target_link_directories(IfcOpenShell_All INTERFACE "${IFC_LIB_DIR}")
        target_link_libraries(IfcOpenShell_All INTERFACE ${ALL_SDK_LIBS})

        add_library(IfcOpenShell::IfcParse ALIAS IfcOpenShell_All)
        add_library(IfcOpenShell::IfcGeom ALIAS IfcOpenShell_All)
    else()
        message(FATAL_ERROR "Could NOT find IfcOpenShell manually! Check CMAKE_PREFIX_PATH.")
    endif()
endif()

# ==============================================================================
# 3. FETCH VENDOR DEPENDENCIES
# ==============================================================================
include(FetchContent)

set(TINYGLTF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(webgpu GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git GIT_TAG main)
FetchContent_Declare(glfw3webgpu GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu.git GIT_TAG main)
FetchContent_Declare(imgui_fetch GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG master)

FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
FetchContent_Declare(tinygltf GIT_REPOSITORY https://github.com/syoyo/tinygltf.git GIT_TAG v2.8.3)
FetchContent_Declare(mini GIT_REPOSITORY https://github.com/pulzed/mINI.git GIT_TAG 0.9.14)

# --- NEW: Fetch ImGuizmo ---
FetchContent_Declare(imguizmo GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git GIT_TAG master)

# Make sure imguizmo is added to this list!
FetchContent_MakeAvailable(webgpu glfw3webgpu imgui_fetch nlohmann_json tinygltf mini imguizmo)

if(TARGET tinygltf AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tinygltf PRIVATE -w)
endif()

# ==============================================================================
# 4. IMGUI MANUAL TARGET SETUP
# ==============================================================================
set(IMGUI_DIR ${imgui_fetch_SOURCE_DIR})
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
    # --- NEW: Compile ImGuizmo right into the ImGui library ---
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp 
)

# --- NEW: Add the ImGuizmo folder to the include directories ---
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${imguizmo_SOURCE_DIR} 
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
target_link_libraries(imgui PUBLIC webgpu glfw)

# ==============================================================================
# 5. THE EDITOR APPLICATION (Merged SDK & App)
# ==============================================================================
set(APP_SOURCES
    src/core/main.cpp
    src/core/EditorApp.cpp
    src/core/EngineConfig.cpp
    src/core/CommandHistory.cpp
    src/ui/AppUI.cpp
    src/ui/UIMainPanel.cpp
    src/ui/UIPropertiesPanel.cpp
    src/ui/UIStatusOverlay.cpp
    src/input/InputController.cpp
    src/graphics/GraphicsContext.cpp
    src/graphics/Camera.cpp
    src/scene/SceneModel.cpp
    src/scene/Raycaster.cpp
    src/io/IfcLoader.cpp
    src/io/IfcExporter.cpp
    src/io/GltfImporter.cpp
    src/io/GltfExporter.cpp
    src/io/BcfImporter.cpp
    src/io/CsvImporter.cpp
    src/platform/Window.cpp
)

add_executable(BIMCoreEditor ${APP_SOURCES})

# Target Include Directories
target_include_directories(BIMCoreEditor PRIVATE 
    src 
    "${GLM_INCLUDE_DIR}" 
    "${mini_SOURCE_DIR}/src"
)
# Add third-party headers as SYSTEM to suppress their warnings
target_include_directories(BIMCoreEditor SYSTEM PRIVATE "${tinygltf_SOURCE_DIR}")

# Link libraries
target_link_libraries(BIMCoreEditor PRIVATE 
    imgui glfw webgpu glfw3webgpu
    IfcOpenShell::IfcParse IfcOpenShell::IfcGeom
    nlohmann_json::nlohmann_json tinygltf
)

if(UNIX AND NOT APPLE)
    target_link_libraries(BIMCoreEditor PRIVATE X11 dl pthread)
endif()

# ==============================================================================
# 6. ASSET & RUNTIME DEPLOYMENT (Pristine Dist Folder)
# ==============================================================================
set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist")

add_custom_command(TARGET BIMCoreEditor POST_BUILD
    # Create the pristine folder
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"

    # Copy the application and assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BIMCoreEditor>" "${DIST_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/fa-solid-900.ttf" "${DIST_DIR}/"

    COMMENT "Publishing pristine build to /dist folder..."
)

# OS-Specific WebGPU Native Library Name
if(WIN32)
    set(WGPU_NAME "wgpu_native.dll")
elseif(APPLE)
    set(WGPU_NAME "libwgpu_native.dylib")
else()
    set(WGPU_NAME "libwgpu_native.so")
endif()

# Safely copy WebGPU if found
file(GLOB_RECURSE WGPU_LIB "${CMAKE_BINARY_DIR}/_deps/*/${WGPU_NAME}")
if(WGPU_LIB)
    add_custom_command(TARGET BIMCoreEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WGPU_LIB} "${DIST_DIR}/"
    )
endif()

# Copy ONLY the heavy SDK DLLs we actually need (Windows Only)
if(WIN32)
    file(GLOB RUNTIME_DLLS
        "${BIMCORE_SDK_BIN_DIR}/Ifc*.dll"
        "${BIMCORE_SDK_BIN_DIR}/TK*.dll"
        "${BIMCORE_SDK_BIN_DIR}/glfw*.dll"
    )

    if(RUNTIME_DLLS)
        add_custom_command(TARGET BIMCoreEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RUNTIME_DLLS} "${DIST_DIR}/"
        )
    endif()
endif()