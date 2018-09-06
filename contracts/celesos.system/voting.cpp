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
        top_producers.reserve(BP_COUNT);

        for (auto it = idx.cbegin();
             it != idx.cend() && top_producers.size() < BP_COUNT && 0 < it->total_votes && it->active(); ++it) {
            top_producers.emplace_back(
                    std::pair<eosio::producer_key, uint16_t>({{it->owner, it->producer_key}, it->location}));
        }

        if (top_producers.size() >= BP_MIN_COUNT && !_gstate.is_network_active) {
            _gstate.is_network_active = true; // active the network
        }
        
        if (top_producers.size() >= BP_MIN_COUNT) {

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

    void
    system_contract::voteproducer(const account_name voter_name, const account_name wood_owner_name, const std::string wood,
                                  const uint32_t block_number, const account_name producer_name) {

        require_auth(voter_name);
        system_contract::update_vote(voter_name, wood_owner_name, wood, block_number, producer_name);
    }

    bool system_contract::verify(const std::string wood, const uint32_t block_number, const account_name wood_owner_name) {

        auto voter_block = (((uint128_t) wood_owner_name) << 64 | (uint128_t) block_number);
        auto idx = _burninfos.get_index<N(voter_block)>();

        auto itl = idx.lower_bound(voter_block);
        auto itu = idx.upper_bound(voter_block);

        while (itl != itu) {
            if (itl->wood == wood) {
                return false;
            }
            ++itl;
        }

        if (block_number <= 100000) {
            return true;
        } else {
            return verify_wood(block_number, wood_owner_name, wood.c_str());
        }

    }

    void system_contract::update_vote(const account_name voter_name, const account_name wood_owner_name,
                                      const std::string wood, const uint32_t block_number,
                                      const account_name producer_name) {

        //validate input
        eosio_assert(producer_name > 0, "cannot vote with no producer");
        eosio_assert(wood.length() > 0, "invalid wood 2");

#if LOG_ENABLE
        eosio::print("voter:", voter_name, ",owner:", wood_owner_name, "wood:", wood, ",block:", block_number,
                     ",producer:", producer_name, "\r\n");
#endif

        if (wood_owner_name && voter_name != wood_owner_name) {
            auto wood_owner = _voters.find(wood_owner_name);
            eosio_assert(wood_owner->proxy == voter_name, "cannot proxy for woodowner");
            require_recipient(wood_owner_name);

            auto voter = _voters.find(voter_name);
            eosio_assert(voter != _voters.end() && voter->is_proxy, "voter is not a proxy");
        }

        auto &owner = wood_owner_name ? wood_owner_name : voter_name;

        eosio_assert(system_contract::verify(wood, block_number, owner), "invalid wood 3");

        // 记录总计投票数
        _gstate.total_activated_stake++;

        // 更新producer总投票计数
        auto &pitr = _producers.get(producer_name, "producer not found"); //data corruption
        eosio_assert(pitr.is_active, "");
        _producers.modify(pitr, 0, [&](auto &p) {
            p.total_votes++;
            _gstate.total_producer_vote_weight++;
        });

#if LOG_ENABLE
        eosio::print("add wood detail:", wood, ",voter:", owner, ",block:", block_number, "\r\n");
#endif

        // 增加投票明细记录
        _burninfos.emplace(N(eosio), [&](auto &burn) {
            burn.rowid = _burninfos.available_primary_key();
            burn.voter = owner;
            burn.wood = wood;
            burn.block_number = block_number;
        });

        // producer 统计
        auto indexofproducer = _burnproducerstatinfos.get_index<N(producer)>();

        auto itl = indexofproducer.lower_bound(producer_name);
        auto itu = indexofproducer.upper_bound(producer_name);

        bool isSuccess = false;

        while (itl != itu) {
            if (itl->block_number == block_number) {
                isSuccess = true;
                break;
            } else {
                ++itl;
            }
        }


        if (isSuccess) {
#if LOG_ENABLE
            eosio::print("modify stat\r\n");
#endif

            _burnproducerstatinfos.modify(*itl, 0, [&](auto &p) {
                p.stat++;
            });
        } else {
#if LOG_ENABLE
            eosio::print("add stat,producer:", producer_name, ",block:", block_number, "\r\n");
#endif

            _burnproducerstatinfos.emplace(N(eosio), [&](auto &p) {
                p.rowid = _burnproducerstatinfos.available_primary_key();
                p.producer = producer_name;
                p.block_number = block_number;
                p.stat = 1;
            });
        }

        {
            auto temp = _burnblockstatinfos.find((uint64_t) block_number);
            if (temp != _burnblockstatinfos.end()) {
#if LOG_ENABLE
                eosio::print("modify block stat\r\n");
#endif
                _burnblockstatinfos.modify(temp, 0, [&](auto &p) {
                    p.stat = p.stat + 1;
                });
            } else {
#if LOG_ENABLE
                eosio::print("add block stat,block:", block_number, "\r\n");
#endif
                _burnblockstatinfos.emplace(N(eosio), [&](auto &p) {
                    p.block_number = block_number;
                    p.stat = 1;
                    p.diff = 1;
                });
            }
        }

        {
            uint32_t head_block_number = get_chain_head_num();

            if (head_block_number > wood_period) {
#if LOG_ENABLE
                eosio::print("clean,head_block_number:", head_block_number, ",period:", wood_period, "\r\n");
#endif
                uint32_t max_clean_limit = 30;
                uint32_t temp = (head_block_number - wood_period) % block_per_forest;
#if LOG_ENABLE
                eosio::print("clean,temp:", temp, "\r\n");
#endif
                uint32_t remain = clean_dirty_stat_producers(head_block_number - temp, max_clean_limit);
#if LOG_ENABLE
                eosio::print("clean,remain:", remain, "\r\n");
#endif
                clean_dirty_wood_history(head_block_number - temp, remain);
            }
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


    uint32_t system_contract::clean_dirty_stat_producers(uint32_t block_number, uint32_t maxline) {

        auto idx = _burnproducerstatinfos.get_index<N(block_number)>();
        auto itl = idx.lower_bound(block_number);
        auto itu = idx.upper_bound(block_number);

        std::vector<wood_burn_producer_block_stat> producer_stat_vector;

        uint32_t round = 0;
        if (itl != itu) {
            for (auto it = itl; it != itu && round < maxline; ++it, ++round) {
                auto producer = _producers.find(it->producer);

                if (producer != _producers.end()) {
                    _producers.modify(producer, 0, [&](auto &p) {
#if LOG_ENABLE
                        eosio::print("clean,total:", p.total_votes, ",this:", it->stat, "\r\n");
#endif
                        p.total_votes = p.total_votes - it->stat;
                    });
                }

                // delete record
                producer_stat_vector.emplace_back(*it);
            }
        }

        for (auto temp : producer_stat_vector) {
#if LOG_ENABLE
            eosio::print("clean,stat:", temp.stat, ",block:", temp.block_number, "\r\n");
#endif

            _burnproducerstatinfos.erase(temp);
        }

        return maxline - round;
    }

    void system_contract::onblock_clean_burn_stat(uint32_t block_number, uint32_t maxline) {

        auto idx = _producers.get_index<N(prototalvote)>();

        uint32_t round = 0;
        std::vector<wood_burn_producer_block_stat> producer_stat_vector;

        for (auto it = idx.cbegin();
             it != idx.cend() && round < maxline && 0 < it->total_votes; ++it, ++round) {

            auto bidx = _burnproducerstatinfos.get_index<N(producer)>();
            auto itl = bidx.lower_bound(it->owner);
            auto itu = bidx.upper_bound(it->owner);

            if (itl != itu) {
                for (auto temp = itl; temp != itu; ++it, ++round) {
                    if (temp->block_number == block_number) {
                        auto producer = _producers.find(it->owner);

                        if (producer != _producers.end()) {
                            _producers.modify(producer, 0, [&](auto &p) {
                                p.total_votes = p.total_votes - temp->stat;
                            });
                        }
                    }

                    // delete record
                    producer_stat_vector.emplace_back(*temp);
                }
            }
        }

        for (auto temp : producer_stat_vector) {
#if LOG_ENABLE
            eosio::print("onblock clean,producer:", temp.producer, ",stat:", temp.stat, ",block:", temp.block_number,
                         "\r\n");
#endif

            _burnproducerstatinfos.erase(temp);
        }
    }

    /**
     * calc suggest diff
     *
     * @param block_number current block umber
     * @return sugeest diff
     */
    double system_contract::calc_diff(uint32_t block_number, account_name producer) {

        auto last1 = _burnblockstatinfos.find(block_number - block_per_forest);
        auto diff1 = ((last1 == _burnblockstatinfos.end()) ? 1 : last1->diff);
        auto wood1 = ((last1 == _burnblockstatinfos.end()) ? target_wood_number : last1->stat);
        auto last2 = _burnblockstatinfos.find(block_number - 2 * block_per_forest);
        auto diff2 = ((last2 == _burnblockstatinfos.end()) ? 1 : last2->diff);
        auto wood2 = ((last2 == _burnblockstatinfos.end()) ? target_wood_number : last2->stat);
        auto last3 = _burnblockstatinfos.find(block_number - 3 * block_per_forest);
        auto diff3 = ((last3 == _burnblockstatinfos.end()) ? 1 : last3->diff);
        auto wood3 = ((last3 == _burnblockstatinfos.end()) ? target_wood_number : last3->stat);

        // Suppose the last 3 cycle,the diff is diff1,diff2,diff2, and the answers count is wood1,wood2,wood3
        // 假设历史三个周期难度分别为diff1,diff2,diff3,对应提交的答案数为wood1,wood2,wood3(1为距离当前时间最短的周期)
        // so suggest diff is:M/wood1*diff1*1/7+M/wood2*diif2*2/7+M/wood3*diff3*4/7,Simplified to M/7*(diff1/wood1+2*diif2/wood2+4*diff3/wood3)
        // 则建议难度值为M/wood1*diff1*1/7+M/wood2*diif2*2/7+M/wood3*diff3*4/7,简化为M/7*(diff1/wood1+2*diif2/wood2+4*diff3/wood3)
        double targetdiff = ((double) target_wood_number) / 7 * (diff1 / (wood1 ? wood1 : 1) * 4 + diff2 / (wood2 ? wood2 : 1) * 2 + diff3 / (wood3 ? wood3 : 1));
        auto current = _burnblockstatinfos.find(block_number);
        if (current == _burnblockstatinfos.end()) {
            // payer is the system account
            _burnblockstatinfos.emplace(producer, [&](auto &p) {
                p.block_number = block_number;
                p.diff = targetdiff;
            });
        }
        else {
            _burnblockstatinfos.modify(current, 0, [&](auto &p) {
                p.diff = targetdiff;
            });
        }
        return targetdiff;
    }

    void system_contract::clean_diff_stat_history(uint32_t block_number) {

        auto itr = _burnblockstatinfos.begin();

        std::vector<wood_burn_block_stat> stat_vector;
        while (itr != _burnblockstatinfos.end()) {
            if (itr->block_number <= block_number - 3 * block_per_forest) {
                stat_vector.emplace_back(*itr);
            } else {
                break;
            }
        }

        for (auto temp : stat_vector) {
#if LOG_ENABLE
            eosio::print("clean_stat:",temp.block_number);
#endif
            _burnblockstatinfos.erase(temp);
        }
    }

    uint32_t system_contract::clean_dirty_wood_history(uint32_t block_number, uint32_t maxline) {

        auto idx = _burninfos.get_index<N(block_number)>();
        auto cust_itr = idx.begin();
        uint32_t round = 0;

        std::vector<wood_burn_info> wood_vector;
        while (cust_itr != idx.end() && round < maxline) {
            if (cust_itr->block_number <= block_number) {
                // delete record
                cust_itr++;
                round++;
            } else {
                break;
            }
        }

        for (auto temp : wood_vector) {
#if LOG_ENABLE
            eosio::print("clean,wood:", temp.wood, ",block:", temp.block_number, ",owner:", temp.voter, "\r\n");
#endif
            _burninfos.erase(temp);
        }

        return maxline - round;
    }

} /// namespace eosiosystem
