//
// Created by yale on 8/9/18.
//

#define BOOST_TEST_MODULE pow

#include "celesos/pow/ethash.h"
#include "boost/test/included/unit_test.hpp"
#include "fc/crypto/hex.hpp"

using namespace celesos::ethash;

namespace celesos {
    namespace ethash {
        extern uint32_t fnv(uint32_t v1, uint32_t v2);

        extern void sha256(unsigned char *dst, const void *src, size_t src_size);

        extern void sha512(unsigned char *dst, const void *src, size_t src_size);
    }
}

BOOST_AUTO_TEST_CASE(fnv_hash_check) {
    uint32_t x = 1235U;
    uint32_t y = 9999999U;
    uint32_t expected = (FNV_PRIME * x) ^y;
    uint32_t actual = fnv(x, y);
    BOOST_REQUIRE_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(sha256_test) {
    auto input = std::string{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[32];
    sha256(buffer, input.c_str(), input.size());

    auto actual = std::string{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    auto expect = std::string{"3af805c48a2c0ebeed554ca823ab935ac1be570533b651122cf517791fa1561c"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}

BOOST_AUTO_TEST_CASE(sha512_test) {
    auto input = std::string{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[64];
    sha512(buffer, input.c_str(), input.size());

    auto actual = std::string{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    auto expect = std::string{
            "f259c589d93341b291ffa906b1ac62977221b86b96535ca61b14fa30f7be0559fa858fa1b7256cd500543f55171a44846decd52c8e667b0685529086d481e0b3"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}