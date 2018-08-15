//
// Created by huberyzhang on 2018/7/27.
//

#include <boost/core/typeinfo.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <eosio/chain/types.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <fc/crypto/sha256.hpp>
#include "../../../../pow/include/celesos/pow/ethash.h"
#include "../include/eosio/chain/controller.hpp"

namespace celesos{
    using namespace eosio;
    using namespace chain;
    namespace forest{

     typedef struct{
       block_id_type seed;
       block_id_type forest;
       uint32_t block_number;
       uint32_t next_block_num;

        boost::multiprecision::uint256_t target;
      }forest_struct;

      uint32_t cache_count();
      uint32_t dataset_count();

     class forest_bank{
        public:
         static forest_bank* getInstance(controller &control);

         bool get_forest(forest_struct& forest, const account_name& account);
         bool verify_wood(uint32_t block_number, const account_name& account, uint64_t wood);


        private:
         forest_bank(controller &control);
         ~forest_bank();

         void update_cache(const block_state_ptr& block);
         static forest_bank* instance;
         controller &chain;

         forest_struct forest_data;
         pair<uint32_t,std::vector<celesos::ethash::node>> first_cache_pair;
         pair<uint32_t,std::vector<celesos::ethash::node>> second_scahe_pair;
     };
    }
}
