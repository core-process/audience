#######################################################################
# CMAKE VERSION REQUIREMENTS
#######################################################################

cmake_minimum_required(VERSION 3.15)
cmake_policy(VERSION 3.15)

#######################################################################
# PRE-PROJECT SETTINGS
#######################################################################

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "Minimum OS X deployment version")

#######################################################################
# PROJECT HEADER AND INCLUDE PATHS
#######################################################################

project(audience_example_ping)

#######################################################################
# USER SETTINGS
#######################################################################

set(AUDIENCE_VERSION            $ENV{AUDIENCE_VERSION}            CACHE STRING "Audience version"            FORCE)
set(USE_STATIC_CRT              $ENV{USE_STATIC_CRT}              CACHE BOOL   "Use static CRT"              FORCE)
set(USE_STATIC_AUDIENCE_LIBRARY $ENV{USE_STATIC_AUDIENCE_LIBRARY} CACHE BOOL   "Use static Audience library" FORCE)
set(USE_AUDIENCE_DEBUG          $ENV{USE_AUDIENCE_DEBUG}          CACHE STRING "Use Audience debug variant"  FORCE)

if(AUDIENCE_VERSION STREQUAL "")
  message(FATAL_ERROR "AUDIENCE_VERSION missing, please export AUDIENCE_VERSION=x.x.x")
endif()

#######################################################################
# COMMON SETTINGS
#######################################################################

if(USE_STATIC_CRT)
  if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  elseif(LINUX)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES}")
  endif()
endif()

# default is c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# rpath handling
SET(CMAKE_SKIP_BUILD_RPATH TRUE)

#######################################################################
# DEPENDENCIES
#######################################################################

set(DEPS_DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/downloads)
set(DEPS_BUILD_ROOT_DIR ${CMAKE_BINARY_DIR}/deps_build)
set(DEPS_INSTALL_ROOT_DIR ${CMAKE_BINARY_DIR}/deps)

include(ExternalProject)

###################################
# threads

if(UNIX)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)
endif()

###################################
# audience

# ... download and extract
if(USE_AUDIENCE_DEBUG)
  set(AUDIENCE_VARIANT debug)
else()
  set(AUDIENCE_VARIANT release-minsize)
endif()

if(WIN32)
  if(USE_STATIC_CRT)
    set(AUDIENCE_ZIP_NAME audience-v${AUDIENCE_VERSION}-windows-scrt-x64-${AUDIENCE_VARIANT}.zip)
  else()
    set(AUDIENCE_ZIP_NAME audience-v${AUDIENCE_VERSION}-windows-dcrt-x64-${AUDIENCE_VARIANT}.zip)
  endif()
elseif(APPLE)
  set(AUDIENCE_ZIP_NAME audience-v${AUDIENCE_VERSION}-macos-x64-${AUDIENCE_VARIANT}.tar.gz)
elseif(LINUX)
  if(USE_STATIC_CRT)
    set(AUDIENCE_ZIP_NAME audience-v${AUDIENCE_VERSION}-linux-scpprt-x64-${AUDIENCE_VARIANT}.tar.gz)
  else()
    set(AUDIENCE_ZIP_NAME audience-v${AUDIENCE_VERSION}-linux-dcpprt-x64-${AUDIENCE_VARIANT}.tar.gz)
  endif()
else()
  message(FATAL_ERROR "No pre-built Audience binaries available")
endif()

set(AUDIENCE_ZIP_URL https://github.com/core-process/audience/releases/download/v${AUDIENCE_VERSION}/${AUDIENCE_ZIP_NAME})
set(AUDIENCE_ZIP_PATH ${DEPS_DOWNLOAD_DIR}/${AUDIENCE_ZIP_NAME})
set(AUDIENCE_DIR ${DEPS_INSTALL_ROOT_DIR}/audience/${AUDIENCE_VERSION}-${AUDIENCE_VARIANT})

if(NOT EXISTS ${AUDIENCE_DIR})
  if(NOT EXISTS ${AUDIENCE_ZIP_PATH})
    file(DOWNLOAD ${AUDIENCE_ZIP_URL} ${AUDIENCE_ZIP_PATH} SHOW_PROGRESS)
  endif()
  file(MAKE_DIRECTORY ${AUDIENCE_DIR})
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar -vxzf ${AUDIENCE_ZIP_PATH} WORKING_DIRECTORY ${AUDIENCE_DIR})
endif()

