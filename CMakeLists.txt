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
        receiver.c
        receiver.h
        sender2.c
        sender2.h
        server
        Server.c
        tester.c)

add_executable(prog_iiw ${SOURCE_FILES})