#pragma once
#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.hpp>

namespace celes {

   class multisig : public eosio::contract {
      public:
         multisig( account_name self ):contract(self){}

         void propose();
         void approve( account_name proposer, eosio::name  proposal_name, eosio::permission_level level );
         void unapprove( account_name proposer, eosio::name  proposal_name, eosio::permission_level level );
         void cancel( account_name proposer, eosio::name  proposal_name, account_name canceler );
         void exec( account_name proposer, eosio::name  proposal_name, account_name executer );

      private:
         struct proposal {
            eosio::name                        proposal_name;
            std::vector<char>               packed_transaction;

            auto primary_key()const { return proposal_name.value; }
         };
         typedef eosio::multi_index<N(proposal),proposal> proposals;

         struct approvals_info {
            eosio::name                        proposal_name;
            std::vector<eosio::permission_level>   requested_approvals;
            std::vector<eosio::permission_level>   provided_approvals;

            auto primary_key()const { return proposal_name.value; }
         };
         typedef eosio::multi_index<N(approvals),approvals_info> approvals;
   };

} /// namespace celes
