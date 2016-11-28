cmake_minimum_required(VERSION 3.6)
project(wiser)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("./include")

set(SOURCE_FILES
    include/utarray.h
    include/uthash.h
    include/utlist.h
    include/utstring.h
    database.c
    database.h
    postings.c
    postings.h
    search.c
    search.h
    token.c
    token.h
    util.c
    util.h
    wikiload.c
    wikiload.h
    wiser.c
    wiser.h)

add_executable(wiser ${SOURCE_FILES})

TARGET_LINK_LIBRARIES(wiser sqlite3)
TARGET_LINK_LIBRARIES(wiser expat)
TARGET_LINK_LIBRARIES(wiser m)