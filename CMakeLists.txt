cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3 -O0 -Wall -Werror -Wno-sign-compare -Wno-unused-function -Wno-unused-variable")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Wall -Wno-sign-compare -Wno-unused-function -Wno-unused-variable")

include(FindPkgConfig)
set(CMAKE_FIND_FRAMEWORK LAST)

project(XcbWindowCapture VERSION 20220515.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

pkg_search_module(XCB REQUIRED xcb)
pkg_search_module(XCB_SHM REQUIRED xcb-shm)
pkg_search_module(AVDEVICE REQUIRED libavdevice)
pkg_search_module(AVFORMAT REQUIRED libavformat)
pkg_search_module(AVCODEC REQUIRED libavcodec)
pkg_search_module(AVSWSCALE REQUIRED libswscale)
pkg_search_module(AVUTIL REQUIRED libavutil)

add_executable(XcbWindowCapture main.cpp mainsettings.cpp xcbwrapper.cpp ffmpegencoder.cpp resources.qrc)

target_compile_options(XcbWindowCapture PUBLIC ${XCB_CFLAGS} ${XCB_SHM_CFLAGS})
target_compile_options(XcbWindowCapture PUBLIC ${AVDEVICE_CFLAGS} ${AVFORMAT_CFLAGS} ${AVCODEC_CFLAGS} ${AVSWSCALE_CFLAGS} ${AVUTIL_CFLAGS})

target_link_options(XcbWindowCapture PUBLIC ${AVDEVICE_LDFLAGS} ${AVFORMAT_LDFLAGS} ${AVCODEC_LDFLAGS} ${AVSWSCALE_LDFLAGS} ${AVUTIL_LDFLAGS})

target_link_libraries(XcbWindowCapture Qt5::Core Qt5::Gui Qt5::Widgets)
target_link_libraries(XcbWindowCapture ${XCB_LIBRARIES} ${XCB_SHM_LIBRARIES})
target_link_libraries(XcbWindowCapture ${AVDEVICE_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVSWSCALE_LIBRARIES} ${AVUTIL_LIBRARIES})

set_target_properties(XcbWindowCapture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
