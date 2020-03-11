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

project(audience)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#######################################################################
# AUDIENCE OPTIONS
#######################################################################

option(AUDIENCE_STATIC_RUNTIME "link static runtime (MSVC and GCC only)" $ENV{AUDIENCE_STATIC_RUNTIME})
option(AUDIENCE_INSTALL_RUNTIME "install shared runtime (MSVC only, ignored when using static runtime)" $ENV{AUDIENCE_INSTALL_RUNTIME})
option(AUDIENCE_VERBOSE_MAKEFILE "enable verbose command output" $ENV{AUDIENCE_VERBOSE_MAKEFILE})

#######################################################################
# AUDIENCE COMMON
#######################################################################

# verbose command output
if(AUDIENCE_VERBOSE_MAKEFILE)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# log configuration
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=$<IF:$<CONFIG:Debug>,0,3>)
add_compile_definitions(AUDIENCE_LOG_LEVEL_CONSOLE=$<IF:$<CONFIG:Debug>,0,4>)

# default is c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# msvc: link runtime statically
if(AUDIENCE_STATIC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES}")
  endif()
endif()

# msvc: defaults
if(WIN32)
  add_compile_options(/bigobj /utf-8 /wd4068)
  add_compile_definitions(NOMINMAX=1 _WIN32_WINNT=0x0601)
endif()

# symbols are hidden by default
if(NOT WIN32)
  add_compile_options(-fvisibility=hidden)
endif()

# public include directory
include_directories(include)

# rpath handling
SET(CMAKE_SKIP_BUILD_RPATH TRUE)

# strip binaries?
if(NOT WIN32 AND (CMAKE_BUILD_TYPE STREQUAL Release OR CMAKE_BUILD_TYPE STREQUAL MinSizeRel))
  SET(STRIP_BINARIES TRUE)
endif()

#######################################################################
# AUDIENCE DEPENDENCIES
#######################################################################

set(DEPS_DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/downloads)
set(DEPS_BUILD_ROOT_DIR ${CMAKE_BINARY_DIR}/deps_build)
set(DEPS_INSTALL_ROOT_DIR ${CMAKE_BINARY_DIR}/deps)

include(ExternalProject)

###################################
# Threading Library

if(UNIX)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)
endif()

###################################
# gabime/spdlog

# ... build
set(SPDLOG_ROOT ${DEPS_INSTALL_ROOT_DIR}/spdlog)
set(SPDLOG_INCLUDE_DIR ${SPDLOG_ROOT}/include)
set(SPDLOG_LIB_DIR ${SPDLOG_ROOT}/lib)
set(SPDLOG_LIB spdlog)

ExternalProject_Add(spdlog_build
  URL "https://github.com/gabime/spdlog/archive/v1.4.2.zip" DOWNLOAD_NAME "spdlog-v1.4.2.zip"
  URL_HASH SHA256=56b90f0bd5b126cf1b623eeb19bf4369516fa68f036bbc22d9729d2da511fb5a
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/spdlog
  INSTALL_DIR ${SPDLOG_ROOT}
  PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/deps/spdlog/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
  CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY} -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET} -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_BUILD_TESTS=OFF
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
)

file(MAKE_DIRECTORY ${SPDLOG_INCLUDE_DIR})

# ... import
add_library(spdlog STATIC IMPORTED)
add_dependencies(spdlog spdlog_build)
if(WIN32)
  set_target_properties(spdlog PROPERTIES IMPORTED_LOCATION ${SPDLOG_LIB_DIR}/${SPDLOG_LIB}.lib)
  set_target_properties(spdlog PROPERTIES IMPORTED_LOCATION_DEBUG ${SPDLOG_LIB_DIR}/${SPDLOG_LIB}d.lib)
else()
  set_target_properties(spdlog PROPERTIES IMPORTED_LOCATION ${SPDLOG_LIB_DIR}/lib${SPDLOG_LIB}.a)
  set_target_properties(spdlog PROPERTIES IMPORTED_LOCATION_DEBUG ${SPDLOG_LIB_DIR}/lib${SPDLOG_LIB}d.a)
