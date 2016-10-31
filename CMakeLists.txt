cmake_minimum_required (VERSION 2.8)
project (socketChat)


IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    MESSAGE(STATUS "Building with gcc-4.9 since your using a mac and that makes things hard")
    set(CMAKE_CXX_COMPILER "g++-4.9")
ENDIF()


IF(${CMAKE_SYSTEM_NAME} MATCHES "linux")
    MESSAGE(STATUS "Good choice in operating system Sr. continue doing what your doing")
ENDIF()



set(CMAKE_CXX_FLAGS "--std=c++11 -pthread -Werror")


add_executable(server src/server.cpp)
add_executable(server2 src/server2.cpp)
add_executable(client src/client.cpp)
add_executable(client2 src/client2.cpp)