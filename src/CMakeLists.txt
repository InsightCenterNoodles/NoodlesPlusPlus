cmake_minimum_required(VERSION 3.12...3.16)

project(Noodles VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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

#find_package(glm REQUIRED)

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

# Common Lib
add_library(noodlesCommon STATIC "")
setup_library_target(noodlesCommon)
add_subdirectory(shared)
target_link_libraries(noodlesCommon PUBLIC Qt::Core)

# Server Lib
add_library(noodlesServer SHARED "")
setup_library_target(noodlesServer)
add_subdirectory(server_lib)


target_link_libraries(noodlesServer PRIVATE noodlesCommon)
target_link_libraries(noodlesServer PUBLIC Qt::WebSockets Qt::Core)

# Client Lib
add_library(noodlesClient SHARED "")
setup_library_target(noodlesClient)
add_subdirectory(client_lib)

target_link_libraries(noodlesClient PRIVATE noodlesCommon)
target_link_libraries(noodlesClient PUBLIC Qt::Widgets Qt::WebSockets Qt::Core)

# Server Example
add_executable(exampleServer "")
setup_exe_target(exampleServer)
add_subdirectory(example_server)
target_include_directories(exampleServer PRIVATE ${CMAKE_SOURCE_DIR}/server_lib)
target_link_libraries(exampleServer PRIVATE noodlesServer)
target_link_libraries(exampleServer PUBLIC Qt::Gui Qt::Core)

# Client Example
add_executable(exampleClient "")
setup_exe_target(exampleClient)
add_subdirectory(example_client)
target_include_directories(exampleClient PRIVATE ${CMAKE_SOURCE_DIR}/client_lib)
target_link_libraries(exampleClient PRIVATE noodlesClient)
target_link_libraries(exampleClient PUBLIC
    Qt::Gui Qt::Widgets Qt::Core
    Qt::3DCore Qt::3DExtras Qt::3DRender
)

# QML Client
add_executable(NoodlesViewer "")
setup_exe_target(NoodlesViewer)
add_subdirectory(qml_client)
target_include_directories(NoodlesViewer PRIVATE ${CMAKE_SOURCE_DIR}/client_lib)
target_link_libraries(NoodlesViewer PRIVATE noodlesClient)
target_link_libraries(NoodlesViewer PUBLIC
    Qt::Gui Qt::Widgets Qt::Core
    Qt::3DCore Qt::3DExtras Qt::3DRender
    Qt::Quick
)



# Files ========================================================================

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

GroupSourcesByFolder(noodlesCommon)

GroupSourcesByFolder(noodlesServer)
GroupSourcesByFolder(noodlesClient)

GroupSourcesByFolder(exampleServer)
GroupSourcesByFolder(exampleClient)
GroupSourcesByFolder(NoodlesViewer)

