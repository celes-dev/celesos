//
// Created by yale on 8/14/18.
//

#ifndef CELESOS_MINER_PLUGIN_MINER_H
#define CELESOS_MINER_PLUGIN_MINER_H

#include <vector>
#include <boost/signals2.hpp>
#include <boost/asio/io_service.hpp>
#include <fc/log/logger.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <celesos/miner_plugin/types.hpp>
#include <celesos/miner_plugin/worker.hpp>

namespace celesos {
    namespace miner {
        class miner {
        private:
            enum state {
                initialized,
                started,
                stopped,
            };

            fc::logger _logger;
            std::vector<std::shared_ptr<celesos::miner::worker>> _alive_worker_ptrs;
            std::vector<boost::signals2::connection> _connections;
            celesos::miner::mine_signal_ptr_type _signal_ptr;
            std::shared_ptr<boost::asio::io_service::work> _io_work_ptr;
            boost::asio::io_service &_main_io_service_ref;
            std::shared_ptr<boost::asio::io_service> _sub_io_service_ptr;
            std::thread _io_thread;
            state _state;
            boost::optional<std::shared_ptr<std::vector<celesos::ethash::node>>> _target_cache_ptr_opt;
            boost::optional<std::shared_ptr<std::vector<celesos::ethash::node>>> _target_dataset_ptr_opt;
            boost::optional<celesos::forest::forest_struct> _target_forest_info_opt;
            boost::optional<uint32_t> _target_cache_count_opt;
            boost::optional<uint32_t> _target_dataset_count_opt;
            boost::optional<fc::microseconds> _last_failure_time_us;
            unsigned int _worker_count;
            fc::microseconds _failure_retry_interval_us;

            void on_forest_updated(const std::shared_ptr<celesos::forest::forest_struct> forest_info_ptr,
                                   const eosio::chain::account_name &relative_account);

            void run();

            void stop_workers(bool wait);

        public:

            static void string_to_uint256_little(boost::multiprecision::uint256_t &dst, const std::string &str);

            static void gen_random_uint256(boost::multiprecision::uint256_t &dst);

            miner(const fc::logger &logger, boost::asio::io_service &main_io_service, unsigned int worker_count = 1);

            miner() = delete;

            ~miner();

            void start(const eosio::chain::account_name &relative_account, eosio::chain::controller &controller);

            void stop(bool wait = true);

            boost::signals2::connection connect(const celesos::miner::mine_slot_type &slot);
        };
    }
}

#endif //CELESOS_MINER_PLUGIN_MINER_H
