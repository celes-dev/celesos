/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include "common.hpp"
#include "voting.hpp"

#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.h>
#include <eosiolib/transaction.hpp>

#include <map>

namespace eosiosystem {
   using eosio::asset;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::bytes;
   using eosio::print;
   using eosio::permission_level;
   using std::map;
   using std::pair;

   template<account_name SystemAccount>
   class delegate_bandwidth : public voting<SystemAccount> {
      public:
         static constexpr account_name system_account = SystemAccount;
         static constexpr time refund_delay = 3*24*3600;
         static constexpr time refund_expiration_time = 3600;
         using eosio_parameters = typename common<SystemAccount>::eosio_parameters;
         using global_state_singleton = typename common<SystemAccount>::global_state_singleton;

         using voting<SystemAccount>::system_token_symbol;

         struct total_resources {
            account_name  owner;
            asset         net_weight;
            asset         cpu_weight;
            asset         storage_stake;
            uint64_t      storage_bytes = 0;

            uint64_t primary_key()const { return owner; }

            EOSLIB_SERIALIZE( total_resources, (owner)(net_weight)(cpu_weight)(storage_stake)(storage_bytes) )
         };


         /**
          *  Every user 'from' has a scope/table that uses every receipient 'to' as the primary key.
          */
         struct delegated_bandwidth {
            account_name  from;
            account_name  to;
            asset         net_weight;
            asset         cpu_weight;
            asset         storage_stake;
            uint64_t      storage_bytes = 0;

            uint64_t  primary_key()const { return to; }

            EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight)(storage_stake)(storage_bytes) )

         };

         struct refund_request {
            account_name  owner;
            time          request_time;
            eosio::asset  amount;

            uint64_t  primary_key()const { return owner; }

            EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(amount) )
         };

         typedef eosio::multi_index< N(totalband), total_resources>   total_resources_table;
         typedef eosio::multi_index< N(delband), delegated_bandwidth> del_bandwidth_table;
         typedef eosio::multi_index< N(refunds), refund_request>      refunds_table;

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

         ACTION( SystemAccount, refund ) {
            account_name owner;

            EOSLIB_SERIALIZE( refund, (owner) )
         };

         static void on( const delegatebw& del ) {
            eosio_assert( del.stake_cpu_quantity.amount >= 0, "must stake a positive amount" );
            eosio_assert( del.stake_net_quantity.amount >= 0, "must stake a positive amount" );
            eosio_assert( del.stake_storage_quantity.amount >= 0, "must stake a positive amount" );

            asset total_stake = del.stake_cpu_quantity + del.stake_net_quantity + del.stake_storage_quantity;
            eosio_assert( total_stake.amount > 0, "must stake a positive amount" );

            require_auth( del.from );

            //eosio_assert( is_account( del.receiver ), "can only delegate resources to an existing account" );
            int64_t storage_bytes = 0;
            if ( 0 < del.stake_storage_quantity.amount ) {
               auto parameters = global_state_singleton::exists() ? global_state_singleton::get()
                  : common<SystemAccount>::get_default_parameters();
               const eosio::asset token_supply = eosio::token(N(eosio.token)).get_supply(eosio::symbol_type(system_token_symbol).name());
               //make sure that there is no posibility of overflow here
               int64_t storage_bytes_estimated = int64_t( parameters.max_storage_size - parameters.total_storage_bytes_reserved )
                  * int64_t(parameters.storage_reserve_ratio) * del.stake_storage_quantity
                  / ( token_supply - parameters.total_storage_stake ) / 1000 /* reserve ratio coefficient */;

               storage_bytes = ( int64_t(parameters.max_storage_size) - int64_t(parameters.total_storage_bytes_reserved) - storage_bytes_estimated )
                  * int64_t(parameters.storage_reserve_ratio) * del.stake_storage_quantity
                  / ( token_supply - del.stake_storage_quantity - parameters.total_storage_stake ) / 1000 /* reserve ratio coefficient */;

               eosio_assert( 0 < storage_bytes, "stake is too small to increase storage even by 1 byte" );

               parameters.total_storage_bytes_reserved += uint64_t(storage_bytes);
               parameters.total_storage_stake += del.stake_storage_quantity;
               global_state_singleton::set(parameters);
            }

            del_bandwidth_table     del_tbl( SystemAccount, del.from );
            auto itr = del_tbl.find( del.receiver );
            if( itr == del_tbl.end() ) {
               del_tbl.emplace( del.from, [&]( auto& dbo ){
                  dbo.from          = del.from;
                  dbo.to            = del.receiver;
                  dbo.net_weight    = del.stake_net_quantity;
                  dbo.cpu_weight    = del.stake_cpu_quantity;
                  dbo.storage_stake = del.stake_storage_quantity;
                  dbo.storage_bytes = uint64_t(storage_bytes);
               });
            }
            else {
               del_tbl.modify( itr, del.from, [&]( auto& dbo ){
                  dbo.net_weight    += del.stake_net_quantity;
                  dbo.cpu_weight    += del.stake_cpu_quantity;
                  dbo.storage_stake += del.stake_storage_quantity;
                  dbo.storage_bytes += uint64_t(storage_bytes);
               });
            }

            total_resources_table   totals_tbl( SystemAccount, del.receiver );
            auto tot_itr = totals_tbl.find( del.receiver );
            if( tot_itr ==  totals_tbl.end() ) {
               tot_itr = totals_tbl.emplace( del.from, [&]( auto& tot ) {
                  tot.owner = del.receiver;
                  tot.net_weight    = del.stake_net_quantity;
                  tot.cpu_weight    = del.stake_cpu_quantity;
                  tot.storage_stake = del.stake_storage_quantity;
                  tot.storage_bytes = uint64_t(storage_bytes);
               });
            } else {
               totals_tbl.modify( tot_itr, del.from == del.receiver ? del.from : 0, [&]( auto& tot ) {
                  tot.net_weight    += del.stake_net_quantity;
                  tot.cpu_weight    += del.stake_cpu_quantity;
                  tot.storage_stake += del.stake_storage_quantity;
                  tot.storage_bytes += uint64_t(storage_bytes);
               });
            }

            //set_resource_limits( tot_itr->owner, tot_itr->storage_bytes, tot_itr->net_weight.quantity, tot_itr->cpu_weight.quantity );

            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {del.from,N(active)},
                                                          { del.from, N(eosio), total_stake, std::string("stake bandwidth") } );

            if ( asset(0) < del.stake_net_quantity + del.stake_cpu_quantity ) {
               voting<SystemAccount>::increase_voting_power( del.from, del.stake_net_quantity + del.stake_cpu_quantity );
            }

         } // delegatebw

         static void on( account_name receiver, const undelegatebw& del ) {
            eosio_assert( del.unstake_cpu_quantity.amount >= 0, "must unstake a positive amount" );
            eosio_assert( del.unstake_net_quantity.amount >= 0, "must unstake a positive amount" );

            require_auth( del.from );

            //eosio_assert( is_account( del.receiver ), "can only delegate resources to an existing account" );

            del_bandwidth_table     del_tbl( SystemAccount, del.from );
            const auto& dbw = del_tbl.get( del.receiver );
            eosio_assert( dbw.net_weight >= del.unstake_net_quantity, "insufficient staked net bandwidth" );
            eosio_assert( dbw.cpu_weight >= del.unstake_cpu_quantity, "insufficient staked cpu bandwidth" );
            eosio_assert( dbw.storage_bytes >= del.unstake_storage_bytes, "insufficient staked storage" );

            eosio::asset storage_stake_decrease(0, system_token_symbol);
            if ( 0 < del.unstake_storage_bytes ) {
               storage_stake_decrease = 0 < dbw.storage_bytes ?
                                            dbw.storage_stake * int64_t(del.unstake_storage_bytes) / int64_t(dbw.storage_bytes)
                                            : eosio::asset(0, system_token_symbol);

               auto parameters = global_state_singleton::get();
               parameters.total_storage_bytes_reserved -= del.unstake_storage_bytes;
               parameters.total_storage_stake -= storage_stake_decrease;
               global_state_singleton::set( parameters );
            }

            eosio::asset total_refund = del.unstake_cpu_quantity + del.unstake_net_quantity + storage_stake_decrease;

            eosio_assert( total_refund.amount > 0, "must unstake a positive amount" );

            del_tbl.modify( dbw, del.from, [&]( auto& dbo ){
               dbo.net_weight -= del.unstake_net_quantity;
               dbo.cpu_weight -= del.unstake_cpu_quantity;
               dbo.storage_stake -= storage_stake_decrease;
               dbo.storage_bytes -= del.unstake_storage_bytes;
            });

            total_resources_table totals_tbl( SystemAccount, del.receiver );
            const auto& totals = totals_tbl.get( del.receiver );
            totals_tbl.modify( totals, 0, [&]( auto& tot ) {
               tot.net_weight -= del.unstake_net_quantity;
               tot.cpu_weight -= del.unstake_cpu_quantity;
               tot.storage_stake -= storage_stake_decrease;
               tot.storage_bytes -= del.unstake_storage_bytes;
            });

            //set_resource_limits( totals.owner, totals.storage_bytes, totals.net_weight.quantity, totals.cpu_weight.quantity );

            refunds_table refunds_tbl( SystemAccount, del.from );
            //create refund request
            auto req = refunds_tbl.find( del.from );
            if ( req != refunds_tbl.end() ) {
               refunds_tbl.modify( req, 0, [&]( refund_request& r ) {
                     r.amount += del.unstake_net_quantity + del.unstake_cpu_quantity + storage_stake_decrease;
                     r.request_time = now();
                  });
            } else {
               refunds_tbl.emplace( del.from, [&]( refund_request& r ) {
                     r.owner = del.from;
                     r.amount = del.unstake_net_quantity + del.unstake_cpu_quantity + storage_stake_decrease;
                     r.request_time = now();
                  });
            }
            //create or replace deferred transaction
            const auto self = receiver; //current_receiver();
            refund act;
            act.owner = del.from;
            transaction out( now() + refund_delay + refund_expiration_time );
            out.actions.emplace_back( permission_level{ del.from, N(active) }, self, N(refund), act );
            out.delay_sec = refund_delay;
            out.send( del.from, receiver );

            if ( asset(0) < del.unstake_net_quantity + del.unstake_cpu_quantity ) {
               voting<SystemAccount>::decrease_voting_power( del.from, del.unstake_net_quantity + del.unstake_cpu_quantity );
            }

         } // undelegatebw

         static void on( const refund& r ) {
            require_auth( r.owner );

            refunds_table refunds_tbl( SystemAccount, r.owner );
            auto req = refunds_tbl.find( r.owner );
            eosio_assert( req != refunds_tbl.end(), "refund request not found" );
            eosio_assert( req->request_time + refund_delay <= now(), "refund is not available yet" );
            // Until now() becomes NOW, the fact that now() is the timestamp of the previous block could in theory
            // allow people to get their tokens earlier than the 3 day delay if the unstake happened immediately after many
            // consecutive missed blocks.

            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                          { N(eosio), req->owner, req->amount, std::string("unstake") } );

            refunds_tbl.erase( req );
         }
   };
}
