//
// Created by yale on 8/14/18.
//

#ifndef CELESOS_MINER_PLUGIN_WORKER_H
#define CELESOS_MINER_PLUGIN_WORKER_H

#include <thread>
#include <shared_mutex>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <celesos/pow/ethash.h>
#include <celesos/miner_plugin/types.hpp>

namespace celesos {
    namespace miner {
        struct worker_ctx {
            using dataset_ptr_type = std::shared_ptr<std::vector<celesos::ethash::node>>;
            using hash_ptr_type = std::shared_ptr<boost::multiprecision::uint256_t>;

            const dataset_ptr_type dataset;
            const std::shared_ptr<std::string> forest;
            const hash_ptr_type nonce_start;
            const hash_ptr_type retry_count;
            const hash_ptr_type target;
            const eosio::chain::block_num_type block_num;
            const std::shared_ptr<boost::asio::io_service> io_service;
            const celesos::miner::mine_signal_ptr_type signal;
        };

        class worker {
        private:
            enum state {
                initialized,
                started,
                stopped,
            };

            worker_ctx _ctx;
            boost::multiprecision::uint256_t _state;
            std::shared_timed_mutex _mutex;
            boost::optional<std::thread> _alive_thread;

            void run();

        protected:
            worker() = default;

        public:

            worker(worker_ctx ctx);

            worker(const worker &) = delete;

            ~worker();

            worker &operator=(const worker &) = delete;

            void start();

            void stop(bool wait = true);
        };
    }
}

#endif //EOSIO_WORKER_H
