cmake_minimum_required(VERSION 3.26)
project(Praktikum1 C)

set(CMAKE_C_STANDARD 11)

add_executable(Praktikum1
        udp_examples/echo_client.c
        udp_examples/echo_server.c
        udp_examples/Socket.h
        udp_examples/Socket.c)