# ... import
add_library(audience STATIC IMPORTED)
target_include_directories(audience INTERFACE ${AUDIENCE_DIR}/include)
if(USE_STATIC_AUDIENCE_LIBRARY)
  target_compile_definitions(audience INTERFACE AUDIENCE_STATIC_LIBRARY=1)
  if(WIN32)
    set_target_properties(audience PROPERTIES IMPORTED_LOCATION ${AUDIENCE_DIR}/lib/audience_static.lib)
  else()
    set_target_properties(audience PROPERTIES IMPORTED_LOCATION ${AUDIENCE_DIR}/lib/libaudience_static.a)
  endif()
else()
  if(WIN32)
    set_target_properties(audience PROPERTIES IMPORTED_LOCATION ${AUDIENCE_DIR}/lib/audience_shared.lib)
  elseif(APPLE)
    set_target_properties(audience PROPERTIES IMPORTED_LOCATION ${AUDIENCE_DIR}/bin/libaudience_shared.dylib)
  else()
    set_target_properties(audience PROPERTIES IMPORTED_LOCATION ${AUDIENCE_DIR}/bin/libaudience_shared.so)
  endif()
endif()

###################################
# nlohmann/json

# ... build
set(JSON_ROOT ${DEPS_INSTALL_ROOT_DIR}/json)
set(JSON_INCLUDE_DIR ${JSON_ROOT}/include)

ExternalProject_Add(json_build
  URL "https://github.com/nlohmann/json/releases/download/v3.7.0/json.hpp" DOWNLOAD_NAME "json-v3.7.0.hpp"
  URL_HASH SHA256=a503214947952b69f0062f572cb74c17582a495767446347ce2e452963fc2ca4
  DOWNLOAD_NO_EXTRACT ON
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/json
  INSTALL_DIR ${JSON_ROOT}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <DOWNLOADED_FILE> ${JSON_INCLUDE_DIR}/json.hpp
)

file(MAKE_DIRECTORY ${JSON_INCLUDE_DIR})

# ... import
add_library(json INTERFACE IMPORTED)
add_dependencies(json json_build)
target_include_directories(json INTERFACE ${JSON_INCLUDE_DIR})

#######################################################################
# EXAMPLE BACKEND
#######################################################################

# source files
add_executable(example main.cpp ping.cpp)

# libraries
target_link_libraries(example PRIVATE audience json)
if(UNIX)
  target_link_libraries(example PRIVATE Threads::Threads)
elseif(WIN32)
  target_link_libraries(example PRIVATE ws2_32)
endif()

# strip binary
if(NOT WIN32 AND (CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL MinSizeRel))
  add_custom_command(TARGET example POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:example>)
endif()

# set runpath
if(APPLE)
  find_program(INSTALL_NAME_TOOL NAMES install_name_tool)
  if(NOT INSTALL_NAME_TOOL)
    message(FATAL_ERROR "command 'install_name_tool' not found!")
  endif()
  add_custom_command(TARGET example POST_BUILD COMMAND ${INSTALL_NAME_TOOL} -add_rpath @executable_path $<TARGET_FILE:example>)
elseif(UNIX)
  find_program(PATCHELF NAMES patchelf)
  if(NOT PATCHELF)
    message(FATAL_ERROR "command 'patchelf' not found!")
  endif()
  add_custom_command(TARGET example POST_BUILD COMMAND ${PATCHELF} --set-rpath '$$ORIGIN' $<TARGET_FILE:example>)
endif()

#######################################################################
# AUDIENCE DIST
#######################################################################

install(TARGETS example
  RUNTIME DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
  LIBRARY DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
  ARCHIVE DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/lib
  PRIVATE_HEADER DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/include
  PUBLIC_HEADER DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/include
)

if(USE_STATIC_AUDIENCE_LIBRARY)
  install(DIRECTORY ${AUDIENCE_DIR}/bin/
    DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
    FILES_MATCHING PATTERN "*.dll" PATTERN "*.dylib" PATTERN "*.so"
                   PATTERN "*audience_shared.*" EXCLUDE
  )
else()
  install(DIRECTORY ${AUDIENCE_DIR}/bin/
    DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
    FILES_MATCHING PATTERN "*.dll" PATTERN "*.dylib" PATTERN "*.so"
  )
endif()
