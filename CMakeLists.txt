cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -ggdb3 -O0 -Wall -Werror -Wno-sign-compare -Wno-unused-function -Wno-unused-variable")
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
pkg_search_module(XCB_XFIXES REQUIRED xcb-xfixes)
pkg_search_module(AVDEVICE REQUIRED libavdevice)
pkg_search_module(AVFORMAT REQUIRED libavformat)
pkg_search_module(AVCODEC REQUIRED libavcodec)
pkg_search_module(AVSWSCALE REQUIRED libswscale)
pkg_search_module(AVSWRESAMPLE REQUIRED libswresample)
pkg_search_module(AVUTIL REQUIRED libavutil)

add_executable(XcbWindowCapture main.cpp mainsettings.cpp xcbwrapper.cpp ffmpegencoder.cpp resources.qrc)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(Boost_USE_MULTITHREADED ON)

    target_compile_options(XcbWindowCapture PUBLIC -DBUILD_DEBUG)
    find_package(Boost 1.69 OPTIONAL_COMPONENTS stacktrace_addr2line)

    if(Boost_STACKTRACE_ADDR2LINE_FOUND)
        target_compile_options(XcbWindowCapture PUBLIC -DBOOST_STACKTRACE_USE)
        target_compile_options(XcbWindowCapture PUBLIC ${Boost_STACKTRACE_ADDR2LINE_CFLAGS})
        target_link_libraries(XcbWindowCapture ${Boost_STACKTRACE_ADDR2LINE_LIBRARIES})
        target_link_libraries(XcbWindowCapture dl)
    endif()

endif()

target_compile_options(XcbWindowCapture PUBLIC ${XCB_CFLAGS} ${XCB_SHM_CFLAGS} ${XCB_XFIXES_CFLAGS} )
target_compile_options(XcbWindowCapture PUBLIC ${AVDEVICE_CFLAGS} ${AVFORMAT_CFLAGS} ${AVCODEC_CFLAGS} ${AVSWSCALE_CFLAGS} ${AVSWRESAMPLE_CFLAGS} ${AVUTIL_CFLAGS})
target_link_options(XcbWindowCapture PUBLIC ${AVDEVICE_LDFLAGS} ${AVFORMAT_LDFLAGS} ${AVCODEC_LDFLAGS} ${AVSWSCALE_LDFLAGS} ${AVSWRESAMPLE_LDFLAGS} ${AVUTIL_LDFLAGS})

target_link_libraries(XcbWindowCapture Qt5::Core Qt5::Gui Qt5::Widgets)
target_link_libraries(XcbWindowCapture ${XCB_LIBRARIES} ${XCB_SHM_LIBRARIES} ${XCB_XFIXES_LIBRARIES})
target_link_libraries(XcbWindowCapture ${AVDEVICE_LIBRARIES} ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES} ${AVSWSCALE_LIBRARIES} ${AVSWRESAMPLE_LIBRARIES} ${AVUTIL_LIBRARIES})

set_target_properties(XcbWindowCapture PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
