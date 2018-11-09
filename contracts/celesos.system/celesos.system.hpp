/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <celesos.system/native.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/fixed_key.hpp>
#include <celesos.system/exchange_state.hpp>

#include <musl/upstream/include/bits/stdint.h>
#include <cstdlib>
#include <string>

#include <eosiolib/forest_bank.h>

// 21*100000000*60% 总量21亿的百分之六十用于此奖励
#define REWARD_HALF_TIME  1260000000

#ifdef DEBUG

#define TARGET_WOOD_NUMBER 120
// number of bp,BP个数
#define BP_COUNT 9
// when the bp count is ok cycle for this number,the active the network(主网启动条件，BP个数达标轮数）
#define ACTIVE_NETWORK_CYCLE 10
// origin reward number (初始出块奖励，折半衰减）
#define ORIGIN_REWARD_NUMBER 10000
// reward get min（if smaller than this number，you can't get the reward）最小奖励领取数，低于此数字将领取失败
#define REWARD_GET_MIN 1000000
// get reward time sep(奖励领取间隔时间，单位：秒）
#define REWARD_TIME_SEP 5*60*uint64_t(1000000)
// singing ticker sep（唱票间隔期，每隔固定时间进行唱票）
#define SINGING_TICKER_SEP 5*2*6*10

#else

#define TARGET_WOOD_NUMBER 80

// number of bp,BP个数
#define BP_COUNT 21
// when the bp count is ok cycle for this number,the active the network(主网启动条件，BP个数达标轮数）
#define ACTIVE_NETWORK_CYCLE 24
// origin reward number (初始出块奖励，折半衰减）
#define ORIGIN_REWARD_NUMBER 10000
// reward get min（if smaller than this number，you can't get the reward）最小奖励领取数，低于此数字将领取失败
#define REWARD_GET_MIN 1000000
// get reward time sep(奖励领取间隔时间，单位：秒）
#define REWARD_TIME_SEP 24*60*60*uint64_t(1000000)
// singing ticker sep（唱票间隔期，每隔固定时间进行唱票）
#define SINGING_TICKER_SEP BP_COUNT*6*60

#endif

namespace celesossystem {

    using eosio::asset;
    using eosio::indexed_by;
    using eosio::const_mem_fun;
    using eosio::block_timestamp;

    struct name_bid {
        account_name newname;
        account_name high_bidder;
        int64_t high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
        uint64_t last_bid_time = 0;

        auto primary_key() const { return newname; }

        uint64_t by_high_bid() const { return static_cast<uint64_t>(-high_bid); }
    };

    typedef eosio::multi_index<N(namebids), name_bid,
            indexed_by<N(highbid), const_mem_fun<name_bid, uint64_t, &name_bid::by_high_bid> >
    > name_bid_table;


    struct eosio_global_state : eosio::blockchain_parameters {
        uint64_t free_ram() const { return max_ram_size - total_ram_bytes_reserved; }

        uint64_t max_ram_size = 64ll * 1024 * 1024 * 1024;
        uint64_t total_ram_bytes_reserved = 0;
        int64_t total_ram_stake = 0;

        uint32_t last_producer_schedule_block;
        uint64_t total_unpaid_fee = 0; /// all blocks which have been produced but not paid
        int64_t total_activated_stake = 0;
        uint64_t thresh_activated_stake_time = 0;
        uint16_t last_producer_schedule_size = 0;
        uint32_t total_producer_vote_weight = 0; /// the sum of all producer votes
        block_timestamp last_name_close;
        bool is_network_active = false;
        uint16_t active_touch_count = 0;


        uint64_t last_account = 0;

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE_DERIVED(eosio_global_state, eosio::blockchain_parameters,
                                 (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                         (last_producer_schedule_block)
                                         (total_unpaid_fee)(total_activated_stake)(
                                         thresh_activated_stake_time)
                                         (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close)(is_network_active)(active_touch_count)(last_account))
    };

    struct producer_info {
        account_name owner;
        uint32_t total_votes = 0;
        eosio::public_key producer_key; /// a packed public key object
        bool is_active = true;
        std::string url;
        uint64_t unpaid_fee = 0;
        uint64_t last_claim_time = 0;
        uint16_t location = 0;

        uint64_t primary_key() const { return owner; }

        double by_votes() const { return is_active ? -1.0 * total_votes : total_votes; }

        bool active() const { return is_active; }

        void deactivate() {
            producer_key = public_key();
            is_active = false;
        }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(producer_info, (owner)(total_votes)(producer_key)(is_active)(url)
                (unpaid_fee)(last_claim_time)(location))
    };

