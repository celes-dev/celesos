//
// Created by yale on 8/14/18.
//

#ifndef CELESOS_MINER_PLUGIN_MINER_H
#define CELESOS_MINER_PLUGIN_MINER_H

#include <vector>
#include <boost/signals2.hpp>
#include <boost/asio/io_service.hpp>
#include <eosio/chain/controller.hpp>
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

            std::vector<std::shared_ptr<celesos::miner::worker>> _alive_worker_ptrs;
            std::vector<boost::signals2::connection> _connections;
            celesos::miner::mine_signal_ptr_type _signal_ptr;
            std::shared_ptr<boost::asio::io_service::work> _io_work_ptr;
            std::shared_ptr<boost::asio::io_service> _io_service_ptr;
            std::thread _io_thread;
            state _state;
            boost::optional<eosio::chain::block_num_type> _next_block_num_opt;
            boost::optional<fc::microseconds> _last_failure_time_us;
            fc::microseconds _failure_retry_interval_us;


            void on_forest_updated(const eosio::chain::account_name &relative_account, eosio::chain::controller &cc);

            void run();

        public:

            static void string_to_uint256_little(boost::multiprecision::uint256_t &dst, const std::string &str);

            static void gen_random_uint256(boost::multiprecision::uint256_t &dst);

            miner();

            ~miner();

            void start(const eosio::chain::account_name &relative_account, eosio::chain::controller &controller);

            void stop(bool wait = true);

            boost::signals2::connection connect(const celesos::miner::mine_slot_type &slot);
        };
    }
}

#endif //CELESOS_MINER_PLUGIN_MINER_H
