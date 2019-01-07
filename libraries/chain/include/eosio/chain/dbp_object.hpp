/**
 *  @file
 *  @copyright defined in eos/libraries/pow/LICENSE.txt
 */
#pragma once
#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/abi_def.hpp>

#include "multi_index_includes.hpp"

#pragma once
#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/abi_def.hpp>

#include "multi_index_includes.hpp"

namespace eosio { namespace chain {

   class dbp_object : public chainbase::object<dbp_object_type, dbp_object> {
      OBJECT_CTOR(dbp_object)

      id_type              id;
      account_name         name;
      int64_t              total_resouresweight = 0;   // 总点击（非真实点击，以CPU、NET及ROM进行折算）
      int64_t              unpaid_resouresweight = 0;  // 未领取奖励的点击（非真实点击，以CPU、NET及ROM进行折算）
   };
   
   using dbp_id_type = dbp_object::id_type;

   struct by_name;
   struct by_total_resouresweight;
   using dbp_index = chainbase::shared_multi_index_container<
      dbp_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<dbp_object, dbp_object::id_type, &dbp_object::id>>,
         ordered_unique<tag<by_name>, member<dbp_object, account_name, &dbp_object::name>>,
         ordered_non_unique<tag<by_total_resouresweight>,
            composite_key< dbp_object,
               member<dbp_object, int64_t, &dbp_object::total_resouresweight>
            >,
            composite_key_compare<std::greater<int64_t> >
         >
      >
   >;

} } // eosio::chain

CHAINBASE_SET_INDEX_TYPE(eosio::chain::dbp_object, eosio::chain::dbp_index)

FC_REFLECT(eosio::chain::dbp_object, (name)(total_resouresweight)(unpaid_resouresweight))