    struct voter_info {
        account_name owner = 0; /// the voter
        account_name proxy = 0; /// the proxy set by the voter, if any
        std::vector<account_name> producers; /// the producers approved by this voter if no proxy set
        int64_t staked = 0;

        /**
         *  Every time a vote is cast we must first "undo" the last vote weight, before casting the
         *  new vote weight.  Vote weight is calculated as:
         *
         *  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
         */
        double last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

        /**
         * Total vote weight delegated to this voter.
         */
        double proxied_vote_weight = 0; /// the total vote weight delegated to this voter as a proxy
        bool is_proxy = 0; /// whether the voter is a proxy for others


        uint32_t reserved1 = 0;
        time reserved2 = 0;
        eosio::asset reserved3;

        uint64_t primary_key() const { return owner; }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(voter_info,
                         (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(reserved1)(
                                 reserved2)(reserved3))
    };

    struct wood_burn_info { // wood burn detail
        uint64_t rowid = 0;
        account_name voter = 0; /// the voter
        uint32_t block_number = 0;
        std::string wood;

        uint64_t primary_key() const { return rowid; }

        static uint64_t woodkey(std::string wood) {
            if (wood.length() > 16) {
                return hextoint64(wood.substr(wood.length() - 16, 16));
            } else {
                return hextoint64(wood);
            }
        }

        static uint64_t hextoint64(std::string str) {

            uint64_t result = 0;
            const char *ch = str.c_str();
            for (int i = 0; (size_t) i < strlen(ch); i++) {
                if (ch[i] >= '0' && ch[i] <= '9') {
                    result = result * 16 + (uint64_t) (ch[i] - '0');
                } else if (ch[i] >= 'A' && ch[i] <= 'Z') {
                    result = result * 16 + (uint64_t) (ch[i] - 'A');
                } else if (ch[i] >= 'a' && ch[i] <= 'z') {
                    result = result * 16 + (uint64_t) (ch[i] - 'a');
                } else {
                    result = result * 16;
                }
            }

            return result;
        }

        uint64_t get_wood_index() const {
            return woodkey(wood);
        }

        uint64_t get_block_number() const { return (uint64_t) block_number; }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(wood_burn_info, (rowid)(voter)(block_number)(wood))
    };

    struct wood_burn_producer_block_stat //  wood burn per bp and block stat(木头焚烧按照BP及block_number统计的表)
    {
        uint64_t rowid = 0;
        account_name producer = 0; /// the producer
        uint32_t block_number = 0;
        uint32_t stat = 0;

        uint64_t primary_key() const { return rowid; }

        static uint128_t bpblockkey(account_name producer, uint32_t block_number) {
            return (uint128_t) producer << 32 | block_number;
        }

        uint128_t get_producer_block() const { return bpblockkey(producer, block_number); }

        uint64_t get_block_number() const { return (uint64_t) block_number; }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(wood_burn_producer_block_stat, (rowid)(producer)(block_number)(stat))
    };

    struct wood_burn_block_stat {
        uint32_t block_number = 0;
        uint32_t stat = 0;
        double diff = 0;

        uint32_t primary_key() const { return block_number; }

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(wood_burn_block_stat, (block_number)(stat)(diff))
    }; // 按照block_number统计的表，用于难度调整


    typedef eosio::multi_index<N(voters), voter_info> voters_table;

    typedef eosio::multi_index<N(woodburns), wood_burn_info, indexed_by<N(
            wood), const_mem_fun<wood_burn_info, uint64_t, &wood_burn_info::get_wood_index>>, indexed_by<N(
            block_number), const_mem_fun<wood_burn_info, uint64_t, &wood_burn_info::get_block_number>>> wood_burn_table;

    typedef eosio::multi_index<N(woodbpblocks), wood_burn_producer_block_stat, indexed_by<N(
            producerblock), const_mem_fun<wood_burn_producer_block_stat, uint128_t, &wood_burn_producer_block_stat::get_producer_block>>, indexed_by<N(
            block_number), const_mem_fun<wood_burn_producer_block_stat, uint64_t, &wood_burn_producer_block_stat::get_block_number>>> wood_burn_producer_block_table;

    typedef eosio::multi_index<N(woodblocks), wood_burn_block_stat> wood_burn_block_stat_table;

    typedef eosio::multi_index<N(producers), producer_info,
            indexed_by<N(prototalvote), const_mem_fun<producer_info, double, &producer_info::by_votes> >
    > producers_table;