endif()
target_include_directories(spdlog INTERFACE ${SPDLOG_INCLUDE_DIR})

###################################
# Boost

# ... build
set(BOOST_ROOT ${DEPS_INSTALL_ROOT_DIR}/boost)
set(BOOST_INCLUDE_DIR ${BOOST_ROOT}/include)

ExternalProject_Add(boost_build
  URL "https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.gz"
  URL_HASH SHA256=96b34f7468f26a141f6020efb813f1a2f3dfb9797ecf76a7d7cbd843cc95f5bd
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/boost
  INSTALL_DIR ${BOOST_ROOT}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/boost/ ${BOOST_INCLUDE_DIR}/boost/
)

file(MAKE_DIRECTORY ${BOOST_INCLUDE_DIR})

# ... import
add_library(boost INTERFACE IMPORTED)
add_dependencies(boost boost_build)
target_include_directories(boost INTERFACE ${BOOST_INCLUDE_DIR})
target_compile_definitions(boost INTERFACE BOOST_ALL_NO_LIB=1)

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

###################################
# libuv

# ... build
set(LIBUV_ROOT ${DEPS_INSTALL_ROOT_DIR}/libuv)
set(LIBUV_INCLUDE_DIR ${LIBUV_ROOT}/include)
set(LIBUV_LIB_DIR ${LIBUV_ROOT}/lib)
set(LIBUV_LIB uv_a)

ExternalProject_Add(libuv_build
  URL "https://github.com/libuv/libuv/archive/v1.33.1.tar.gz" DOWNLOAD_NAME "libuv-v1.33.1.tar.gz"
  URL_HASH SHA256=63794499bf5da1720e249d3fc14ff396b70b8736effe6ce5b4e47e0f3d476467
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/libuv
  INSTALL_DIR ${LIBUV_ROOT}
  PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/deps/libuv/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
  CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY} -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET} -DLIBUV_BUILD_TESTS=OFF
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
  INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
)

file(MAKE_DIRECTORY ${LIBUV_INCLUDE_DIR})

# ... import
add_library(libuv STATIC IMPORTED)
add_dependencies(libuv libuv_build)
if(WIN32)
  foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config_upper)
    set_target_properties(libuv PROPERTIES "IMPORTED_LOCATION_${config_upper}" ${LIBUV_LIB_DIR}/${config}/${LIBUV_LIB}.lib)
  endforeach()
else()
  set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${LIBUV_LIB_DIR}/lib${LIBUV_LIB}.a)
endif()
target_include_directories(libuv INTERFACE ${LIBUV_INCLUDE_DIR})

###################################
# skypjack/uvw

# ... build
set(UVW_ROOT ${DEPS_INSTALL_ROOT_DIR}/uvw)
set(UVW_INCLUDE_DIR ${UVW_ROOT}/include)

ExternalProject_Add(uvw_build
  URL "https://github.com/skypjack/uvw/archive/v2.2.0_libuv-v1.33.zip" DOWNLOAD_NAME "uvw-v2.2.0_libuv-v1.33.zip"
  URL_HASH SHA256=02ee78ffc251318dcd57c93f61d925e26fd1a74db7bedad47a54a27c8d90b59e
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/uvw
  INSTALL_DIR ${UVW_ROOT}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/ ${UVW_INCLUDE_DIR}/
)

file(MAKE_DIRECTORY ${UVW_INCLUDE_DIR})

# ... import
add_library(uvw INTERFACE IMPORTED)
add_dependencies(uvw uvw_build)
target_include_directories(uvw INTERFACE ${UVW_INCLUDE_DIR})
target_link_libraries (uvw INTERFACE libuv)

###################################
# jarro2783/cxxopts

# ... build
set(CXXOPTS_ROOT ${DEPS_INSTALL_ROOT_DIR}/cxxopts)
set(CXXOPTS_INCLUDE_DIR ${CXXOPTS_ROOT}/include)

