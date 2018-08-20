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

            std::vector<std::shared_ptr<celesos::miner::worker>> _alive_workers;
            std::vector<boost::signals2::connection> _connections;
            celesos::miner::mine_signal_ptr_type _signal;
            std::shared_ptr<boost::asio::io_service::work> _io_work;
            std::shared_ptr<boost::asio::io_service> _io_service;
            std::thread _io_thread;
            state _state;

            static void string_to_uint256_little(boost::multiprecision::uint256_t &dst, const std::string &str);

            static void gen_random_uint256(boost::multiprecision::uint256_t &dst);

            void on_forest_updated(eosio::chain::controller &cc);

            void run();

        public:

            miner();

            ~miner();

            void start(eosio::chain::controller &controller);

            void stop(bool wait = true);

            boost::signals2::connection connect(const celesos::miner::mine_slot_type &slot);
        };
    }
}

#endif //EOSIO_MINER_H
