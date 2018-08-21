/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosiolib/types.h>
extern "C" {
    bool verify_wood(uint32_t block_number, const account_name account, uint64_t wood);
}
