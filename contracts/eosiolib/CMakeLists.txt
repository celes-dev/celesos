configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core_symbol.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/core_symbol.hpp)

add_wast_library(TARGET eosiolib
  INCLUDE_FOLDERS "${STANDARD_INCLUDE_FOLDERS}" ${CMAKE_SOURCE_DIR}/externals/magic_get/include
  DESTINATION_FOLDER ${CMAKE_CURRENT_BINARY_DIR}
)

file(WRITE ./stdlib.hpp "#pragma once\r\n")
file(APPEND ./stdlib.hpp "#include <initializer_list>\r\n")
file(APPEND ./stdlib.hpp "#include <iterator>\r\n")
file(APPEND ./stdlib.hpp "#include <memory>\r\n")

IF (CMAKE_BUILD_TYPE STREQUAL Debug)
        file(APPEND ./stdlib.hpp "\r\n#define DEBUG 1\r\n")
ELSE()
        file(APPEND ./stdlib.hpp "\r\n#define DEBUG 0\r\n")
ENDIF()

