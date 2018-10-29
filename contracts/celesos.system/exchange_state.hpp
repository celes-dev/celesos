#pragma once

#include <eosiolib/asset.hpp>

namespace celesossystem {
    using eosio::asset;
    using eosio::symbol_type;

    typedef double real_type;

    /**
     *  Uses Bancor math to create a 50/50 relay between two eosio::asset types. The state of the
     *  bancor exchange is entirely contained within this struct. There are no external
     *  side effects associated with using this API.
     */
    struct exchange_state {
        eosio::asset    supply;

        struct connector {
            eosio::asset balance;
            double weight = .5;

            EOSLIB_SERIALIZE( connector, (balance)(weight) )
        };

        connector base;
        connector quote;

        uint64_t primary_key()const { return supply.symbol; }

        eosio::asset convert_to_exchange( connector& c, eosio::asset in );
        eosio::asset convert_from_exchange( connector& c, eosio::asset in );
        eosio::asset convert( eosio::asset from, symbol_type to );

        EOSLIB_SERIALIZE( exchange_state, (supply)(base)(quote) )
    };

    typedef eosio::multi_index<N(rammarket), exchange_state> rammarket;

} /// namespace celesossystem