ExternalProject_Add(cxxopts_build
  URL "https://github.com/jarro2783/cxxopts/archive/v2.2.0.zip" DOWNLOAD_NAME "cxxopts-v2.2.0.zip"
  URL_HASH SHA256=f9640c00d9938bedb291a21f9287902a3a8cee38db6910b905f8eba4a6416204
  DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
  PREFIX ${DEPS_BUILD_ROOT_DIR}/cxxopts
  INSTALL_DIR ${CXXOPTS_ROOT}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/include/ ${CXXOPTS_INCLUDE_DIR}/
)

file(MAKE_DIRECTORY ${CXXOPTS_INCLUDE_DIR})

# ... import
add_library(cxxopts INTERFACE IMPORTED)
add_dependencies(cxxopts cxxopts_build)
target_include_directories(cxxopts INTERFACE ${CXXOPTS_INCLUDE_DIR})

#######################################################################
# AUDIENCE FRONTEND LIBRARY
#######################################################################

set(AUDIENCE_FRONTEND_LIBRARY_CODE_CPP ${CMAKE_BINARY_DIR}/frontend-library-code.cpp)

file(READ ${CMAKE_SOURCE_DIR}/integrations/frontend/index.js AUDIENCE_FRONTEND_LIBRARY_HEX HEX)
string(REGEX REPLACE "(..)" "'\\\\x\\1', " AUDIENCE_FRONTEND_LIBRARY_INNERCODE "${AUDIENCE_FRONTEND_LIBRARY_HEX}")
file(WRITE ${AUDIENCE_FRONTEND_LIBRARY_CODE_CPP} "
#include <cstddef>
const char _audience_frontend_library_code[] = { ${AUDIENCE_FRONTEND_LIBRARY_INNERCODE} };
const char* _audience_frontend_library_code_begin = _audience_frontend_library_code;
std::size_t _audience_frontend_library_code_length = sizeof(_audience_frontend_library_code);
")

#######################################################################
# AUDIENCE SHELL
#######################################################################

# audience library
set(SHELL_LIB_SOURCES
  src/shell/lib/lib.cpp
  src/shell/lib/whereami.c
  src/shell/lib/webserver/process.cpp
  ${AUDIENCE_FRONTEND_LIBRARY_CODE_CPP}
)
set(SHELL_LIB_PUBLIC_HEADER
  include/audience_api.h
  include/audience_details.h
  include/audience.h
)

add_library(audience_static STATIC ${SHELL_LIB_SOURCES})
add_library(audience_shared SHARED ${SHELL_LIB_SOURCES})

foreach(audience_lib audience_static audience_shared)
  set_target_properties(${audience_lib} PROPERTIES PUBLIC_HEADER "${SHELL_LIB_PUBLIC_HEADER}")
  target_link_libraries(${audience_lib} PRIVATE spdlog boost)
  if(UNIX)
    target_link_libraries(${audience_lib} PRIVATE dl)
    target_link_libraries(${audience_lib} PUBLIC Threads::Threads)
  endif()
endforeach()

# ... static library specifics
target_compile_definitions(audience_static PUBLIC AUDIENCE_STATIC_LIBRARY=1)

# ... shared library specifics
if(STRIP_BINARIES)
  add_custom_command(TARGET audience_shared POST_BUILD COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:audience_shared>)
endif()

# audience app
add_executable(audience WIN32
  src/shell/app/main.cpp
  src/shell/app/channel.cpp
)
target_link_libraries(audience PRIVATE spdlog audience_static json uvw cxxopts)
if(WIN32)
  target_link_libraries(audience PRIVATE ws2_32 iphlpapi psapi userenv)
elseif(APPLE)
  target_link_libraries(audience PRIVATE "-framework CoreFoundation")
endif()
if(STRIP_BINARIES)
  add_custom_command(TARGET audience POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:audience>)
endif()
if(APPLE)
  find_program(INSTALL_NAME_TOOL NAMES install_name_tool)
  if(NOT INSTALL_NAME_TOOL)
    message(FATAL_ERROR "command 'install_name_tool' not found!")
  endif()
  add_custom_command(TARGET audience POST_BUILD COMMAND ${INSTALL_NAME_TOOL} -add_rpath @executable_path $<TARGET_FILE:audience>)
elseif(UNIX)
  find_program(PATCHELF NAMES patchelf)
  if(NOT PATCHELF)
    message(FATAL_ERROR "command 'patchelf' not found!")
  endif()
  add_custom_command(TARGET audience POST_BUILD COMMAND ${PATCHELF} --set-rpath '$$ORIGIN' $<TARGET_FILE:audience>)
endif()

#######################################################################
# AUDIENCE NUCLEUS
#######################################################################

if(WIN32)

  # windows: edge widget
  add_library(audience_windows_edge SHARED
    src/nucleus/windows/edge/nucleus.cpp
    src/nucleus/windows/edge/interface.cpp
    src/nucleus/windows/edge/security.cpp
    src/nucleus/windows/shared/load.cpp
    src/nucleus/windows/shared/icons.cpp
    ${AUDIENCE_FRONTEND_LIBRARY_CODE_CPP}
  )
  target_compile_options(audience_windows_edge PRIVATE "/await")
  target_include_directories(audience_windows_edge PRIVATE src/nucleus/windows/edge)
  target_link_libraries(audience_windows_edge PRIVATE spdlog boost WindowsApp.lib gdiplus.lib)

  # windows: ie11 widget
  add_library(audience_windows_ie11 SHARED
    src/nucleus/windows/ie11/nucleus.cpp
    src/nucleus/windows/ie11/webview.cpp
    src/nucleus/windows/ie11/interface.cpp
    src/nucleus/windows/shared/load.cpp
    src/nucleus/windows/shared/icons.cpp
  )
  target_include_directories(audience_windows_ie11 PRIVATE src/nucleus/windows/ie11)
  target_link_libraries(audience_windows_ie11 PRIVATE spdlog boost comsuppw.lib gdiplus.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED
    src/nucleus/macos/webkit/nucleus.mm
    src/nucleus/macos/webkit/interface.mm
  )
  target_compile_options(audience_macos_webkit PRIVATE "-fobjc-arc")
  target_include_directories(audience_macos_webkit PRIVATE src/nucleus/macos/webkit)
  target_link_libraries(audience_macos_webkit PRIVATE spdlog boost "-framework CoreFoundation" "-framework Cocoa" "-framework WebKit")
  if(STRIP_BINARIES)
    add_custom_command(TARGET audience_macos_webkit POST_BUILD COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:audience_macos_webkit>)
  endif()

elseif(UNIX)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED IMPORTED_TARGET webkit2gtk-4.0)

  # unix/linux: webkit widget
  add_library(audience_unix_webkit SHARED
    src/nucleus/unix/webkit/nucleus.cpp
    src/nucleus/unix/webkit/interface.cpp
  )
  target_include_directories(audience_unix_webkit PRIVATE src/nucleus/unix/webkit)
  target_link_libraries(audience_unix_webkit PRIVATE spdlog boost PkgConfig::GTK3 PkgConfig::WEBKIT2 Threads::Threads)
  if(STRIP_BINARIES)
    add_custom_command(TARGET audience_unix_webkit POST_BUILD COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:audience_unix_webkit>)
  endif()

endif()

#######################################################################
# AUDIENCE DIST
#######################################################################

get_directory_property(all_targets BUILDSYSTEM_TARGETS)
foreach(some_target ${all_targets})
  if(some_target MATCHES "^audience")
    install(TARGETS ${some_target}
      RUNTIME DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
      LIBRARY DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin
      ARCHIVE DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/lib
      PRIVATE_HEADER DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/include
      PUBLIC_HEADER DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/include
    )
  endif()
endforeach()

if(NOT AUDIENCE_STATIC_RUNTIME AND AUDIENCE_INSTALL_RUNTIME)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION \${CMAKE_INSTALL_CONFIG_NAME}/bin)
  include(InstallRequiredSystemLibraries)
endif()
