file(GLOB HEADERS "include/celesos/hashrate_metric_plugin/*.hpp")
add_library(hashrate_metric_plugin
        hashrate_metric_plugin.cpp
        ${HEADERS})

target_link_libraries(hashrate_metric_plugin
        appbase
        fc
        pow)
target_include_directories(hashrate_metric_plugin
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
