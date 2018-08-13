
set(MY_SOURCES test.cpp)

file(GLOB ${FC_HEADERS} ../../fc/include/fc/*.hpp)

add_executable(test_pow ${MY_SOURCES})
target_link_libraries(test_pow pow fc)
target_include_directories(test_pow PUBLIC
        ${FC_HEADERS})

add_test(NAME test_pow COMMAND test_pow)