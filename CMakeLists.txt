cmake_minimum_required(VERSION 3.16)

project(noodles VERSION 0.2 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# hack for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
# end hack for clangd


# CMake Modules ================================================================

include(cmake/CPM.cmake)

CPMAddPackage(
  NAME GroupSourcesByFolder.cmake
  GITHUB_REPOSITORY TheLartians/GroupSourcesByFolder.cmake
  VERSION 1.0
)

CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.6.0")

option(NOO_LOCAL_GLM "Use a local version of GLM instead of using CPM" OFF)

if (NOT NOO_LOCAL_GLM)
    CPMAddPackage(
        NAME glm
        URL  https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.7z
        VERSION 0.9.9.8
    )
endif()

CPMAddPackage(
    NAME magic_enum
    GITHUB_REPOSITORY Neargye/magic_enum
    GIT_TAG v0.8.0
)


# Options ======================================================================

set(SANITIZER "none" CACHE STRING "Build with a given sanitizer")
set_property(CACHE SANITIZER PROPERTY STRINGS none address)

SET(sanitizer_compile_flag "")

if (${SANITIZER} STREQUAL "address")
    SET(sanitizer_compile_flag "-fsanitize=address")
endif()

# Set Up =======================================================================

# Server Lib
add_library(noodles SHARED "")
target_compile_features(noodles PUBLIC cxx_std_20)
target_compile_options(noodles PUBLIC ${sanitizer_compile_flag})
target_link_options(noodles PUBLIC ${sanitizer_compile_flag})

target_include_directories(noodles PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# Because packagers like having fun
target_link_libraries(noodles PRIVATE magic_enum)
if (NOT NOO_LOCAL_GLM)
    target_link_libraries(noodles PRIVATE glm)
ENDIF()


find_package(Qt6 COMPONENTS Gui Widgets Core Websockets)

if (NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Gui Widgets Core WebSockets)
    # universal targets were not added till 5.15
    target_link_libraries(noodles PUBLIC Qt5::Core Qt5::WebSockets Qt5::Gui)
else()
    target_link_libraries(noodles PUBLIC Qt::Core Qt::WebSockets Qt::Gui)
endif()

add_subdirectory(src)

# We have to do this for the group-by-folders module
# We cant have a cmakelists in the include dir as that appears to be installed
target_sources(noodles
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_interface_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_interface_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_client_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_client_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_common.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_id.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_include_glm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_server_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/noo_server_interface.h
)

#get_target_property(NDBG noodles SOURCES)

#message(${NDBG})

# Files ========================================================================

GroupSourcesByFolder(noodles)

# Install ======================================================================

string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

packageProject(
  NAME ${PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  NAMESPACE ${PROJECT_NAME}


  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"

  EXPORT_HEADER "${PROJECT_NAME}/export.h"

  DEPENDENCIES "glm 0.9.9.8"
)
