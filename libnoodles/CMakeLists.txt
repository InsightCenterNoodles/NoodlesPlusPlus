cmake_minimum_required(VERSION 3.12...3.16)

project(libnoodles VERSION 0.1 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_EX_CLIENT "Build example client executable" ON)
option(BUILD_EX_SERVER "Build example server executable" ON)

# CMake Modules ================================================================

include(cmake/CPM.cmake)

CPMAddPackage(
  NAME GroupSourcesByFolder.cmake
  GITHUB_REPOSITORY TheLartians/GroupSourcesByFolder.cmake
  VERSION 1.0
)

CPMAddPackage(
  NAME flatbuffers
  URL https://github.com/google/flatbuffers/archive/v1.12.0.zip
  VERSION 1.12.0
  OPTIONS
    "FLATBUFFERS_CODE_COVERAGE OFF"
    "FLATBUFFERS_BUILD_TESTS OFF"
    "FLATBUFFERS_BUILD_FLATLIB OFF"
    "FLATBUFFERS_BUILD_FLATC OFF"
    "FLATBUFFERS_BUILD_FLATHASH OFF"
    "FLATBUFFERS_LIBCXX_WITH_CLANG ON"
)

if (flatbuffers_ADDED)
  add_library(flatbuffers INTERFACE IMPORTED)
  target_include_directories(flatbuffers INTERFACE ${flatbuffers_SOURCE_DIR}/include)
endif()

CPMAddPackage(
    NAME glm
    URL  https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.7z
    VERSION 0.9.9.8
)

find_package(Qt5 COMPONENTS Gui Widgets WebSockets Core 3DCore 3DExtras 3DRender Quick)

# Options ======================================================================

set(SANITIZER "none" CACHE STRING "Build with a given sanitizer")
set_property(CACHE SANITIZER PROPERTY STRINGS none address)

SET(sanitizer_compile_flag "")

if (${SANITIZER} STREQUAL "address")
    SET(sanitizer_compile_flag "-fsanitize=address")
endif()

macro(setup_exe_target t)
    target_compile_features(${t} PUBLIC cxx_std_20)
    target_compile_options(${t} PUBLIC ${sanitizer_compile_flag})
    target_link_options(${t} PUBLIC ${sanitizer_compile_flag})

    if(LINUX)
        target_compile_options(${t} PUBLIC -stdlib=libc++)
        target_link_options(${t} PUBLIC -stdlib=libc++)
    endif()

    # Because packagers like having fun
    target_link_libraries(${t} PRIVATE glm)
endmacro()

macro(setup_library_target t)
    setup_exe_target(${t})
    target_link_libraries(${t} PRIVATE flatbuffers)
endmacro()

# Set Up =======================================================================

# Server Lib
add_library(noodles SHARED "")
setup_library_target(noodles)
target_link_libraries(noodles PUBLIC Qt::Core Qt::WebSockets)

add_subdirectory(common)
add_subdirectory(generated)
add_subdirectory(include)
add_subdirectory(client)

# Files ========================================================================

GroupSourcesByFolder(noodles)
