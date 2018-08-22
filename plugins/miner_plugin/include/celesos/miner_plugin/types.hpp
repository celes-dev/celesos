//
// Created by yalez on 2018/8/20.
//

#ifndef CELESOS_MINER_PLUGIN_TYPES_H
#define CELESOS_MINER_PLUGIN_TYPES_H

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/signals2.hpp>
#include <boost/optional.hpp>
#include <eosio/chain/types.hpp>

namespace celesos {
    namespace miner {
        using mine_callback_type = void(bool is_success,
                                        eosio::chain::block_num_type block_num,
                                        const boost::optional<boost::multiprecision::uint256_t> &);
        using mine_slot_type = std::function<mine_callback_type>;
        using mine_signal_type = boost::signals2::signal<mine_callback_type>;
        using mine_signal_ptr_type = std::shared_ptr<mine_signal_type>;
    }
}

#endif //EOSIO_TYPES_H
