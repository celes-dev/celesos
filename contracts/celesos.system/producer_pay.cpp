#include "eosio.system.hpp"

#include <eosio.token/eosio.token.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/forest_bank.h>
#include <math.h>

namespace eosiosystem {

    const uint32_t seconds_per_year = 52 * 7 * 24 * 3600;
    const uint32_t blocks_per_day = 2 * 24 * 3600;
    const uint64_t useconds_per_day = 24 * 3600 * uint64_t(1000000);


    void system_contract::onblock(block_timestamp timestamp, account_name producer) {

        using namespace eosio;

        require_auth(N(eosio));

        uint32_t head_block_number = get_chain_head_num();

        /**
         * At startup the initial producer may not be one that is registered / elected
         * and therefore there may be no producer object for them.
         */
        if (_gstate.is_network_active) {
            auto prod = _producers.find(producer);
            if (prod != _producers.end()) {
                double fee = ORIGIN_REWARD_NUMBER / (pow(2, (head_block_number / REWARD_HALF_TIME)));
                _gstate.total_unpaid_fee = _gstate.total_unpaid_fee + fee;
                _producers.modify(prod, 0, [&](auto &p) {
                    p.unpaid_fee = p.unpaid_fee + fee;
                });
            }
        }

        if (head_block_number % (uint32_t) forest_space_number() == 0) {
            set_difficulty(calc_diff(head_block_number));
        }

        // 即将开始唱票，提前清理数据
        // ready to singing the voting
        uint32_t temp = SINGING_TICKER_SEP - (timestamp.slot - _gstate.last_producer_schedule_update.slot);
        if (temp <= 10) {
            clean_diff_stat_history(head_block_number + 10 - temp);
            clean_dirty_stat_producers(head_block_number - temp, 30);
        }

        /// only update block producers once every minute, block_timestamp is in half seconds
        if (timestamp.slot - _gstate.last_producer_schedule_update.slot >= SINGING_TICKER_SEP) {
            update_elected_producers(timestamp);

            if (_gstate.is_network_active) {
                if ((timestamp.slot - _gstate.last_name_close.slot) > blocks_per_day) {
                    name_bid_table bids(_self, _self);
                    auto idx = bids.get_index<N(highbid)>();
                    auto highest = idx.begin();
                    if (highest != idx.end() &&
                        highest->high_bid > 0 &&
                        highest->last_bid_time < (current_time() - useconds_per_day) &&
                        _gstate.thresh_activated_stake_time > 0 &&
                        (current_time() - _gstate.thresh_activated_stake_time) > 14 * useconds_per_day) {
                        _gstate.last_name_close = timestamp;
                        idx.modify(highest, 0, [&](auto &b) {
                            b.high_bid = -b.high_bid;
                        });
                    }
                }
            }
        }
    }

    using namespace eosio;

    void system_contract::claimrewards(const account_name &owner) {
        require_auth(owner);

        const auto &prod = _producers.get(owner);
        eosio_assert(prod.active(), "producer does not have an active key");

        eosio_assert(_gstate.is_network_active,
                     "the network is not actived");

        auto ct = current_time();
        eosio_assert(ct - prod.last_claim_time > REWARD_TIME_SEP, "already claimed rewards within past day");

        double rewards = (prod.unpaid_fee >= REWARD_GET_MIN) ? prod.unpaid_fee : 0.0;
        _gstate.total_unpaid_fee = _gstate.total_unpaid_fee - prod.unpaid_fee;

        _producers.modify(prod, 0, [&](auto &p) {
            p.last_claim_time = ct;
            p.unpaid_fee = 0;
        });

        if (rewards > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio.bpay), N(active)},
                                                         {N(eosio.bpay), owner, asset(static_cast<int64_t>(rewards)),
                                                          std::string("producer block pay")});
        }
    }

} //namespace eosiosystem
