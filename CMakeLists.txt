cmake_minimum_required(VERSION 3.13)
project(audience)

add_compile_definitions(ENABLE_TRACE)

add_library(audience STATIC src/lib.cpp src/whereami.c)
set_property(TARGET audience PROPERTY CXX_STANDARD 11)

if(UNIX)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)

  target_link_libraries(audience PUBLIC dl Threads::Threads)
endif()

if(WIN32)

  if(NOT MSVC)
    message(FATAL_ERROR "MSVC required on Windows systems!")
  endif()

  # windows: edge widget
  add_library(audience_windows_edge SHARED src/windows/edge.cpp src/windows/common.cpp src/windows/resource.rc)
  set_property(TARGET audience_windows_edge PROPERTY CXX_STANDARD 17)
  target_link_libraries(audience_windows_edge PUBLIC WindowsApp.lib)

  # windows: ie11 widget
  add_library(audience_windows_ie11 SHARED src/windows/ie11.cpp src/windows/ie11_webview.cpp src/windows/common.cpp src/windows/resource.rc)
  set_property(TARGET audience_windows_ie11 PROPERTY CXX_STANDARD 11)
  target_link_libraries(audience_windows_ie11 PUBLIC comsuppw.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED src/macos/webkit.mm)
  set_property(TARGET audience_macos_webkit PROPERTY CXX_STANDARD 11)
  target_compile_options(audience_macos_webkit PUBLIC "-fobjc-arc")
  target_link_libraries(audience_macos_webkit PUBLIC "-framework CoreFoundation" "-framework Cocoa" "-framework WebKit")

elseif(UNIX)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)

  # unix/linux: webkit widget
  add_library(audience_unix_webkit SHARED src/unix/webkit.cpp)
  set_property(TARGET audience_unix_webkit PROPERTY CXX_STANDARD 11)
  target_include_directories(audience_unix_webkit PUBLIC ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS})
  target_link_libraries(audience_unix_webkit PUBLIC Threads::Threads ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES})

else()
  message(FATAL_ERROR "System not supported!")
endif()

add_executable(audienceapp WIN32 src/app.cpp)
set_property(TARGET audienceapp PROPERTY CXX_STANDARD 11)
target_link_libraries (audienceapp PUBLIC audience)
