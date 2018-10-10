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

namespace celesos{

    namespace forest{

     typedef struct{
       eosio::chain::block_id_type seed;
       eosio::chain::block_id_type forest;
       uint32_t block_number;
       uint32_t next_block_num;
       boost::multiprecision::uint256_t target;
      }forest_struct;

      uint32_t cache_count();
      uint32_t dataset_count();

     class forest_bank{
        public:
         static forest_bank* getInstance(eosio::chain::controller &control);

         bool get_forest(forest_struct& forest, const eosio::chain::account_name& account);

         bool verify_wood(uint32_t block_number, const eosio::chain::account_name& account, const char* wood);
         uint32_t  forest_period_number();
         uint32_t forest_space_number();


        private:
         forest_bank(eosio::chain::controller &control);
         ~forest_bank();

         void update_cache(const eosio::chain::block_state_ptr& block);
         void update_forest(const eosio::chain::block_state_ptr &block);
         eosio::chain::controller &chain;

         void cleanBlockIdCache(uint32_t block_number);
         eosio::chain::block_id_type getBlockIdWithCache(uint32_t block_number);

         forest_struct forest_data;
         using cache_pair_type = std::pair<uint32_t,std::vector<celesos::ethash::node>> ;
         std::shared_ptr<cache_pair_type> first_cache_pair;
         std::shared_ptr<cache_pair_type> second_cache_pair;
     };
    }
}
