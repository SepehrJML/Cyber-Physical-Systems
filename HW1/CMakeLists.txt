cmake_minimum_required(VERSION 3.19)
project(CPS4042 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_executable(CPS4042
    include/CPS4042/main.h
    src/main.cpp
    

    include/CPS4042/Protocols/Protocol.h
    include/CPS4042/Wires/Link.h
    include/CPS4042/Wires/Pin.h
    include/CPS4042/Hardwares/Transmitter.h
    include/CPS4042/Units/Bit.h
    include/CPS4042/Units/Byte.h
    include/CPS4042/Units/Voltage.h
    include/CPS4042/Utils/Queue.h
    include/CPS4042/Hardwares/Board.h
    include/CPS4042/Hardwares/Processor.h
    include/CPS4042/Units/Frequency.h
    include/CPS4042/Units/BaudRate.h
    include/CPS4042/Hardwares/Boards/Esp8266.h
    include/CPS4042/Hardwares/Sensors/VL530X.h
    include/CPS4042/Hardwares/Comm/I2CMux.h
    include/CPS4042/Globals/Object.h
    include/CPS4042/Sketchs/AbstractSketch.h

    include/CPS4042/Sketchs/Sensor.h
    include/CPS4042/Sketchs/Microcontroller.h
    include/CPS4042/Utils/Wave.h
    include/CPS4042/Utils/ByteStream.h

    include/CPS4042/Sketchs/HardDisk.h
)

target_include_directories(CPS4042
    PRIVATE
        ./include

        D:/University/CPS/HW1/boost_build_mingw/install/include/boost-1_84
)

target_link_directories(CPS4042
    PRIVATE
        D:/University/CPS/HW1/boost_build_mingw/install/lib
)


include(GNUInstallDirs)

install(TARGETS CPS4042
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
