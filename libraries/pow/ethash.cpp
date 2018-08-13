//
// Created by yale on 7/26/18.
//

#include "celesos/pow/ethash.h"
#include "openssl/sha.h"
#include "boost/endian/conversion.hpp"
#include "boost/predef.h"

using std::string;
using std::vector;

using boost::endian::endian_reverse;
using boost::endian::native_to_little;

using boost::multiprecision::uint512_t;
using boost::multiprecision::uint256_t;

using byte = unsigned char;

namespace celesos {
    namespace ethash {

        void sha512(byte *dst, const void *src, size_t src_size) {
            SHA512_CTX ctx;
            SHA512_Init(&ctx);
            SHA512_Update(&ctx, src, src_size);
            SHA512_Final(dst, &ctx);
        }

        void sha256(byte *dst, const void *src, size_t src_size) {
            SHA256_CTX ctx;
            SHA256_Init(&ctx);
            SHA256_Update(&ctx, src, src_size);
            SHA256_Final(dst, &ctx);
        }

        inline uint32_t fnv(uint32_t v1, uint32_t v2) {
            return v1 * FNV_PRIME ^ v2;
        }

        bool bytes_to_uint256(uint256_t &dst, const byte *src, uint32_t src_size) {
            // avoid exceed
            if (src_size > 32) {
                return false;
            }

            dst = 0;
            uint256_t tmp;
#ifdef BOOST_LITTLE_ENDIAN
//    for (uint i = 0; i < src_size; ++i) {
//        dst |= uint256_t{src[i]} << (8 * i);
//    }
            for (uint i = 0; i < src_size; ++i) {
                tmp = src[i];
                tmp <<= 8 * i;
                dst |= tmp;
            }
#endif
#ifdef BOOST_BIG_ENDIAN
            //    for (int i = 0; i < src_size; ++i) {
    //        dst |= uint256_t{src[src_size - 1 - i]} << (8 * i);
    //    }
        for (uint i = 0; i < src_size; ++i) {
            tmp = src[src_size - 1 - i];
            tmp <<= 8 * i;
            dst |= tmp;
        }
#endif
            return true;
        }

        void fix_endian_arr32(uint32_t *arr, uint32_t arr_size) {
#ifdef BOOST_BIG_ENDIAN
            for (int i = 0; i < arr_size; ++i) {
        arr[i] = boost::endian::endian_reverse(arr[i]);
    }
#endif
        }

        bool calc_cache(vector<node> &cache, uint32_t cache_count, const string &seed) {
            cache.resize(cache_count);

            sha512(cache[0].bytes, seed.c_str(), seed.size());
            for (int i = 1; i < cache_count; ++i) {
                sha512(cache[i].bytes, cache[i - 1].bytes, NODE_BYTES);
            }

            auto tmp = node{};
            for (uint32_t _ = 0; _ < CACHE_ROUNDS; ++_) {
                for (uint32_t i = 0; i < cache_count; ++i) {
                    const auto &first = cache[(cache_count - 1 + i) % cache_count].words;
                    const auto &second = cache[cache[i].words[0] % cache_count].words;
                    for (int j = 0; j < NODE_WORDS; ++j) {
                        tmp.words[j] = first[j] ^ second[j];
                    }
                    sha512(cache[i].bytes, tmp.bytes, NODE_BYTES);
//            fix_endian_arr32(cache[i].words, NODE_WORDS);
                }
            }

            auto cache_words = static_cast<uint32_t >(NODE_WORDS * cache.size());
            fix_endian_arr32(cache.data()->words, cache_words);

            return true;
        }

