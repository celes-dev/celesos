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
#include <celesos/pow/ethash.hpp>
#include <celesos/miner_plugin/types.hpp>

namespace celesos {
    namespace miner {
        struct worker_ctx {
            using dataset_ptr_type = std::shared_ptr<std::vector<celesos::ethash::node>>;
            using hash_ptr_type = std::shared_ptr<boost::multiprecision::uint256_t>;

            const dataset_ptr_type dataset_ptr;
            const std::shared_ptr<std::string> seed_ptr;
            const std::shared_ptr<std::string> forest_ptr;
            const hash_ptr_type nonce_start_ptr;
            const hash_ptr_type retry_count_ptr;
            const hash_ptr_type target_ptr;
            const eosio::chain::block_num_type block_num;
            const std::shared_ptr<boost::asio::io_service> io_service_ptr;
            const celesos::miner::mine_signal_ptr_type signal_ptr;
        };

        class worker {
        private:
            using mutex_type = std::shared_timed_mutex;
            using write_lock_type = std::unique_lock<mutex_type>;
            using read_lock_type = std::shared_lock<mutex_type>;

            enum state {
                initialized,
                started,
                stopped,
            };

            worker_ctx _ctx;
            state _state;
            mutex_type _mutex;
            boost::optional<std::thread> _alive_thread_opt;

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

#endif //CELESOS_MINER_PLUGIN_WORKER_H
