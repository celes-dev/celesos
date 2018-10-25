file(GLOB HEADERS "include/celesos/miner_plugin/*.hpp")
add_library(miner_plugin
        miner_plugin.cpp
        worker.cpp
        miner.cpp
        ${HEADERS})

target_link_libraries(miner_plugin
        PUBLIC pow
        PUBLIC eosio_chain
        PUBLIC appbase
        PUBLIC chain_plugin
        PUBLIC http_client_plugin)
target_include_directories(miner_plugin
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(test)
