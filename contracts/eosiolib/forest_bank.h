/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosiolib/types.h>
extern "C" {
    bool verify_wood(uint32_t block_number, account_name account, const char* wood);

    uint32_t get_chain_head_num();

    uint32_t forest_period_number();
    uint32_t forest_space_number();
}