    typedef eosio::singleton<N(global), eosio_global_state> global_state_singleton;

    //   static constexpr uint32_t     max_inflation_rate = 5;  // 5% annual inflation
    static constexpr uint32_t seconds_per_day = 24 * 3600;
    static constexpr uint64_t system_token_symbol = CORE_SYMBOL;

    class system_contract : public native {
    private:
        voters_table _voters;
        producers_table _producers;
        global_state_singleton _global;

        wood_burn_table _burninfos;
        wood_burn_producer_block_table _burnproducerstatinfos;
        wood_burn_block_stat_table _burnblockstatinfos;

        eosio_global_state _gstate;
        rammarket _rammarket;

    public:
        system_contract(account_name s);

        ~system_contract();

        // Actions:
        void onblock(block_timestamp timestamp, account_name producer);
        // const block_heade     r& header ); /// only parse first 3 fields of block header

        // functions defined in delegate_bandwidth.cpp

        /**
         *  Stakes SYS from the balance of 'from' for the benfit of 'receiver'.
         *  If transfer == true, then 'receiver' can unstake to their account
         *  Else 'from' can unstake at any time.
         */
        void delegatebw(account_name from, account_name receiver,
                        eosio::asset stake_net_quantity, eosio::asset stake_cpu_quantity, bool transfer);


        /**
         *  Decreases the total tokens delegated by from to receiver and/or
         *  frees the memory associated with the delegation if there is nothing
         *  left to delegate.
         *
         *  This will cause an immediate reduction in net/cpu bandwidth of the
         *  receiver.
         *
         *  A transaction is scheduled to send the tokens back to 'from' after
         *  the staking period has passed. If existing transaction is scheduled, it
         *  will be canceled and a new transaction issued that has the combined
         *  undelegated amount.
         *
         *  The 'from' account loses voting power as a result of this call and
         *  all producer tallies are updated.
         */
        void undelegatebw(account_name from, account_name receiver,
                          eosio::asset unstake_net_quantity, eosio::asset unstake_cpu_quantity);


        /**
         * Increases receiver's ram quota based upon current price and quantity of
         * tokens provided. An inline transfer from receiver to system contract of
         * tokens will be executed.
         */
        void buyram(account_name buyer, account_name receiver, eosio::asset tokens);

        void buyrambytes(account_name buyer, account_name receiver, uint32_t bytes);

        /**
         *  Reduces quota my bytes and then performs an inline transfer of tokens
         *  to receiver based upon the average purchase price of the original quota.
         */
        void sellram(account_name receiver, int64_t bytes);



        /**
         *  This action is called after the delegation-period to claim all pending
         *  unstaked tokens belonging to owner
         */
        void refund(account_name owner);

        // functions defined in voting.cpp

        void regproducer(const account_name producer, const public_key &producer_key, const std::string &url,
                         uint16_t location);

        void unregprod(const account_name producer);

        void setram(uint64_t max_ram_size);

        void setproxy(const account_name voter_name, const account_name proxy_name);

        void voteproducer(const account_name voter_name, const account_name wood_owner_name, std::string wood,
                          const uint32_t block_number, const account_name producer_name);

        void regproxy(const account_name proxy, bool isproxy);

        void setparams(const eosio::blockchain_parameters &params);

        // functions defined in producer_pay.cpp
        void claimrewards(const account_name &owner);

        void setpriv(account_name account, uint8_t ispriv);

        void rmvproducer(account_name producer);

        void bidname(account_name bidder, account_name newname, eosio::asset bid);

    private:
        void update_elected_producers(uint32_t head_block_number);

        bool verify(const std::string wood, const uint32_t block_number, const account_name wood_owner_name);

        uint32_t clean_dirty_stat_producers(uint32_t block_number, uint32_t maxline);

        void clean_diff_stat_history(uint32_t block_number);

        uint32_t clean_dirty_wood_history(uint32_t block_number, uint32_t maxline);

//        void onblock_clean_burn_stat(uint32_t block_number, uint32_t maxline);

        double calc_diff(uint32_t block_number);

        //defind in delegate_bandwidth.cpp
        void changebw(account_name from, account_name receiver,
                      eosio::asset stake_net_quantity, eosio::asset stake_cpu_quantity, bool transfer);

        //defined in voting.hpp
        static eosio_global_state get_default_parameters();

        void update_vote(const account_name voter_name, const account_name wood_owner_name,
                         const std::string wood, const uint32_t block_number, const account_name producer_name);

        void ramattenuator();
        void ramattenuator(account_name account);

    };

} /// celesossystem
