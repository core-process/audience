cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
project(audience)

#######################################################################
# AUDIENCE OPTIONS
#######################################################################

option(AUDIENCE_STATIC_RUNTIME "link static runtime (MSVC only)" ON)

#######################################################################
# AUDIENCE COMMON
#######################################################################

# enable traces for debug
add_compile_definitions(AUDIENCE_ENABLE_TRACE=$<CONFIG:Debug>)

# default is c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# msvc: link runtime statically
if(AUDIENCE_STATIC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# msvc: support big object files
if(WIN32)
  add_compile_options(/bigobj)
endif()

# symbols are hidden by default
if(NOT WIN32)
  add_compile_options(-fvisibility=hidden)
endif()

# public include directory
include_directories(include)

# rpath handling
SET(CMAKE_SKIP_BUILD_RPATH TRUE)

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

# ###################################
# # zlib

# # ... build
# set(ZLIB_ROOT ${DEPS_INSTALL_ROOT_DIR}/zlib)
# set(ZLIB_INCLUDE_DIR ${ZLIB_ROOT}/include)
# set(ZLIB_LIB_DIR ${ZLIB_ROOT}/lib)
# set(ZLIB_LIB z)

# ExternalProject_Add(zlib_build
#   URL "https://zlib.net/zlib-1.2.11.tar.gz"
#   URL_HASH SHA256=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1
#   DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
#   PREFIX ${DEPS_BUILD_ROOT_DIR}/zlib
#   INSTALL_DIR ${ZLIB_ROOT}
#   CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
#   BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
#   INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
# )

# file(MAKE_DIRECTORY ${ZLIB_INCLUDE_DIR})

# # ... import
# add_library(zlib STATIC IMPORTED)
# add_dependencies(zlib zlib_build)
# if(WIN32)
#   set_target_properties(zlib PROPERTIES IMPORTED_LOCATION ${ZLIB_LIB_DIR}/${ZLIB_LIB}.lib)
# else()
#   set_target_properties(zlib PROPERTIES IMPORTED_LOCATION ${ZLIB_LIB_DIR}/lib${ZLIB_LIB}.a)
# endif()
# target_include_directories(zlib INTERFACE ${ZLIB_INCLUDE_DIR})

# ###################################
# # libuv

# # ... build
# set(LIBUV_ROOT ${DEPS_INSTALL_ROOT_DIR}/libuv)
# set(LIBUV_INCLUDE_DIR ${LIBUV_ROOT}/include)
# set(LIBUV_LIB_DIR ${LIBUV_ROOT}/lib)
# set(LIBUV_LIB uv_a)

# ExternalProject_Add(libuv_build
#   URL "https://github.com/libuv/libuv/archive/v1.33.1.tar.gz" DOWNLOAD_NAME "libuv-v1.33.1.tar.gz"
#   URL_HASH SHA256=63794499bf5da1720e249d3fc14ff396b70b8736effe6ce5b4e47e0f3d476467
#   DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
#   PREFIX ${DEPS_BUILD_ROOT_DIR}/libuv
#   INSTALL_DIR ${LIBUV_ROOT}
#   CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY} -DLIBUV_BUILD_TESTS=OFF
#   BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
#   INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
# )

# file(MAKE_DIRECTORY ${LIBUV_INCLUDE_DIR})

# # ... import
# add_library(libuv STATIC IMPORTED)
# add_dependencies(libuv libuv_build)
# if(WIN32)
#   set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${LIBUV_LIB_DIR}/${LIBUV_LIB}.lib)
# else()
#   set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${LIBUV_LIB_DIR}/lib${LIBUV_LIB}.a)
# endif()
# target_include_directories(libuv INTERFACE ${LIBUV_INCLUDE_DIR})

# ###################################
# # uSockets

# # ... build
# set(USOCKETS_ROOT ${DEPS_INSTALL_ROOT_DIR}/usockets)
# set(USOCKETS_INCLUDE_DIR ${USOCKETS_ROOT}/include)
# set(USOCKETS_LIB_DIR ${USOCKETS_ROOT}/lib)
# set(USOCKETS_LIB usockets)

# ExternalProject_Add(usockets_build
#   URL "https://github.com/uNetworking/uSockets/archive/v0.3.2.tar.gz" DOWNLOAD_NAME "usockets-v0.3.2.tar.gz"
#   URL_HASH SHA256=2bd33157cd7359f143673c94de87d10aac167392ab52bae7c7464d6d2cbb3da7
#   DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
#   PREFIX ${DEPS_BUILD_ROOT_DIR}/usockets
#   INSTALL_DIR ${USOCKETS_ROOT}
#   PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/deps/usockets/CMakeLists.txt <SOURCE_DIR>/CMakeLists.txt
#   CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY} -DLIBUV_INCLUDE_DIR=${LIBUV_INCLUDE_DIR} -DLIBUV_LIB_DIR=${LIBUV_LIB_DIR} -DLIBUV_LIB=${LIBUV_LIB}
#   BUILD_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG>
#   INSTALL_COMMAND ${CMAKE_COMMAND} --build . --config $<CONFIG> --target install
# )
# add_dependencies(usockets_build libuv_build)

# file(MAKE_DIRECTORY ${USOCKETS_INCLUDE_DIR})

# # ... import
# add_library(usockets STATIC IMPORTED)
# add_dependencies(usockets usockets_build)
# if(WIN32)
#   set_target_properties(usockets PROPERTIES IMPORTED_LOCATION ${USOCKETS_LIB_DIR}/${USOCKETS_LIB}.lib)
# else()
#   set_target_properties(usockets PROPERTIES IMPORTED_LOCATION ${USOCKETS_LIB_DIR}/lib${USOCKETS_LIB}.a)
# endif()
# target_include_directories(usockets INTERFACE ${USOCKETS_INCLUDE_DIR})
# target_link_libraries(usockets INTERFACE libuv)

# ###################################
# # uWebSockets

# # ... import
# set(UWEBSOCKETS_ROOT ${DEPS_INSTALL_ROOT_DIR}/uwebsockets)
# set(UWEBSOCKETS_INCLUDE_DIR ${UWEBSOCKETS_ROOT}/include)

# ExternalProject_Add(uwebsockets_build
#   URL "https://github.com/uNetworking/uWebSockets/archive/v0.16.1.tar.gz" DOWNLOAD_NAME "uwebsockets-v0.16.1.tar.gz"
#   URL_HASH SHA256=a6f6cc4c7a761294b7b7dd8978714727e39f94734a957c15d6896405099af562
#   DOWNLOAD_DIR ${DEPS_DOWNLOAD_DIR}
#   PREFIX ${DEPS_BUILD_ROOT_DIR}/uwebsockets
#   INSTALL_DIR ${UWEBSOCKETS_ROOT}
#   CONFIGURE_COMMAND ""
#   BUILD_COMMAND ""
#   INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/src/ ${UWEBSOCKETS_INCLUDE_DIR}/uwebsockets/
# )

# file(MAKE_DIRECTORY ${UWEBSOCKETS_INCLUDE_DIR})

# # ... import
# add_library(uwebsockets INTERFACE IMPORTED)
# add_dependencies(uwebsockets uwebsockets_build)
# target_include_directories(uwebsockets INTERFACE ${UWEBSOCKETS_INCLUDE_DIR})
# target_link_libraries(uwebsockets INTERFACE usockets zlib)

#######################################################################
# AUDIENCE WEBAPP MESSAGING
#######################################################################

file(READ ${CMAKE_SOURCE_DIR}/src/webapp/messaging/websocket.js AUDIENCE_WEBAPP_MESSAGING_WEBSOCKET_HEX HEX)
string(REGEX REPLACE "(..)" "'\\\\x\\1', " AUDIENCE_WEBAPP_MESSAGING_WEBSOCKET_HEXLIT "${AUDIENCE_WEBAPP_MESSAGING_WEBSOCKET_HEX}")
file(WRITE ${CMAKE_SOURCE_DIR}/src/webapp/messaging/websocket.cpp "
#include <cstddef>
const char _audience_webapp_messaging_websocket_code[] = { ${AUDIENCE_WEBAPP_MESSAGING_WEBSOCKET_HEXLIT} };
const char* _audience_webapp_messaging_websocket_code_begin = _audience_webapp_messaging_websocket_code;
std::size_t _audience_webapp_messaging_websocket_code_length = sizeof(_audience_webapp_messaging_websocket_code);
")

file(READ ${CMAKE_SOURCE_DIR}/src/webapp/messaging/edge.js AUDIENCE_WEBAPP_MESSAGING_EDGE_HEX HEX)
string(REGEX REPLACE "(..)" "'\\\\x\\1', " AUDIENCE_WEBAPP_MESSAGING_EDGE_HEXLIT "${AUDIENCE_WEBAPP_MESSAGING_EDGE_HEX}")
file(WRITE ${CMAKE_SOURCE_DIR}/src/webapp/messaging/edge.cpp "
#include <cstddef>
const char _audience_webapp_messaging_edge_code[] = { ${AUDIENCE_WEBAPP_MESSAGING_EDGE_HEXLIT} };
const char* _audience_webapp_messaging_edge_code_begin = _audience_webapp_messaging_edge_code;
std::size_t _audience_webapp_messaging_edge_code_length = sizeof(_audience_webapp_messaging_edge_code);
")

#######################################################################
# AUDIENCE SHELL
#######################################################################

# audience library
set(SHELL_LIB_SOURCES
  src/shell/lib/lib.cpp
  src/shell/lib/whereami.c
  src/shell/lib/webserver/process.cpp
  src/webapp/messaging/websocket.cpp
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
  target_link_libraries(${audience_lib} PRIVATE boost)
  if(UNIX)
    target_link_libraries(${audience_lib} PRIVATE dl)
    target_link_libraries(${audience_lib} PUBLIC Threads::Threads)
  endif()
endforeach()

# ... static library specifics
target_compile_definitions(audience_static PUBLIC AUDIENCE_STATIC_LIBRARY=1)

# ... shared library specifics
if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
  add_custom_command(TARGET audience_shared POST_BUILD COMMAND ${CMAKE_STRIP} -x $<TARGET_FILE:audience_shared>)
endif()

# audience app
add_executable(audience WIN32
  src/shell/app/main.cpp
  src/shell/app/quotes.cpp
)
target_link_libraries (audience PRIVATE audience_shared)
if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
  add_custom_command(TARGET audience POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:audience>)
endif()
if(APPLE)
  add_custom_command(TARGET audience POST_BUILD COMMAND install_name_tool -add_rpath @executable_path $<TARGET_FILE:audience>)
endif()

#######################################################################
# AUDIENCE NUCLEUS
#######################################################################

if(WIN32)

  # windows: edge widget
  add_library(audience_windows_edge SHARED
    src/nucleus/windows/edge/nucleus.cpp
    src/nucleus/windows/edge/interface.cpp
    src/nucleus/windows/shared/init.cpp
    src/nucleus/windows/shared/resource.rc
    src/webapp/messaging/edge.cpp
  )
  set_target_properties(audience_windows_edge PROPERTIES CXX_STANDARD 17)
  target_compile_options(audience_windows_edge PRIVATE "/await")
  target_include_directories(audience_windows_edge PRIVATE src/nucleus/windows/edge)
  target_link_libraries(audience_windows_edge PRIVATE boost WindowsApp.lib)

  # windows: ie11 widget
  add_library(audience_windows_ie11 SHARED
    src/nucleus/windows/ie11/nucleus.cpp
    src/nucleus/windows/ie11/webview.cpp
    src/nucleus/windows/ie11/interface.cpp
    src/nucleus/windows/shared/init.cpp
    src/nucleus/windows/shared/resource.rc
  )
  target_include_directories(audience_windows_ie11 PRIVATE src/nucleus/windows/ie11)
  target_link_libraries(audience_windows_ie11 PRIVATE boost comsuppw.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED
    src/nucleus/macos/webkit/nucleus.mm
    src/nucleus/macos/webkit/interface.mm
  )
  target_compile_options(audience_macos_webkit PRIVATE "-fobjc-arc")
  target_include_directories(audience_macos_webkit PRIVATE src/nucleus/macos/webkit)
  target_link_libraries(audience_macos_webkit PRIVATE boost "-framework CoreFoundation" "-framework Cocoa" "-framework WebKit")
  if(CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
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
  target_link_libraries(audience_unix_webkit PRIVATE boost PkgConfig::GTK3 PkgConfig::WEBKIT2 Threads::Threads)
  if(CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
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
