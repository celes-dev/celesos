/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/token.hpp>
#include <eosiolib/reflect.hpp>
#include <eosiolib/generic_currency.hpp>

#include <bancor/converter.hpp>
#include <currency/currency.hpp>

namespace bancor {
   typedef eosio::generic_currency< celes::token<N(other),S(4,OTHER)> >  other_currency;
   typedef eosio::generic_currency< celes::token<N(bancor),S(4,RELAY)> > relay_currency;
   typedef eosio::generic_currency< celes::token<N(currency),S(4,CUR)> > cur_currency;

   typedef converter<relay_currency, other_currency, cur_currency > example_converter;
} /// bancor

