
set(MY_SOURCES test.cpp)

add_executable(test_pow ${MY_SOURCES})
target_link_libraries(test_pow
        PRIVATE pow)

add_test(NAME test_pow COMMAND test_pow)