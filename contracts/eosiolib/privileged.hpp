#pragma once
#include "privileged.h"
#include "serialize.hpp"

namespace eosio {
   struct blockchain_parameters : ::blockchain_parameters {
   
      EOSLIB_SERIALIZE( blockchain_parameters, (target_block_size)(max_block_size)(target_block_acts_per_scope)
                        (max_block_acts_per_scope)(target_block_acts)(max_block_acts)(max_storage_size)
                        (max_transaction_lifetime)(max_transaction_exec_time)(max_authority_depth)
                        (max_inline_depth)(max_inline_action_size)(max_generated_transaction_size)
      )
   };

   struct producer_key {
      account_name           producer_name;
      std::array<char, 4+33> block_signing_key;

      EOSLIB_SERIALIZE( producer_key, (producer_name)(block_signing_key) )
   };

   struct producer_schedule {
      uint32_t             version = 0; ///< sequentially incrementing version number
      vector<producer_key> producers;

      EOSLIB_SERIALIZE( producer_schedule, (version)(producers) )
   };

}
