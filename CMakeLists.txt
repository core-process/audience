cmake_minimum_required(VERSION 3.15)
project(audience)

add_compile_definitions(ENABLE_TRACE)

add_library(audience STATIC src/lib.cpp)

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
  target_link_libraries(audience_windows_ie11 PUBLIC comsuppw.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED src/macos/webkit.cpp)

elseif(UNIX)

  # unix/linux: webkit widget
  add_library(audience_unix_webkit SHARED src/unix/webkit.cpp)

else()
  message(FATAL_ERROR "System not supported!")
endif()

add_executable(audienceapp WIN32 src/app.cpp)
target_link_libraries (audienceapp audience)
