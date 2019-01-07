/**
 *  @file
 *  @copyright defined in eos/libraries/pow/LICENSE.txt
 */
//
// Created by huberyzhang on 2018/7/27.
//
#pragma once
#include <boost/core/typeinfo.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <eosio/chain/types.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <fc/crypto/sha256.hpp>
#include "celesos/pow/ethash.hpp"
#include "eosio/chain/controller.hpp"

namespace celesos
{

namespace forest
{

typedef struct
{
  eosio::chain::block_id_type seed;
  eosio::chain::block_id_type forest;
  uint32_t block_number = 0;
  uint32_t next_block_num = 0;
  boost::multiprecision::uint256_t target = 0;
} forest_struct;

uint32_t cache_count();
uint32_t dataset_count();

struct forest_info_cache_object : public chainbase::object<eosio::chain::forest_info_cache_object_type, forest_info_cache_object>
{
  OBJECT_CTOR(forest_info_cache_object);

  id_type id;
  uint32_t block_number;
  double diff;
  eosio::chain::block_id_type block_id;
};

struct by_block_number;

using forest_info_multi_index = chainbase::shared_multi_index_container<
    forest_info_cache_object,
    indexed_by<ordered_unique<tag<by_id>, member<forest_info_cache_object, forest_info_cache_object::id_type, &forest_info_cache_object::id>>,
        ordered_unique<tag<by_block_number>, member<forest_info_cache_object, uint32_t, &forest_info_cache_object::block_number>>>>;
} // namespace forest
} // namespace celesos

CHAINBASE_SET_INDEX_TYPE(celesos::forest::forest_info_cache_object, celesos::forest::forest_info_multi_index)

namespace celesos
{

namespace forest
{

class forest_bank
{
public:
  static forest_bank *getInstance(eosio::chain::controller &control);

  bool get_forest(forest_struct &forest, const eosio::chain::account_name &account);

  bool verify_wood(uint32_t block_number, const eosio::chain::account_name &account, const char *wood);
  uint32_t forest_period_number();
  uint32_t cache_period_number();
  uint32_t forest_space_number();

private:
  forest_bank(eosio::chain::controller &control);
  ~forest_bank();

  void update_cache(const eosio::chain::block_state_ptr &block);
  void update_cache_with_block_number(uint32_t current_block_number);
  void update_forest(const eosio::chain::block_state_ptr &block);
  void update_forest_with_block_number(const uint32_t current_block_number,const double diff,const bool bcache);

  eosio::chain::controller &chain;

  eosio::chain::block_id_type getBlockIdFromCache(const uint32_t block_number);

  double getBlockDiffFromCache(const uint32_t block_number);

  void cacheBlockInfo(const uint32_t block_number, const eosio::chain::block_id_type block_id, const double diff);

  void cleanBlockCache(const uint32_t block_number);

  void loadBlockCacheInfo();

  forest_struct forest_data;
  using cache_pair_type = std::pair<uint32_t, std::vector<celesos::ethash::node>>;
  std::shared_ptr<cache_pair_type> first_cache_pair;
  std::shared_ptr<cache_pair_type> second_cache_pair;

  std::map<uint32_t, std::pair<eosio::chain::block_id_type, double>> block_cache;
  boost::mutex forestLock;
  fc::logger logger;
};
} // namespace forest
} // namespace celesos
