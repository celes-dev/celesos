
file(GLOB MY_HEADERS "include/celesos/pow/*.h")

set(MY_SOURCES
        ethash.cpp)

add_library(pow ${MY_SOURCES} ${MY_HEADERS})

target_link_libraries(pow PUBLIC
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES})

target_include_directories(pow PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPENSSL_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS})

add_subdirectory(test)