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

using namespace celesos;

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
    uint32_t expected = (ethash::FNV_PRIME * x) ^y;
    uint32_t actual = ethash::fnv(x, y);
    BOOST_REQUIRE_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(sha256_test) {
    std::string input{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[32];
    ethash::sha256(buffer, input.c_str(), input.size());

    std::string actual{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    std::string expect{"3af805c48a2c0ebeed554ca823ab935ac1be570533b651122cf517791fa1561c"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}

BOOST_AUTO_TEST_CASE(sha512_test) {
    std::string input{"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"};
    unsigned char buffer[64];
    ethash::sha512(buffer, input.c_str(), input.size());

    std::string actual{std::cbegin(buffer), std::cend(buffer)};
    actual = fc::to_hex(actual.c_str(), static_cast<uint32_t>(actual.size()));

    std::string expect{
            "f259c589d93341b291ffa906b1ac62977221b86b96535ca61b14fa30f7be0559fa858fa1b7256cd500543f55171a44846decd52c8e667b0685529086d481e0b3"};
    BOOST_REQUIRE_EQUAL(actual, expect);
}

BOOST_AUTO_TEST_CASE(uint256_hex_conversion) {
    uint256_t val{0x190FAB};
    std::string val_hex{"0x190FAB"};

    std::string ret_hex{};
    ethash::uint256_to_hex(ret_hex, val);
    BOOST_REQUIRE(ret_hex == val_hex);

    uint256_t ret{};
    ethash::hex_to_uint256(ret, ret_hex);
    BOOST_REQUIRE(ret == val);
}

BOOST_AUTO_TEST_CASE(light_and_full_client_checks) {

    const static auto CACHE_COUNT = 512;
    const static auto DATASET_COUNT = 512 * 16;

    uint256_t target{"0x00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"};
    std::string seed{"15fb89b6d22d57d008d74c4dfa8ae5e2ea2f2cda5f050d690d3d67e31cd15b8b"};
    std::string forest{"ee4da936687ccd26b510b23b600a8cf461b30b1d61f51bcaa9f6a27d2e55a9bc"};

    auto cache = std::vector<ethash::node>{CACHE_COUNT, std::vector<ethash::node>::allocator_type()};
    calc_cache(cache, CACHE_COUNT, seed);

    auto dataset = std::vector<ethash::node>{DATASET_COUNT, std::vector<ethash::node>::allocator_type()};
    calc_dataset(dataset, DATASET_COUNT, cache);

    uint256_t wood_init = 0;
    uint256_t tmp = 0;
    std::random_device rd;
    for (int i = 0; i < 16; ++i) {
        tmp = rd();
        tmp <<= i * 16;
        wood_init |= tmp;
    }
    uint256_t wood = wood_init;
    bool is_find = false;
    do {
        if (ethash::hash_full(forest, wood, DATASET_COUNT, dataset) <= target) {
            is_find = true;
            break;
        }
        wood++;
    } while (wood > wood_init);
    BOOST_REQUIRE(is_find == true);
    auto wood_hex = std::string{"0x"} + wood.str(0, std::ios_base::hex);
    BOOST_REQUIRE(ethash::hash_full(forest, wood, DATASET_COUNT, dataset) ==
                  ethash::hash_light(forest, wood, DATASET_COUNT, cache));
    BOOST_REQUIRE(ethash::hash_light(forest, wood, DATASET_COUNT, cache) <= target);
}

BOOST_AUTO_TEST_CASE(light_and_full_client_checks_hex) {
    const static auto CACHE_COUNT = 512;
    const static auto DATASET_COUNT = 8192;

    uint256_t target{"586331470385716162175479365644515953327327300448928783824253240972673024"};
    std::string seed{"101dcbc6b7cf5170508696a930a6a03bc893d13ebb96067a2752bf8c37b97382"};
    std::string forest{"5b1eb124908f2ffe503c07a3ed74c3dffb0eef47d6650fba37b2a05ebc8a9960"};

    auto cache = std::vector<ethash::node>{CACHE_COUNT, std::vector<ethash::node>::allocator_type()};
    calc_cache(cache, CACHE_COUNT, seed);

    auto dataset = std::vector<ethash::node>{DATASET_COUNT, std::vector<ethash::node>::allocator_type()};
    calc_dataset(dataset, DATASET_COUNT, cache);

//    uint256_t wood{0xA8E25909D52F01212ACAFA6398E0CD6AFE169C247E4F59B9457393918E9F34BA};
    std::string wood_hex{"0x5F064CC659695DE19E14C742E05C8D22804613A2D0D448DB7C2EF3EEC5C676AF"};
    uint256_t wood{"0x5F064CC659695DE19E14C742E05C8D22804613A2D0D448DB7C2EF3EEC5C676AF"};
    auto light_ret = ethash::hash_light(forest, wood, DATASET_COUNT, cache).str(0, std::ios_base::hex);
    auto full_ret = ethash::hash_full(forest, wood, DATASET_COUNT, dataset).str(0, std::ios_base::hex);
    auto light_hex_ret = ethash::hash_light_hex(forest, wood_hex, DATASET_COUNT, cache).str(0, std::ios_base::hex);
    auto full_hex_ret = ethash::hash_full_hex(forest, wood_hex, DATASET_COUNT, dataset).str(0, std::ios_base::hex);
    BOOST_REQUIRE(light_ret == light_hex_ret);
    BOOST_REQUIRE(full_ret == full_hex_ret);
    BOOST_REQUIRE(light_ret == full_ret);
    BOOST_REQUIRE(light_hex_ret == full_hex_ret);
    BOOST_REQUIRE(uint256_t{std::string{"0x"} + light_ret} <= target);
}