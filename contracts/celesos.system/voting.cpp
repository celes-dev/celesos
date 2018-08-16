/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include "eosio.system.hpp"

#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>
#include <eosiolib/forest_bank.h>
#include <eosio.token/eosio.token.hpp>

#include <algorithm>
#include <cmath>

namespace eosiosystem {
    using eosio::indexed_by;
    using eosio::const_mem_fun;
    using eosio::bytes;
    using eosio::print;
    using eosio::singleton;
    using eosio::transaction;

    /**
     *  This method will create a producer_config and producer_info object for 'producer'
     *
     *  @pre producer is not already registered
     *  @pre producer to register is an account
     *  @pre authority of producer to register
     *
     */
    void system_contract::regproducer(const account_name producer, const eosio::public_key &producer_key,
                                      const std::string &url, uint16_t location) {
        eosio_assert(url.size() < 512, "url too long");
        eosio_assert(producer_key != eosio::public_key(), "public key should not be the default value");
        require_auth(producer);

        auto prod = _producers.find(producer);

        if (prod != _producers.end()) {
            _producers.modify(prod, producer, [&](producer_info &info) {
                info.producer_key = producer_key;
                info.is_active = true;
                info.url = url;
                info.location = location;
            });
        } else {
            _producers.emplace(producer, [&](producer_info &info) {
                info.owner = producer;
                info.total_votes = 0;
                info.producer_key = producer_key;
                info.is_active = true;
                info.url = url;
                info.location = location;
            });
        }
    }

    void system_contract::unregprod(const account_name producer) {
        require_auth(producer);

        const auto &prod = _producers.get(producer, "producer not found");

        _producers.modify(prod, 0, [&](producer_info &info) {
            info.deactivate();
        });
    }

