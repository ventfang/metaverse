
FILE(GLOB_RECURSE mvs-tools_SOURCES "*.cpp")

ADD_DEFINITIONS(-DBCX_STATIC=1)

ADD_EXECUTABLE(mvs-tools ${mvs-tools_SOURCES})

# for experimental/string_view
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

IF(ENABLE_SHARED_LIBS)
    TARGET_LINK_LIBRARIES(mvs-tools ${bitcoin_LIBRARY} ${database_LIBRARY})
ELSE()
    TARGET_LINK_LIBRARIES(mvs-tools ${bitcoin_LIBRARY} ${database_LIBRARY})
ENDIF()

INSTALL(TARGETS mvs-tools DESTINATION bin)
