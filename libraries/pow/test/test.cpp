//
// Created by yale on 8/9/18.
//

#define BOOST_TEST_MODULE pow

#include <random>
#include "celesos/pow/ethash.hpp"
#include "boost/test/included/unit_test.hpp"
#include "boost/multiprecision/cpp_int.hpp"
#include "boost/optional.hpp"
#include "fc/crypto/hex.hpp"

using namespace celesos::ethash;

using boost::multiprecision::uint256_t;

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
    std::string input{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[32];
    sha256(buffer, input.c_str(), input.size());

    std::string actual{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    std::string expect{"3af805c48a2c0ebeed554ca823ab935ac1be570533b651122cf517791fa1561c"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}

BOOST_AUTO_TEST_CASE(sha512_test) {
    std::string input{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[64];
    sha512(buffer, input.c_str(), input.size());

    std::string actual{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    std::string expect{
            "f259c589d93341b291ffa906b1ac62977221b86b96535ca61b14fa30f7be0559fa858fa1b7256cd500543f55171a44846decd52c8e667b0685529086d481e0b3"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}

BOOST_AUTO_TEST_CASE(light_and_full_client_checks) {
    const static uint32_t HASH_BYTES = 64;
    const static uint64_t CACHE_BYTES = 1024;
    const static uint64_t DATASET_BYTES = 1024 * 32;

    const static auto CACHE_COUNT = static_cast<uint32_t>(CACHE_BYTES / HASH_BYTES);
    const static auto DATASET_COUNT = static_cast<uint32_t>(DATASET_BYTES / HASH_BYTES);

    uint256_t target{197};
    target |= uint256_t{90} << 1;
    for (int i = 2; i < 32; ++i) {
        target |= uint256_t{255} << i * 8;
    }

    std::string seed{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    auto &forest = seed;

    auto cache = std::vector<node>{CACHE_COUNT, std::vector<node>::allocator_type()};
    calc_cache(cache, CACHE_COUNT, seed);

    auto dataset = std::vector<node>{DATASET_COUNT, std::vector<node>::allocator_type()};
    calc_dataset(dataset, DATASET_COUNT, cache);

    uint256_t nonce_init = 0;
    uint256_t tmp = 0;
    std::random_device rd;
    for (int i = 0; i < 16; ++i) {
        tmp = rd();
        tmp <<= i * 16;
        nonce_init |= tmp;
    }
    uint256_t nonce = nonce_init;
    do {
        if (hash_full(forest, nonce, DATASET_COUNT, dataset) <= target) {
            break;
        }
        nonce++;
    } while (nonce > nonce_init);

    BOOST_REQUIRE(hash_light(forest, nonce, DATASET_COUNT, cache));
}
