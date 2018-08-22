
set(MY_SOURCES test.cpp)

add_executable(test_miner_plugin ${MY_SOURCES})
target_link_libraries(test_miner_plugin
        PRIVATE eosio_testing
        PRIVATE pow
        PRIVATE miner_plugin)

add_test(NAME test_miner_plugin COMMAND test_miner_plugin)