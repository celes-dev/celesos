/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include "celesos.system.hpp"

#include <celes.token/celes.token.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/datastream.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/forest_bank.h>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>

#include <algorithm>
#include <cmath>

#include <stdio.h>
#include <time.h>

namespace celesossystem
{
using eosio::bytes;
using eosio::const_mem_fun;
using eosio::indexed_by;
using eosio::singleton;
using eosio::transaction;

/**
     *  This method will create a producer_config and producer_info object for
 * 'producer'
     *
     *  @pre producer is not already registered
     *  @pre producer to register is an account
     *  @pre authority of producer to register
     *
     */
void system_contract::regproducer(const account_name producer,
                                  const eosio::public_key &producer_key,
                                  const std::string &url, uint16_t location)
{
    eosio_assert(url.size() < 512, "url too long");
    eosio_assert(producer_key != eosio::public_key(),
                 "public key should not be the default value");
    require_auth(producer);

    auto prod = _producers.find(producer);

    if (prod != _producers.end())
    {
        _producers.modify(prod, producer, [&](producer_info &info) {
            info.producer_key = producer_key;
            info.is_active = true;
            info.url = url;
            info.location = location;
        });
    }
    else
    {
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

void system_contract::unregprod(const account_name producer)
{
    require_auth(producer);

    const auto &prod = _producers.get(producer, "producer not found");

    _producers.modify(prod, 0, [&](producer_info &info) { info.deactivate(); });
}

void system_contract::update_elected_producers(uint32_t head_block_number)
{

    _gstate.last_producer_schedule_block = head_block_number;

    auto idx = _producers.get_index<N(prototalvote)>();

    std::vector<std::pair<eosio::producer_key, uint16_t>> top_producers;
    top_producers.reserve(BP_COUNT);

    for (auto it = idx.cbegin();
         it != idx.cend() && top_producers.size() < BP_COUNT &&
         0 < it->total_votes && it->active();
         ++it)
    {
        top_producers.emplace_back(std::pair<eosio::producer_key, uint16_t>(
            {{it->owner, it->producer_key}, it->location}));
    }

    if (!_gstate.is_network_active)
    {
        if (top_producers.size() >= BP_COUNT)
        {
            _gstate.active_touch_count++;
        }
        else
        {
            _gstate.active_touch_count = 0;
        }

        if (_gstate.active_touch_count >= ACTIVE_NETWORK_CYCLE)
        {
            _gstate.is_network_active = true;
        }
    }

    if (_gstate.is_network_active && top_producers.size() >= BP_COUNT)
    {

        /// sort by producer name
        std::sort(top_producers.begin(), top_producers.end());

        std::vector<eosio::producer_key> producers;

        producers.reserve(top_producers.size());
        for (const auto &item : top_producers)
            producers.push_back(item.first);

        bytes packed_schedule = pack(producers);

        if (set_proposed_producers(packed_schedule.data(),
                                   packed_schedule.size()) >= 0)
        {
            _gstate.last_producer_schedule_size =
                static_cast<decltype(_gstate.last_producer_schedule_size)>(
                    top_producers.size());
        }
    }
}

void system_contract::setproxy(const account_name voter_name,
                                const account_name proxy_name)
{
    require_auth(voter_name);

    eosio_assert(voter_name != proxy_name, "can not set proxy to self");

    if (proxy_name)
    {
        require_recipient(proxy_name);

        auto new_proxy = _voters.find(proxy_name);
        eosio_assert(new_proxy != _voters.end(), "invalid proxy specified");
        eosio_assert(new_proxy->is_proxy, "proxy not found");
    }

    auto voter = _voters.find(voter_name);
    eosio_assert(voter != _voters.end(), "voter is not found");
    eosio_assert(voter->proxy != proxy_name, "action has no effect");

    _voters.modify(voter, 0, [&](auto &av) {
        av.proxy = proxy_name;
    });
}

void system_contract::voteproducer(const account_name voter_name,
                                   const account_name wood_owner_name,
                                   const std::string wood,
                                   const uint32_t block_number,
                                   const account_name producer_name)
{

    require_auth(voter_name);
    system_contract::update_vote(voter_name, wood_owner_name, wood, block_number,
                                 producer_name);
}

bool system_contract::verify(const std::string wood,
                             const uint32_t block_number,
                             const account_name wood_owner_name)
{
    auto woodkey = wood_burn_info::woodkey(wood);
    auto idx = _burninfos.get_index<N(wood)>();

    auto itl = idx.lower_bound(woodkey);
    auto itu = idx.upper_bound(woodkey);

    while (itl != itu)
    {
        if (itl->wood == wood && itl->block_number == block_number &&
            itl->voter == wood_owner_name)
        {
            return false;
        }

        itl++;
    }

    return verify_wood(block_number, wood_owner_name, wood.c_str());
}

void system_contract::update_vote(const account_name voter_name,
                                  const account_name wood_owner_name,
                                  const std::string wood,
                                  const uint32_t block_number,
                                  const account_name producer_name)
{
    // validate input
    eosio_assert(producer_name > 0, "cannot vote with no producer");
    eosio_assert(wood.length() > 0, "invalid wood 2");

    if (wood_owner_name && voter_name != wood_owner_name)
    {
        auto wood_owner = _voters.find(wood_owner_name);
        eosio_assert(wood_owner->proxy == voter_name, "cannot proxy for woodowner");
        require_recipient(wood_owner_name);

        auto voter = _voters.find(voter_name);
        eosio_assert(voter != _voters.end() && voter->is_proxy,
                     "voter is not a proxy");
    }

    auto &owner = wood_owner_name ? wood_owner_name : voter_name;
    eosio_assert(system_contract::verify(wood, block_number, owner),
                 "invalid wood 3");

    // 更新producer总投票计数
    auto &pitr =
        _producers.get(producer_name, "producer not found"); // data corruption
    eosio_assert(pitr.is_active, "producer is not active");

    _producers.modify(pitr, 0, [&](auto &p) {
        p.total_votes++;
    });

    _gstate.total_producer_vote_weight++;

    // 增加投票明细记录
    _burninfos.emplace(N(celes), [&](auto &burn) {
        burn.rowid = _burninfos.available_primary_key();
        burn.voter = owner;
        burn.wood = wood;
        burn.block_number = block_number;
    });

    // producer 统计
    auto indexofproducer = _burnproducerstatinfos.get_index<N(producerblock)>();

    auto bpblockkey =
        wood_burn_producer_block_stat::bpblockkey(producer_name, block_number);

    auto itl = indexofproducer.lower_bound(bpblockkey);
    auto itu = indexofproducer.upper_bound(bpblockkey);

    if (itl != itu)
    {
        _burnproducerstatinfos.modify(*itl, 0, [&](auto &p) { p.stat++; });
    }
    else
    {
        _burnproducerstatinfos.emplace(N(celes), [&](auto &p) {
            p.rowid = _burnproducerstatinfos.available_primary_key();
            p.producer = producer_name;
            p.block_number = block_number;
            p.stat = 1;
        });
    }

    {
        auto temp = _burnblockstatinfos.find(block_number);
        if (temp != _burnblockstatinfos.end())
        {
            _burnblockstatinfos.modify(temp, 0,
                                       [&](auto &p) { p.stat = p.stat + 1; });
        }
        else
        {
            _burnblockstatinfos.emplace(N(celes), [&](auto &p) {
                p.block_number = block_number;
                p.stat = 1;
                p.diff = 1;
            });
        }
    }

    // 记录总计投票数
    _gstate.total_activated_stake++;

    {
        uint32_t head_block_number = get_chain_head_num();

        if (head_block_number > forest_period_number())
        {
            uint32_t max_clean_limit = 5;
            uint32_t remain =
                clean_dirty_stat_producers(head_block_number, max_clean_limit);
            clean_dirty_wood_history(head_block_number, remain);
        }
    }
}

/**
     *  An account marked as a proxy can vote with the weight of other accounts
 * which
     *  have selected it as a proxy. Other accounts must refresh their
 * voteproducer tosub total_voteru
     *  update the proxy's weight.
     *
     *  @param isproxy - true if proxy wishes to vote on behalf of others, false
 * otherwise
     *  @pre proxy must have something staked (existing row in voters table)
     *  @pre new state must be different than current state
     */
void system_contract::regproxy(const account_name proxy, bool isproxy)
{
    require_auth(proxy);

    auto pitr = _voters.find(proxy);
    if (pitr != _voters.end())
    {
        eosio_assert(isproxy != pitr->is_proxy, "action has no effect");
        eosio_assert(!isproxy || !pitr->proxy,
                     "account that uses a proxy is not allowed to become a proxy");
        _voters.modify(pitr, 0, [&](auto &p) { p.is_proxy = isproxy; });
    }
    else
    {
        _voters.emplace(proxy, [&](auto &p) {
            p.owner = proxy;
            p.is_proxy = isproxy;
        });
    }
}

uint32_t system_contract::clean_dirty_stat_producers(uint32_t block_number,
                                                     uint32_t maxline)
{

    if (block_number <= forest_period_number())
        return 0;

    auto idx = _burnproducerstatinfos.get_index<N(block_number)>();
    auto itl = idx.begin();
    auto itu = idx.lower_bound(block_number - forest_period_number());

    std::vector<wood_burn_producer_block_stat> producer_stat_vector;

    uint32_t round = 0;
    if (itl != itu)
    {
        for (auto it = itl; it != itu && round < maxline; ++it, ++round)
        {
            auto producer = _producers.find(it->producer);

            if (producer != _producers.end())
            {
                _producers.modify(producer, 0, [&](auto &p) {
                    p.total_votes = p.total_votes - it->stat;
                });
            }

            // delete record
            producer_stat_vector.emplace_back(*it);
        }
    }

    for (auto temp : producer_stat_vector)
    {
        auto itr = _burnproducerstatinfos.find(temp.rowid);
        if (itr != _burnproducerstatinfos.end())
        {
            _burnproducerstatinfos.erase(itr);
        }
    }

    return maxline - round;
}

/**
     * calc suggest diff
     *
     * @param block_number current block umber
     * @return sugeest diff
     */
double system_contract::calc_diff(uint32_t block_number)
{
    auto last1 =
        _burnblockstatinfos.find(block_number - (uint32_t)forest_space_number());
    auto diff1 = ((last1 == _burnblockstatinfos.end()) ? 1 : last1->diff);
    auto wood1 =
        ((last1 == _burnblockstatinfos.end()) ? TARGET_WOOD_NUMBER : last1->stat);
    auto last2 = _burnblockstatinfos.find(block_number -
                                          2 * (uint32_t)forest_space_number());
    auto diff2 = ((last2 == _burnblockstatinfos.end()) ? 1 : last2->diff);
    auto wood2 =
        ((last2 == _burnblockstatinfos.end()) ? TARGET_WOOD_NUMBER : last2->stat);
    auto last3 = _burnblockstatinfos.find(block_number -
                                          3 * (uint32_t)forest_space_number());
    auto diff3 = ((last3 == _burnblockstatinfos.end()) ? 1 : last3->diff);
    auto wood3 =
        ((last3 == _burnblockstatinfos.end()) ? TARGET_WOOD_NUMBER : last3->stat);

    // Suppose the last 3 cycle,the diff is diff1,diff2,diff2, and the answers
    // count is wood1,wood2,wood3
    // 假设历史三个周期难度分别为diff1,diff2,diff3,对应提交的答案数为wood1,wood2,wood3(1为距离当前时间最短的周期)
    // so suggest diff
    // is:wood1/M*diff1*4/7+wood1/M*diif2*2/7+wood1/M*diff3/7,Simplified to
    // (wood1*diff1*4+wood2*diff2*2+wood3*diff3)/7/M
    // 则建议难度值为wood1/M*diff1*4/7+wood1/M*diif2*2/7+wood1/M*diff3/7,简化为(wood1*diff1*4+wood2*diff2*2+wood3*diff3)/7/M
    double targetdiff = (wood1 * diff1 * 4 + wood2 * diff2 * 2 + wood3 * diff3) / TARGET_WOOD_NUMBER / 7;
    if (targetdiff <= .0001)
    {
        targetdiff = 0.0001;
    }
    auto current = _burnblockstatinfos.find(block_number);
    if (current == _burnblockstatinfos.end())
    {
        // payer is the system account
        _burnblockstatinfos.emplace(N(celes), [&](auto &p) {
            p.block_number = block_number;
            p.diff = targetdiff;
            p.stat = 0;
        });
    }
    else
    {
        _burnblockstatinfos.modify(current, 0,
                                   [&](auto &p) { p.diff = targetdiff; });
    }

    return targetdiff;
}

void system_contract::clean_diff_stat_history(uint32_t block_number)
{
    eosio::print("clean_diff_stat_history---------------");

    auto itr = _burnblockstatinfos.begin();

    std::vector<wood_burn_block_stat> stat_vector;
    while (itr != _burnblockstatinfos.end())
    {
        if (itr->block_number + 3 * (uint32_t)forest_space_number() <
            block_number)
        {
            eosio::print("clean_diff_stat_history---------------2");
            stat_vector.emplace_back(*itr);
            itr++;
        }
        else
        {
            break;
        }
    }

    for (auto temp : stat_vector)
    {
        auto temp2 = _burnblockstatinfos.find(temp.block_number);
        if (temp2 != _burnblockstatinfos.end())
        {
            eosio::print("clean_diff_stat_history---------------3");
            _burnblockstatinfos.erase(temp2);
        }
    }
}

uint32_t system_contract::clean_dirty_wood_history(uint32_t block_number,
                                                   uint32_t maxline)
{

    auto idx = _burninfos.get_index<N(block_number)>();
    auto cust_itr = idx.begin();
    uint32_t round = 0;

    std::vector<wood_burn_info> wood_vector;
    while (cust_itr != idx.end() && round < maxline)
    {
        if (cust_itr->block_number < block_number - forest_period_number())
        {
            // delete record
            wood_vector.emplace_back(*cust_itr);
            cust_itr++;
            round++;
        }
        else
        {
            break;
        }
    }

    for (auto temp : wood_vector)
    {
        auto itr = _burninfos.find(temp.rowid);
        if (itr != _burninfos.end())
        {
            _burninfos.erase(itr);
        }
    }

    return maxline - round;
}

} // namespace celesossystem
