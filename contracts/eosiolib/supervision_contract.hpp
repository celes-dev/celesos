//
// Created by cc on 18-8-29.
//

#include "contract.hpp"
#include "types.h"

#ifndef EOSIO_SUPERVISION_CONTRACT_HPP
#define EOSIO_SUPERVISION_CONTRACT_HPP

#endif //EOSIO_SUPERVISION_CONTRACT_HPP
namespace eosio {

    class supervision_contract : public contract {
    public:
        using contract::contract;

        void supervision(account_name account,
                         account_name  code,
                         action_name  type,
                         permission_name requirement);

    };
}