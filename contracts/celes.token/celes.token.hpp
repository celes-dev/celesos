/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

namespace celesosystem {
   class system_contract;
}

namespace celes {

   using std::string;

   class token : public eosio::contract {
      public:
         token( account_name self ):contract(self){}

         void create( account_name issuer,
                      eosio::asset        maximum_supply);

         void issue( account_name to, eosio::asset quantity, string memo );

         void transfer( account_name from,
                        account_name to,
                        eosio::asset        quantity,
                        string       memo );
      
      
         inline eosio::asset get_supply( eosio::symbol_name  sym )const;
         
         inline eosio::asset get_balance( account_name owner, eosio::symbol_name  sym )const;

      private:
         struct account {
            eosio::asset    balance;

            uint64_t primary_key()const { return balance.symbol.name(); }
         };

         struct currency_stats {
            eosio::asset          supply;
            eosio::asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
         };

         typedef eosio::multi_index<N(accounts), account> accounts;
         typedef eosio::multi_index<N(stat), currency_stats> stats;

         void sub_balance( account_name owner, eosio::asset value );
         void add_balance( account_name owner, eosio::asset value, account_name ram_payer );

      public:
         struct transfer_args {
            account_name  from;
            account_name  to;
            eosio::asset         quantity;
            string        memo;
         };
   };

   eosio::asset token::get_supply( eosio::symbol_name  sym )const
   {
      stats statstable( _self, sym );
      const auto& st = statstable.get( sym );
      return st.supply;
   }

   eosio::asset token::get_balance( account_name owner, eosio::symbol_name  sym )const
   {
      accounts accountstable( _self, owner );
      const auto& ac = accountstable.get( sym );
      return ac.balance;
   }

} /// namespace eosio