        node calc_dataset_item(const vector<node> &cache, uint32_t item_idx) {
            auto cache_size = cache.size();

            auto dataset_item = cache[item_idx % cache_size];
            dataset_item.words[0] ^= item_idx;
            sha512(dataset_item.bytes, dataset_item.bytes, NODE_BYTES);

            for (uint32_t i = 0; i < DATASET_PARENTS; ++i) {
                auto parent_idx = fnv(item_idx ^ i, dataset_item.words[i % NODE_WORDS]) % cache_size;
                const auto &parent = cache[parent_idx % cache_size];
                for (int j = 0; j < NODE_WORDS; ++j) {
                    dataset_item.words[j] = fnv(dataset_item.words[j], parent.words[j]);
                }
            }
            sha512(dataset_item.bytes, dataset_item.bytes, NODE_BYTES);
            return dataset_item;
        }

        bool calc_dataset(vector<node> &dataset, uint32_t dataset_count, const vector<node> &cache) {
            dataset.resize(dataset_count);

            for (uint32_t i = 0; i < dataset_count; ++i) {
                dataset.push_back(calc_dataset_item(cache, i));
            }
            return true;
        }

        uint256_t hash_impl(const string &forest_template, uint64_t nonce_template, uint32_t dataset_count,
                            const std::function<node(uint32_t)> &dataset_lookup) {

            char buffer[40];
            memcpy(buffer, &forest_template[0], 32);
            auto nonce = native_to_little(nonce_template);
            memcpy(buffer + 32, &nonce, 8);

            auto forest = node{};
            sha512(forest.bytes, buffer, 40);
            fix_endian_arr32(forest.words, NODE_WORDS);

            union {
                uint8_t bytes[MIX_BYTES];
                uint32_t words[MIX_WORDS];
            } mix = {};

            for (int i = 0; i < MIX_NODES; ++i) {
                memcpy(&mix.bytes[0] + i * NODE_BYTES, forest.bytes, NODE_BYTES);
            }

            auto full_pages = dataset_count * NODE_WORDS / MIX_WORDS;
            union {
                uint8_t bytes[MIX_BYTES];
                uint32_t words[MIX_WORDS];
            } new_data = {};
            for (uint32_t i = 0; i < ACCESSES; ++i) {
                auto p = fnv(i ^ forest.words[0], mix.words[i % MIX_WORDS]) % full_pages;

                for (uint32_t j = 0; j < MIX_NODES; ++j) {
                    memcpy(&new_data.bytes[0] + i * NODE_BYTES, dataset_lookup(MIX_NODES * p + j).bytes, NODE_BYTES);
                }

                for (int j = 0; j < MIX_BYTES; ++j) {
                    mix.words[j] = fnv(mix.words[j], new_data.words[j]);
                }
            }

            // compress mix
            for (uint32_t i = 0; i != MIX_WORDS; i += 4) {
                mix.words[i / 4] = fnv(mix.words[i + 3], fnv(mix.words[i + 2], fnv(mix.words[i + 1], mix.words[i])));
            }
            fix_endian_arr32(mix.words, MIX_WORDS / 4);

            // concat forest and compress mix
            char forest_cmix[NODE_BYTES + MIX_BYTES / 4];
            memcpy(&forest_cmix[0], forest.bytes, NODE_BYTES);
            memcpy(&forest_cmix[0] + NODE_BYTES, mix.bytes, MIX_BYTES / 4);

            byte ret_hash[32];
            sha256(ret_hash, forest_cmix, NODE_BYTES + MIX_BYTES / 4);
            auto ret_uint256 = uint256_t{0};
            bytes_to_uint256(ret_uint256, ret_hash, 32);
            const auto &ret_little_uint256 = native_to_little(ret_uint256);
            return ret_little_uint256;
        }

        uint256_t hash_light(const string &forest, uint64_t nonce, uint32_t dataset_count,
                             const vector<node> &cache) {
            auto dataset_lookup = [&cache](uint32_t x) {
                return calc_dataset_item(cache, x);
            };
            return hash_impl(forest, nonce, dataset_count, dataset_lookup);
        }

        uint256_t hash_full(const string &forest, uint64_t nonce, uint32_t dataset_count,
                            const vector<node> &dataset) {
            auto dataset_lookup = [&dataset](uint32_t x) {
                return dataset[x];
            };
            return hash_impl(forest, nonce, dataset_count, dataset_lookup);
        }
    }
}

