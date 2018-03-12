/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include "common.hpp"
#include "voting.hpp"

#include <eosiolib/eosio.hpp>
#include <eosiolib/token.hpp>
#include <eosiolib/print.hpp>

#include <eosiolib/generic_currency.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>

#include <map>

namespace eosiosystem {
   using eosio::asset;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::bytes;
   using eosio::print;
   using std::map;
   using std::pair;

   template<account_name SystemAccount>
   class delegate_bandwith : public voting<SystemAccount> {
      public:
         static constexpr account_name system_account = SystemAccount;
         using currency = typename common<SystemAccount>::currency;
         using system_token_type = typename common<SystemAccount>::system_token_type;
         using eosio_parameters = typename common<SystemAccount>::eosio_parameters;
         using global_state_singleton = typename common<SystemAccount>::global_state_singleton;

         struct total_resources {
            account_name owner;
            typename currency::token_type net_weight;
            typename currency::token_type cpu_weight;
            typename currency::token_type storage_stake;
            uint64_t storage_bytes = 0;

            uint64_t primary_key()const { return owner; }

            EOSLIB_SERIALIZE( total_resources, (owner)(net_weight)(cpu_weight)(storage_stake)(storage_bytes) )
         };


         /**
          *  Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
          */
         struct delegated_bandwidth {
            account_name from;
            account_name to;
            typename currency::token_type net_weight; 
            typename currency::token_type cpu_weight;
            typename currency::token_type storage_stake;
            uint64_t storage_bytes = 0;
            /*
            uint32_t start_pending_net_withdraw = 0;
            typename currency::token_type pending_net_withdraw;
            uint64_t deferred_net_withdraw_handler = 0;

            uint32_t start_pending_cpu_withdraw = 0;
            typename currency::token_type pending_cpu_withdraw;
            uint64_t deferred_cpu_withdraw_handler = 0;
            */
            
            uint64_t  primary_key()const { return to; }

            EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight)(storage_stake)(storage_bytes)
                              /* (start_pending_net_withdraw)(pending_net_withdraw)(deferred_net_withdraw_handler)
                                 (start_pending_cpu_withdraw)(pending_cpu_withdraw)(deferred_cpu_withdraw_handler)*/ )

         };

         typedef eosio::multi_index< N(totalband), total_resources>  total_resources_index_type;
         typedef eosio::multi_index< N(delband), delegated_bandwidth> del_bandwidth_index_type;

         ACTION( SystemAccount, delegatebw ) {
            account_name from;
            account_name receiver;
            asset        stake_net_quantity;
            asset        stake_cpu_quantity;
            asset        stake_storage_quantity;


            EOSLIB_SERIALIZE( delegatebw, (from)(receiver)(stake_net_quantity)(stake_cpu_quantity)(stake_storage_quantity) )
         };

         ACTION( SystemAccount, undelegatebw ) {
            account_name from;
            account_name receiver;
            asset        unstake_net_quantity;
            asset        unstake_cpu_quantity;
            uint64_t     unstake_storage_bytes;

            EOSLIB_SERIALIZE( undelegatebw, (from)(receiver)(unstake_net_quantity)(unstake_cpu_quantity)(unstake_storage_bytes) )
         };

         static void on( const delegatebw& del ) {
            eosio_assert( del.stake_cpu_quantity.amount >= 0, "must stake a positive amount" );
            eosio_assert( del.stake_net_quantity.amount >= 0, "must stake a positive amount" );
            eosio_assert( del.stake_storage_quantity.amount >= 0, "must stake a positive amount" );

            system_token_type total_stake = del.stake_cpu_quantity + del.stake_net_quantity + del.stake_storage_quantity;
            eosio_assert( total_stake.quantity > 0, "must stake a positive amount" );

            require_auth( del.from );

            //eosio_assert( is_account( del.receiver ), "can only delegate resources to an existing account" );
            uint64_t storage_bytes = 0;
            if ( 0 < del.stake_storage_quantity.amount ) {
               auto parameters = global_state_singleton::exists() ? global_state_singleton::get()
                  : common<SystemAccount>::get_default_parameters();
               auto token_supply = currency::get_total_supply();//.quantity;

               //make sure that there is no posibility of overflow here
               uint64_t storage_bytes_estimated = ( parameters.max_storage_size - parameters.total_storage_bytes_reserved )
                  * parameters.storage_reserve_ratio * system_token_type(del.stake_cpu_quantity)
                  / ( token_supply - parameters.total_storage_stake ) / 1000 /* reserve ratio coefficient */;

               storage_bytes = ( parameters.max_storage_size - parameters.total_storage_bytes_reserved - storage_bytes_estimated )
                  * parameters.storage_reserve_ratio * system_token_type(del.stake_cpu_quantity)
                  / ( token_supply - del.stake_storage_quantity - parameters.total_storage_stake ) / 1000 /* reserve ratio coefficient */;

               eosio_assert( 0 < storage_bytes, "stake is too small to increase storage even by 1 byte" );

               parameters.total_storage_bytes_reserved += storage_bytes;
               parameters.total_storage_stake += del.stake_storage_quantity;
               global_state_singleton::set(parameters);
            }

            del_bandwidth_index_type     del_index( SystemAccount, del.from );
            auto itr = del_index.find( del.receiver );
            if( itr == nullptr ) {
               del_index.emplace( del.from, [&]( auto& dbo ){
                  dbo.from          = del.from;
                  dbo.to            = del.receiver;
                  dbo.net_weight    = del.stake_net_quantity;
                  dbo.cpu_weight    = del.stake_cpu_quantity;
                  dbo.storage_stake = del.stake_storage_quantity;
                  dbo.storage_bytes = storage_bytes;
               });
            }
            else {
               del_index.update( *itr, del.from, [&]( auto& dbo ){
                  dbo.net_weight    += del.stake_net_quantity;
                  dbo.cpu_weight    += del.stake_cpu_quantity;
                  dbo.storage_stake += del.stake_storage_quantity;
                  dbo.storage_bytes += storage_bytes;
               });
            }

            total_resources_index_type   total_index( SystemAccount, del.receiver );
            auto tot_itr = total_index.find( del.receiver );
            if( tot_itr == nullptr ) {
               tot_itr = &total_index.emplace( del.from, [&]( auto& tot ) {
                  tot.owner = del.receiver;
                  tot.net_weight    = del.stake_net_quantity;
                  tot.cpu_weight    = del.stake_cpu_quantity;
                  tot.storage_stake = del.stake_storage_quantity;
                  tot.storage_bytes = storage_bytes;
               });
            } else {
               total_index.update( *tot_itr, del.from == del.receiver ? del.from : 0, [&]( auto& tot ) {
                  tot.net_weight    += del.stake_net_quantity;
                  tot.cpu_weight    += del.stake_cpu_quantity;
                  tot.storage_stake += del.stake_storage_quantity;
                  tot.storage_bytes += storage_bytes;
               });
            }

            set_resource_limits( tot_itr->owner, tot_itr->storage_bytes, tot_itr->net_weight.quantity, tot_itr->cpu_weight.quantity, 0 );

            currency::inline_transfer( del.from, SystemAccount, total_stake, "stake bandwidth" );
            /* temporarily commented out
            if ( 0 < del.stake_net_quantity + del.stake_cpu_quantity ) {
               increase_voting_power( del.from, del.stake_net_quantity + del.stake_cpu_quantity );
            }
            */
         } // delegatebw

         static void on( const undelegatebw& del ) {
            eosio_assert( del.unstake_cpu_quantity.amount >= 0, "must unstake a positive amount" );
            eosio_assert( del.unstake_net_quantity.amount >= 0, "must unstake a positive amount" );

            require_auth( del.from );

            //eosio_assert( is_account( del.receiver ), "can only delegate resources to an existing account" );

            del_bandwidth_index_type     del_index( SystemAccount, del.from );
            const auto& dbw = del_index.get( del.receiver );
            eosio_assert( dbw.net_weight >= del.unstake_net_quantity, "insufficient staked net bandwidth" );
            eosio_assert( dbw.cpu_weight >= del.unstake_cpu_quantity, "insufficient staked cpu bandwidth" );
            eosio_assert( dbw.storage_bytes >= del.unstake_storage_bytes, "insufficient staked storage" );

            system_token_type storage_stake_decrease = system_token_type(0);
            if ( 0 < del.unstake_storage_bytes ) {
               storage_stake_decrease = 0 < dbw.storage_bytes ?
                                            dbw.storage_stake * del.unstake_storage_bytes / dbw.storage_bytes
                                            : system_token_type(0);

               auto parameters = global_state_singleton::get();
               parameters.total_storage_bytes_reserved -= del.unstake_storage_bytes;
               parameters.total_storage_stake -= storage_stake_decrease;
               global_state_singleton::set( parameters );
            }

            auto total_refund = system_token_type(del.unstake_cpu_quantity)
               + system_token_type(del.unstake_net_quantity) + storage_stake_decrease;

            eosio_assert( total_refund.quantity > 0, "must unstake a positive amount" );

            del_index.update( dbw, del.from, [&]( auto& dbo ){
               dbo.net_weight -= del.unstake_net_quantity;
               dbo.cpu_weight -= del.unstake_cpu_quantity;
               dbo.storage_stake -= storage_stake_decrease;
               dbo.storage_bytes -= del.unstake_storage_bytes;
            });

            total_resources_index_type   total_index( SystemAccount, del.receiver );
            const auto& totals = total_index.get( del.receiver );
            total_index.update( totals, 0, [&]( auto& tot ) {
               tot.net_weight -= del.unstake_net_quantity;
               tot.cpu_weight -= del.unstake_cpu_quantity;
               tot.storage_stake -= storage_stake_decrease;
               tot.storage_bytes -= del.unstake_storage_bytes;
            });

            set_resource_limits( totals.owner, totals.storage_bytes, totals.net_weight.quantity, totals.cpu_weight.quantity, 0 );

            /// TODO: implement / enforce time delays on withdrawing
            currency::inline_transfer( SystemAccount, del.from, asset( static_cast<int64_t>( total_refund.quantity )), "unstake bandwidth" );
         } // undelegatebw
   };
}