    void system_contract::update_elected_producers(block_timestamp block_time) {

        _gstate.last_producer_schedule_update = block_time;

        auto idx = _producers.get_index<N(prototalvote)>();

        std::vector<std::pair<eosio::producer_key, uint16_t> > top_producers;
        top_producers.reserve(21);

        for (auto it = idx.cbegin();
             it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it) {
            top_producers.emplace_back(
                    std::pair<eosio::producer_key, uint16_t>({{it->owner, it->producer_key}, it->location}));
        }

        if (top_producers.size() < _gstate.last_producer_schedule_size) {
            return;
        }

        /// sort by producer name
        std::sort(top_producers.begin(), top_producers.end());

        std::vector<eosio::producer_key> producers;

        producers.reserve(top_producers.size());
        for (const auto &item : top_producers)
            producers.push_back(item.first);

        bytes packed_schedule = pack(producers);

        if (set_proposed_producers(packed_schedule.data(), packed_schedule.size()) >= 0) {
            _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size());
        }
    }

    uint32_t system_contract::clean_dirty_stat_producers(uint32_t block_number, uint32_t maxline) {

        auto idx = _burnproducerstatinfos.get_index<N(block_number)>();
        auto itl = idx.lower_bound(block_number);
        auto itu = idx.upper_bound(block_number);

        uint32_t round = 0;
        if (itl != idx.end()) {
            for (auto it = itl; it != itu && round < maxline; ++it, ++round) {
                auto producer = _producers.find(it->producer);

                if (producer != _producers.end()) {
                    _producers.modify(producer, 0, [&](auto &p) {
                        p.total_votes = p.total_votes - it->stat;
                    });
                }

                // 在本表中干掉此记录
                _burnproducerstatinfos.erase(*it);
            }
        }

        return maxline - round;
    }

    double system_contract::calc_diff(uint32_t block_number) {

        // 计算当前轮的目标难度
        auto last1 = _burnblockstatinfos.find(block_number - block_per_forest);
        auto last2 = _burnblockstatinfos.find(block_number - 2 * block_per_forest);
        auto last3 = _burnblockstatinfos.find(block_number - 3 * block_per_forest);

        auto diff1 = (last1 == _burnblockstatinfos.end()) ? 1 : last1->diff;
        auto wood1 = (last1 == _burnblockstatinfos.end()) ? target_wood_number : last1->stat;
        auto diff2 = (last2 == _burnblockstatinfos.end()) ? 1 : last2->diff;
        auto wood2 = (last2 == _burnblockstatinfos.end()) ? target_wood_number : last2->stat;
        auto diff3 = (last3 == _burnblockstatinfos.end()) ? 1 : last3->diff;
        auto wood3 = (last3 == _burnblockstatinfos.end()) ? target_wood_number : last3->stat;

        // 假设历史三个周期难度分别为diff1,diff2,diff3,对应提交的答案数为wood1,wood2,wood3(1为距离当前时间最短的周期)
        // 则建议难度值为:M/wood1*diff1*1/7+M/wood2*diif2*2/7+M/wood3*diff3*4/7,简化为M/7*(diff1/wood1+2*diif2/wood2+4*diff3/wood3)
        auto targetdiff = ((double) target_wood_number) / 7 * (wood1 / diff1 * 4 + wood2 / diff2 * 2 + wood3 / diff3);

        auto current = _burnblockstatinfos.find(block_number);
        if (current == _burnblockstatinfos.end()) {
            // 由系统账号为此存储付费
            _burnblockstatinfos.emplace(N(eosio), [&](auto &p) {
                p.block_number = block_number;
                p.diff = targetdiff;
            });
        } else {
            _burnblockstatinfos.modify(current, 0, [&](auto &p) {
                p.diff = targetdiff;
            });
        }

        return targetdiff;
    }

    void system_contract::clean_diff_stat_history(uint32_t block_number) {

        auto itr = _burnblockstatinfos.begin();

        while (itr != _burnblockstatinfos.end()) {
            if (itr->block_number <= block_number - 3 * block_per_forest) {
                _burnblockstatinfos.erase(itr);
            } else {
                break;
            }
        }
    }

    uint32_t system_contract::clean_dirty_wood_history(uint32_t block_number, uint32_t maxline) {

        auto idx = _burninfos.get_index<N(block_number)>();
        auto cust_itr = idx.begin();
        uint32_t round = 0;
        while (cust_itr != idx.end() && round < maxline) {
            if (cust_itr->block_number <= block_number) {
                // 在本表中干掉此记录
                _burninfos.erase(*cust_itr);
                cust_itr++;
                round++;
            } else {
                break;
            }
        }

        return maxline - round;
    }

    void system_contract::setproxy(const account_name voter_name, const account_name proxy_name) {

        require_auth(voter_name);
        if (proxy_name) {
            require_recipient(proxy_name);
        }

        auto voter = _voters.find(voter_name);

        if (proxy_name) {
            auto new_proxy = _voters.find(proxy_name);
            eosio_assert(new_proxy != _voters.end(),
                         "invalid proxy specified");
            eosio_assert(voter->proxy != proxy_name, "action has no effect");
            eosio_assert(new_proxy->is_proxy, "proxy not found");
        } else {
            eosio_assert(voter != _voters.end() && voter->proxy, "user haven't set proxy");
        }

        if (voter == _voters.end() && proxy_name) {
            _voters.emplace(voter_name, [&](auto &p) {
                p.owner = voter_name;
                p.proxy = proxy_name;
            });
        } else {
            if (proxy_name || voter->is_proxy) {
                _voters.modify(voter, 0, [&](auto &av) {
                    av.proxy = proxy_name;
                });
            } else {
                _voters.erase(voter);
            }
        }
    }

    void system_contract::voteproducer(const account_name voter_name, const account_name woodowner_name,
                                       const wood_info &wood_info, const account_name producer_name) {

        require_auth(voter_name);
        system_contract::update_vote(voter_name, woodowner_name, wood_info, producer_name);
    }

    bool system_contract::verify(const wood_info &wood, const account_name woodowner_name) {

        auto voter_wood = (((uint128_t) woodowner_name) << 64 | (uint128_t) wood.wood);
        auto idx = _burninfos.get_index<N(voter_wood)>();

        auto itl = idx.lower_bound(voter_wood);
        auto itu = idx.upper_bound(voter_wood);

        while (itl != itu) {
            if (itl->block_number == wood.block_number) {
                return false;
            }
            ++itl;
        }

        return verify_wood(wood.block_number, woodowner_name, wood.wood);
    }

    void system_contract::update_vote(const account_name voter_name, const account_name woodowner_name,
                                      const wood_info &wood_info, const account_name producer_name) {

        //validate input
        eosio_assert(producer_name > 0, "cannot vote with no producer");
        eosio_assert(wood_info.block_number > 0, "invalid wood");
        eosio_assert(wood_info.wood > 0, "invalid wood");

        if (woodowner_name && voter_name != woodowner_name) {
            auto wood_owner = _voters.find(woodowner_name);
            eosio_assert(wood_owner->proxy == voter_name, "cannot proxy for woodowner");
            require_recipient(woodowner_name);

            auto voter = _voters.find(voter_name);
            eosio_assert(voter != _voters.end() && voter->is_proxy, "voter is not a proxy");
        }

        auto &owner = woodowner_name ? woodowner_name : voter_name;

        eosio_assert(system_contract::verify(wood_info, owner), "invalid wood");

        // 记录总计投票数
        _gstate.total_activated_stake++;

        // 更新producer总投票计数
        auto &pitr = _producers.get(producer_name, "producer not found"); //data corruption
        eosio_assert(pitr.is_active, "");
        _producers.modify(pitr, 0, [&](auto &p) {
            p.total_votes++;
            if (p.total_votes < 0) { // floating point arithmetics can give small negative numbers
                p.total_votes = 0;
            }
            _gstate.total_producer_vote_weight++;
        });

        // 增加投票明细记录
        _burninfos.emplace(N(voter_name), [&](auto &burn) {
            burn.rowid = _burninfos.available_primary_key();
            burn.voter = owner;
            burn.wood = wood_info.wood;
            burn.block_number = wood_info.block_number;
        });

        // producer 统计
        auto indexofproducer = _burnproducerstatinfos.get_index<N(producer)>();

        auto itl = indexofproducer.lower_bound(producer_name);
        auto itu = indexofproducer.upper_bound(producer_name);

        bool isSuccess = false;

        while (itl != itu) {
            if (itl->block_number == wood_info.block_number) {
                isSuccess = true;
                break;
            } else {
                ++itl;
            }
        }

        if (isSuccess) {
            _burnproducerstatinfos.modify(*itl, 0, [&](auto &p) {
                p.stat++;
            });
        } else {
            _burnproducerstatinfos.emplace(N(eosio), [&](auto &p) {
                p.rowid = _burnproducerstatinfos.available_primary_key();
                p.producer = producer_name;
                p.block_number = wood_info.block_number;
                p.stat = 1;
            });
        }

        {
            auto temp = _burnblockstatinfos.find((uint64_t) wood_info.block_number);
            if (temp != _burnblockstatinfos.end()) {
                _burnblockstatinfos.modify(temp, 0, [&](auto &p) {
                    p.stat = p.stat + wood_info.block_number;
                });
            } else {
                _burnblockstatinfos.emplace(N(eosio), [&](auto &p) {
                    p.block_number = wood_info.block_number;
                    p.stat = 1;
                });
            }
        }

        {
            uint32_t max_clean_limit = 30;
            uint32_t temp = (_gstate.last_block_time.slot - wood_period) % block_per_forest;
            uint32_t remain = clean_dirty_stat_producers(_gstate.last_block_time.slot - temp, max_clean_limit);
            clean_dirty_wood_history(_gstate.last_block_time.slot - temp, remain);
        }
    }

    /**
     *  An account marked as a proxy can vote with the weight of other accounts which
     *  have selected it as a proxy. Other accounts must refresh their voteproducer to
     *  update the proxy's weight.
     *
     *  @param isproxy - true if proxy wishes to vote on behalf of others, false otherwise
     *  @pre proxy must have something staked (existing row in voters table)
     *  @pre new state must be different than current state
     */
    void system_contract::regproxy(const account_name proxy, bool isproxy) {
        require_auth(proxy);

        auto pitr = _voters.find(proxy);
        if (pitr != _voters.end()) {
            eosio_assert(isproxy != pitr->is_proxy, "action has no effect");
            eosio_assert(!isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy");
            _voters.modify(pitr, 0, [&](auto &p) {
                p.is_proxy = isproxy;
            });
        } else {
            _voters.emplace(proxy, [&](auto &p) {
                p.owner = proxy;
                p.is_proxy = isproxy;
            });
        }
    }

} /// namespace eosiosystem
