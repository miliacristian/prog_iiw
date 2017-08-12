cmake_minimum_required(VERSION 3.8)
project(prog_iiw)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        cmake-build-debug/cmake_install.cmake
        cmake-build-debug/CMakeCache.txt
        cmake-build-debug/Makefile
        cmake-build-debug/prog_iiw.cbp
        basic.c
        basic.h
        Client.c
        CMakeLists.txt
        file_lock.c
        file_lock.h
        io.c
        io.h
        lock_fcntl.c
        main.c
        makefile
        parser.c
        parser.h
        tester.c
        Server.c
        timer.c timer.h get_client.c get_server.c Client.h Server.h get_client.h get_server.h communication.c communication.h list_client.c list_server.c list_server.h list_client.h put_client.c put_server.c put_server.h put_client.h)

add_executable(prog_iiw ${SOURCE_FILES})