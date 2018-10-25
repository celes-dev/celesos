set(MY_SOURCES forest_bank_test.cpp)

add_executable(forest_bank_test ${MY_SOURCES})
target_link_libraries(forest_bank_test eosio_chain eosio_testing)

add_test(NAME forest_bank_test COMMAND forest_bank_test)