/**
 *  @file
 *  @copyright defined in celes/LICENSE.txt
 */
#pragma once
#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/abi_def.hpp>

#include "multi_index_includes.hpp"

/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/chain/database_utils.hpp>
#include <eosio/chain/authority.hpp>
#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/abi_def.hpp>

#include "multi_index_includes.hpp"

namespace eosio { namespace chain {

   class dbp_object : public chainbase::object<dbp_object_type, dbp_object> {
      OBJECT_CTOR(dbp_object,(code)(abi))

      id_type              id;
      account_name         name;
      int64_t              total_resouresweight;   // 总点击（非真实点击，以CPU、NET及ROM进行折算）
      int64_t              unpaid_resouresweight;  // 未领取奖励的点击（非真实点击，以CPU、NET及ROM进行折算）

      time_point           last_code_update;
      digest_type          code_version;
      block_timestamp_type creation_date;

      shared_blob    code;
      shared_blob    abi;

      void set_abi( const eosio::chain::abi_def& a ) {
         abi.resize( fc::raw::pack_size( a ) );
         fc::datastream<char*> ds( abi.data(), abi.size() );
         fc::raw::pack( ds, a );
      }

      eosio::chain::abi_def get_abi()const {
         eosio::chain::abi_def a;
         EOS_ASSERT( abi.size() != 0, abi_not_found_exception, "No ABI set on account ${n}", ("n",name) );

         fc::datastream<const char*> ds( abi.data(), abi.size() );
         fc::raw::unpack( ds, a );
         return a;
      }
   };
   using dbp_id_type = dbp_object::id_type;

   struct by_name;
   struct by_total_resouresweight;
   using dbp_index = chainbase::shared_multi_index_container<
      dbp_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<dbp_object, dbp_object::id_type, &dbp_object::id>>,
         ordered_unique<tag<by_name>, member<dbp_object, account_name, &dbp_object::name>>,
         ordered_unique<tag<by_total_resouresweight>,
            composite_key< dbp_object,
               member<dbp_object, int64_t, &dbp_object::total_resouresweight>
            >,
            composite_key_compare<std::greater<int64_t> >
         >
      >
   >;

} } // eosio::chain

CHAINBASE_SET_INDEX_TYPE(eosio::chain::dbp_object, eosio::chain::dbp_index)

FC_REFLECT(eosio::chain::dbp_object, (name)(total_resouresweight)(unpaid_resouresweight)(last_code_update)(code_version)(creation_date)(code)(abi))