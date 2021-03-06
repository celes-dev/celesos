//
// Created by yale on 7/26/18.
//

#ifndef POW_ETHASH_H
#define POW_ETHASH_H

#include <cstdint>
#include <vector>
#include <string>
#include <boost/multiprecision/cpp_int.hpp>

namespace celesos {
    namespace ethash {
        static const uint32_t FNV_PRIME = 0x01000193;
        static const uint32_t WORD_BYTES = 4;
        static const uint32_t NODE_BYTES = 64;
        static const uint32_t NODE_WORDS = NODE_BYTES / WORD_BYTES;
        static const uint32_t MIX_BYTES = NODE_BYTES * 2;
        static const uint32_t MIX_WORDS = MIX_BYTES / WORD_BYTES;
        static const uint32_t MIX_NODES = MIX_WORDS / NODE_WORDS;
        static const uint32_t DATASET_PARENTS = 256;
        static const uint32_t CACHE_ROUNDS = 3;
        static const uint32_t ACCESSES = 3;

        union node {
            uint8_t bytes[NODE_BYTES];
            uint32_t words[NODE_WORDS];
        };

        bool calc_cache(std::vector<node> &cache, uint32_t cache_count, const std::string &seed);

        node calc_dataset_item(const std::vector<node> &cache, uint32_t item_idx);

        bool
        calc_dataset(std::vector<node> &dataset, uint32_t dataset_count, const std::vector<node> &cache);

        boost::multiprecision::uint256_t
        hash_light(const std::string &forest,
                   const boost::multiprecision::uint256_t &wood,
                   uint32_t dataset_count,
                   const std::vector<node> &cache);

        boost::multiprecision::uint256_t
        hash_full(const std::string &forest,
                  const boost::multiprecision::uint256_t &wood,
                  uint32_t dataset_count,
                  const std::vector<node> &dataset);

        bool uint256_to_hex(std::string &dst, const boost::multiprecision::uint256_t &src);

        bool hex_to_uint256(boost::multiprecision::uint256_t &dst, const std::string &src);

        boost::multiprecision::uint256_t
        hash_light_hex(const std::string &forest,
                       const std::string &wood_hex,
                       uint32_t dataset_count,
                       const std::vector<node> &cache);

        boost::multiprecision::uint256_t
        hash_full_hex(const std::string &forest,
                      const std::string &wood_hex,
                      uint32_t dataset_count,
                      const std::vector<node> &dataset);
    }
}

#endif //POW_ETHASH_H
