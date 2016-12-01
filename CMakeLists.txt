cmake_minimum_required(VERSION 3.6)
project(wiser)



set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("src/wiser/include")

set(SOURCE_FILES
    src/wiser/include/utarray.h
    src/wiser/include/uthash.h
    src/wiser/include/utlist.h
    src/wiser/include/utstring.h
    src/wiser/database.c
    src/wiser/database.h
    src/wiser/postings.c
    src/wiser/postings.h
    src/wiser/search.c
    src/wiser/search.h
    src/wiser/token.c
    src/wiser/token.h
    src/wiser/util.c
    src/wiser/util.h
    src/wiser/wikiload.c
    src/wiser/wikiload.h
    src/wiser/wiser.c
    src/wiser/wiser.h)

add_executable(wiser ${SOURCE_FILES})

TARGET_LINK_LIBRARIES(wiser sqlite3)
TARGET_LINK_LIBRARIES(wiser expat)
TARGET_LINK_LIBRARIES(wiser m)