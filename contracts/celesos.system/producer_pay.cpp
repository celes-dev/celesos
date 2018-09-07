#include "eosio.system.hpp"

#include <eosio.token/eosio.token.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/forest_bank.h>

namespace eosiosystem {

    const int64_t min_pervote_daily_pay = 100'0000;
//    const int64_t min_activated_stake = 150'000'000'0000;
    const int64_t min_activated_stake = 1;
    const double continuous_rate = 0.04879;          // 5% annual rate
    const double perblock_rate = 0.0025;           // 0.25%
    const double standby_rate = 0.0075;           // 0.75%
    const uint32_t blocks_per_year = 52 * 7 * 24 * 2 * 3600;   // half seconds per year
    const uint32_t seconds_per_year = 52 * 7 * 24 * 3600;
    const uint32_t blocks_per_day = 2 * 24 * 3600;
    const uint32_t blocks_per_hour = 2 * 3600;
    const uint64_t useconds_per_day = 24 * 3600 * uint64_t(1000000);
    const uint64_t useconds_per_year = seconds_per_year * 1000000ll;


    void system_contract::onblock(block_timestamp timestamp, account_name producer) {

        using namespace eosio;

        require_auth(N(eosio));

        /**
         * At startup the initial producer may not be one that is registered / elected
         * and therefore there may be no producer object for them.
         */
        if (_gstate.is_network_active) {

            if (_gstate.last_pervote_bucket_fill == 0)  /// start the presses
                _gstate.last_pervote_bucket_fill = current_time();

            auto prod = _producers.find(producer);
            if (prod != _producers.end()) {
                _gstate.total_unpaid_blocks++;
                _producers.modify(prod, 0, [&](auto &p) {
                    p.unpaid_blocks++;
                });
            }
        }

        uint32_t head_block_number = get_chain_head_num();

#if LOG_ENABLE
        print("block:", head_block_number);
#endif

        if (head_block_number >= wood_period) {
            uint32_t temp = (head_block_number + 10 - wood_period) % block_per_forest;
            if (temp <= 10) {
                clean_diff_stat_history(head_block_number + 10 - temp);
                clean_dirty_stat_producers(head_block_number - temp, 30);
            }
        }

        if (head_block_number % block_per_forest == 0) {
            double diff = calc_diff(head_block_number, producer);
            set_difficulty(diff);
        }

        /// only update block producers once every minute, block_timestamp is in half seconds
        if (timestamp.slot - _gstate.last_producer_schedule_update.slot > 120) {
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

        eosio_assert(ct - prod.last_claim_time > useconds_per_day, "already claimed rewards within past day");

        const asset token_supply = token(N(eosio.token)).get_supply(symbol_type(system_token_symbol).name());
        const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

        if (usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0) {
            auto new_tokens = static_cast<int64_t>(
                    (continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) /
                    double(useconds_per_year));

            auto to_producers = new_tokens / 5;
            auto to_savings = new_tokens - to_producers;
            auto to_per_block_pay = to_producers / 4;
            auto to_per_vote_pay = to_producers - to_per_block_pay;

            INLINE_ACTION_SENDER(eosio::token, issue)(N(eosio.token), {{N(eosio), N(active)}},
                                                      {N(eosio), asset(new_tokens),
                                                       std::string("issue tokens for producer pay and savings")});

            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio), N(active)},
                                                         {N(eosio), N(eosio.saving), asset(to_savings),
                                                          "unallocated inflation"});

            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio), N(active)},
                                                         {N(eosio), N(eosio.bpay), asset(to_per_block_pay),
                                                          "fund per-block bucket"});

            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio), N(active)},
                                                         {N(eosio), N(eosio.vpay), asset(to_per_vote_pay),
                                                          "fund per-vote bucket"});

            _gstate.pervote_bucket += to_per_vote_pay;
            _gstate.perblock_bucket += to_per_block_pay;

            _gstate.last_pervote_bucket_fill = ct;
        }

        int64_t producer_per_block_pay = 0;
        if (_gstate.total_unpaid_blocks > 0) {
            producer_per_block_pay = (_gstate.perblock_bucket * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
        }
        int64_t producer_per_vote_pay = 0;
        if (_gstate.total_producer_vote_weight > 0) {
            producer_per_vote_pay = int64_t(
                    (_gstate.pervote_bucket * prod.total_votes) / _gstate.total_producer_vote_weight);
        }
        if (producer_per_vote_pay < min_pervote_daily_pay) {
            producer_per_vote_pay = 0;
        }
        _gstate.pervote_bucket -= producer_per_vote_pay;
        _gstate.perblock_bucket -= producer_per_block_pay;
        _gstate.total_unpaid_blocks -= prod.unpaid_blocks;

        _producers.modify(prod, 0, [&](auto &p) {
            p.last_claim_time = ct;
            p.unpaid_blocks = 0;
        });

        if (producer_per_block_pay > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio.bpay), N(active)},
                                                         {N(eosio.bpay), owner, asset(producer_per_block_pay),
                                                          std::string("producer block pay")});
        }
        if (producer_per_vote_pay > 0) {
            INLINE_ACTION_SENDER(eosio::token, transfer)(N(eosio.token), {N(eosio.vpay), N(active)},
                                                         {N(eosio.vpay), owner, asset(producer_per_vote_pay),
                                                          std::string("producer vote pay")});
        }
    }

} //namespace eosiosystem
