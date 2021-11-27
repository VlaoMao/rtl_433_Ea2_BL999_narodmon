cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 14)

include_directories(serial)

add_executable (rtl_ea2_narodmon rtl.cc sockets.c serial/SerialPort.cpp)
target_link_libraries (rtl_ea2_narodmon pthread)
install (TARGETS rtl_ea2_narodmon RUNTIME DESTINATION bin)
