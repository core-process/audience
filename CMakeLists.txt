cmake_minimum_required(VERSION 3.12)
project(audience)

add_compile_definitions(ENABLE_TRACE)

#######################################################################
# AUDIENCE SHELL
#######################################################################

# audience library
add_library(audience SHARED
  src/shell/lib.cpp
  src/shell/whereami.c
)
set_target_properties(audience PROPERTIES CXX_STANDARD 11)
target_include_directories(audience PUBLIC include)

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
    src/nucleus/windows/edge.cpp
    src/nucleus/windows/common.cpp
    src/nucleus/windows/resource.rc
  )
  set_target_properties(audience_windows_edge PROPERTIES CXX_STANDARD 17)
  target_link_libraries(audience_windows_edge PRIVATE WindowsApp.lib)

  # windows: ie11 widget
  add_library(audience_windows_ie11 SHARED
    src/nucleus/windows/ie11.cpp
    src/nucleus/windows/ie11_webview.cpp
    src/nucleus/windows/common.cpp
    src/nucleus/windows/resource.rc
  )
  set_target_properties(audience_windows_ie11 PROPERTIES CXX_STANDARD 11)
  target_link_libraries(audience_windows_ie11 PRIVATE comsuppw.lib)

elseif(APPLE)

  # macos: webkit widget
  add_library(audience_macos_webkit SHARED
    src/nucleus/macos/webkit.mm
  )
  set_target_properties(audience_macos_webkit PROPERTIES CXX_STANDARD 11)
  target_compile_options(audience_macos_webkit PRIVATE "-fobjc-arc")
  target_link_libraries(audience_macos_webkit PRIVATE "-framework CoreFoundation" "-framework Cocoa" "-framework WebKit")

elseif(UNIX)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)

  # unix/linux: webkit widget
  add_library(audience_unix_webkit SHARED
    src/nucleus/unix/webkit.cpp
  )
  set_target_properties(audience_unix_webkit PROPERTIES CXX_STANDARD 11)
  target_include_directories(audience_unix_webkit PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS})
  target_link_libraries(audience_unix_webkit PRIVATE ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES})
  target_link_libraries(audience_unix_webkit PUBLIC Threads::Threads)

endif()
