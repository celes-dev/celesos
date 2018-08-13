file(GLOB HEADERS "include/eosio/miner_plugin/*.hpp")
add_library( miner_plugin
             miner_plugin.cpp
             ${HEADERS} )

target_link_libraries( miner_plugin appbase fc pow )
target_include_directories( miner_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
