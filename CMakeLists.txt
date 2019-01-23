cmake_minimum_required(VERSION 3.13)
project(qnet)

set(CMAKE_CXX_STANDARD 14)



add_executable(nextctapudp
        src/nextctapudp/ctapudp.c
        src/nextctapudp/keyqueue.c
        src/nextctapudp/keyqueue.h
        src/nextctapudp/post_curl.c
        src/nextctapudp/post_curl.h
        src/nextctapudp/sha.c
        src/nextctapudp/sha.h
        src/nextctapudp/thpool.c
        src/nextctapudp/thpool.h)

add_executable(keyworker
        src/keyworker/keyqueue.c
        src/keyworker/keyqueue.h
        src/keyworker/keyworker.c
        src/keyworker/sha.c
        src/keyworker/sha.h)

TARGET_LINK_LIBRARIES(nextctapudp
        pthread
        curl
        z
        m
        crypto)


TARGET_LINK_LIBRARIES(keyworker
        pthread
        microhttpd
        db
        crypto)