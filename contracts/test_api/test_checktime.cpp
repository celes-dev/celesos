/**
 * @file
 * @copyright defined in eos/LICENSE.txt
 */

#include <eosiolib/eosio.hpp>
#include "test_api.hpp"

void test_checktime::checktime_pass() {
   int p = 0;
   for ( int i = 0; i < 10000; i++ )
      p += i;

   eosio::print(p);
}

void test_checktime::checktime_failure() {
   int p = 0;
   checktime_failure();
   p++;
   eosio::print(p);
}
