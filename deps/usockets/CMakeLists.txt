cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
project(usockets)

include(GNUInstallDirs)

FILE(GLOB USOCKETS_SOURCES src/*.c)
FILE(GLOB USOCKETS_SOURCES_EVENTING src/eventing/*.c)

add_compile_definitions(
  LIBUS_NO_SSL=1
  LIBUS_USE_LIBUV=1
)

add_library(usockets STATIC
  ${USOCKETS_SOURCES}
  ${USOCKETS_SOURCES_EVENTING}
)

target_include_directories(usockets PRIVATE src ${LIBUV_INCLUDE_DIR})
target_link_directories(usockets PUBLIC ${LIBUV_LIB_DIR})
target_link_libraries(usockets PUBLIC ${LIBUV_LIB})

install(FILES src/libusockets.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS usockets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
