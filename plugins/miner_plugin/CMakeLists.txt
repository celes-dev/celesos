file(GLOB HEADERS "include/eosio/miner_plugin/*.h*")
add_library(miner_plugin
        miner_plugin.cpp
        worker.cpp
        miner.cpp
        ${HEADERS})

target_link_libraries(miner_plugin appbase pow chain_plugin)
target_include_directories(miner_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
