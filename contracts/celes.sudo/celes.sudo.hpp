#pragma once

#include <eosiolib/eosio.hpp>

namespace celes {

   class sudo : public eosio::contract {
      public:
         sudo( account_name self ):contract(self){}

         void exec();

   };

} /// namespace celes
