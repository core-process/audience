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

add_compile_definitions(AUDIENCE_ENABLE_TRACE=$<CONFIG:Debug>)
add_compile_definitions(BOOST_ALL_NO_LIB=1)

if(AUDIENCE_STATIC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(WIN32)
  add_compile_options(/bigobj)
endif()

include_directories(include)

#######################################################################
# AUDIENCE DEPENDENCIES
#######################################################################

include(ExternalProject)

ExternalProject_Add(boost
  URL "https://dl.bintray.com/boostorg/release/1.71.0/source/boost_1_71_0.tar.gz"
  URL_HASH SHA256=96b34f7468f26a141f6020efb813f1a2f3dfb9797ecf76a7d7cbd843cc95f5bd
  DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/downloads
  PREFIX ${CMAKE_BINARY_DIR}/deps_build/boost
  INSTALL_DIR ${CMAKE_BINARY_DIR}/deps/boost
  BUILD_IN_SOURCE 1
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND cmake -E copy_directory ${CMAKE_BINARY_DIR}/deps_build/boost/src/boost/boost/ ${CMAKE_BINARY_DIR}/deps/boost/boost/
)

#######################################################################
# AUDIENCE SHELL
#######################################################################

# audience library
add_library(audience STATIC
  src/shell/lib.cpp
  src/shell/whereami.c
  src/shell/webserver/process.cpp
)
set_target_properties(audience PROPERTIES CXX_STANDARD 11)

add_dependencies(audience boost)
target_include_directories(audience PRIVATE ${CMAKE_BINARY_DIR}/deps/boost)

if(UNIX)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)

  target_link_libraries(audience PRIVATE dl)
  target_link_libraries(audience PUBLIC Threads::Threads)
endif()

# audience app
add_executable(audienceapp WIN32
  src/shell/app.cpp
)
set_target_properties(audienceapp PROPERTIES CXX_STANDARD 11)
target_link_libraries (audienceapp PRIVATE audience)

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
  )
  set_target_properties(audience_windows_edge PROPERTIES CXX_STANDARD 17)
  target_include_directories(audience_windows_edge PRIVATE src/nucleus/windows/edge)
  target_compile_options(audience_windows_edge PRIVATE "/await")
  target_link_libraries(audience_windows_edge PRIVATE WindowsApp.lib)

  # windows: ie11 widget
  add_library(audience_windows_ie11 SHARED
    src/nucleus/windows/ie11/nucleus.cpp
    src/nucleus/windows/ie11/webview.cpp
    src/nucleus/windows/ie11/interface.cpp
    src/nucleus/windows/shared/init.cpp
    src/nucleus/windows/shared/resource.rc
  )
  set_target_properties(audience_windows_ie11 PROPERTIES CXX_STANDARD 11)
  target_include_directories(audience_windows_ie11 PRIVATE src/nucleus/windows/ie11)
  target_link_libraries(audience_windows_ie11 PRIVATE comsuppw.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED
    src/nucleus/macos/webkit/nucleus.mm
    src/nucleus/macos/webkit/interface.mm
  )
  set_target_properties(audience_macos_webkit PROPERTIES CXX_STANDARD 11)
  target_include_directories(audience_macos_webkit PRIVATE src/nucleus/macos/webkit)
  target_compile_options(audience_macos_webkit PRIVATE "-fobjc-arc")
  target_link_libraries(audience_macos_webkit PRIVATE "-framework CoreFoundation" "-framework Cocoa" "-framework WebKit")

elseif(UNIX)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)

  # unix/linux: webkit widget
  add_library(audience_unix_webkit SHARED
    src/nucleus/unix/webkit/nucleus.cpp
    src/nucleus/unix/webkit/interface.cpp
  )
  set_target_properties(audience_unix_webkit PROPERTIES CXX_STANDARD 11)
  target_include_directories(audience_unix_webkit PRIVATE src/nucleus/unix/webkit)
  target_include_directories(audience_unix_webkit PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS})
  target_link_libraries(audience_unix_webkit PRIVATE ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES} Threads::Threads)

endif()
