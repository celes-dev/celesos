/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/contract_types.hpp>

namespace eosio { namespace chain {

   class apply_context;

   /**
    * @defgroup native_action_handlers Native Action Handlers
    */
   ///@{
   void apply_celes_newaccount(apply_context&);
   void apply_celes_updateauth(apply_context&);
   void apply_celes_deleteauth(apply_context&);
   void apply_celes_linkauth(apply_context&);
   void apply_celes_unlinkauth(apply_context&);

   /*
   void apply_celes_postrecovery(apply_context&);
   void apply_celes_passrecovery(apply_context&);
   void apply_celes_vetorecovery(apply_context&);
   */

   void apply_celes_setcode(apply_context&);
   void apply_celes_setabi(apply_context&);

   void apply_celes_canceldelay(apply_context&);
   ///@}  end action handlers

} } /// namespace eosio::chain
