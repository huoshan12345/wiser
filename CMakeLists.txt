cmake_minimum_required(VERSION 3.6)
project(wiser)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("src/include")

set(SOURCE_FILES
    src/include/utarray.h
    src/include/uthash.h
    src/include/utlist.h
    src/include/utstring.h
    src/database.c
    src/database.h
    src/postings.c
    src/postings.h
    src/search.c
    src/search.h
    src/token.c
    src/token.h
    src/util.c
    src/util.h
    src/wikiload.c
    src/wikiload.h
    src/wiser.c
    src/wiser.h)

add_executable(wiser ${SOURCE_FILES})

TARGET_LINK_LIBRARIES(wiser sqlite3)
TARGET_LINK_LIBRARIES(wiser expat)
TARGET_LINK_LIBRARIES(wiser m)