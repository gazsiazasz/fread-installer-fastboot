cmake_minimum_required(VERSION 3.0)
project(fread_installer_fastboot C)

set(CMAKE_C_STANDARD 11)


if(UNIX AND APPLE)
    set(PLATFORM osx)
elseif(UNIX)
    set(PLATFORM linux)
else()
    set(PLATFORM windows)
endif()


set(CORE_SOURCES
        engine.c
        fastboot.c
        protocol.c)

set(PLATFORM_SOURCES
        usb_${PLATFORM}.c
        util_${PLATFORM}.c)

add_executable(fread_installer_fastboot ${CORE_SOURCES} ${PLATFORM_SOURCES})

if(APPLE)
    target_link_libraries(fread_installer_fastboot "-framework CoreFoundation -framework IOKit -framework Carbon")
endif()
