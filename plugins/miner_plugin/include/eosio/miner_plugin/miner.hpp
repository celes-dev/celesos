//
// Created by yale on 8/14/18.
//

#ifndef EOSIO_MINER_H
#define EOSIO_MINER_H

#include <vector>
#include <boost/signals2.hpp>
#include <boost/asio/io_service.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/miner_plugin/worker.h>

namespace celesos {
    namespace miner {
        //TODO 把投票action对应结构体移到正确的位置
        struct action_vote {
            eosio::chain::account_name voter;
            eosio::chain::account_name proxy;
            eosio::chain::uint256_t wood;
            eosio::chain::account_name producer;

            static eosio::chain::account_name get_account() {
                return eosio::chain::config::system_account_name;
            }

            static eosio::chain::action_name get_name() {
                return N(burn_wood);
            }
        };

        class miner {
            using slot_type = void(const boost::multiprecision::uint256_t &);

        private:
            std::vector<std::shared_ptr<celesos::miner::worker>> _alive_workers;
            std::vector<boost::signals2::connection> _connections;
            std::shared_ptr<boost::signals2::signal<slot_type>> _signal;
            std::shared_ptr<boost::asio::io_service::work> _io_work;
            std::shared_ptr<boost::asio::io_service> _io_service;
            std::thread _io_thread;

            static void string_to_uint256_little(boost::multiprecision::uint256_t &dst, const std::string &str);

            static void gen_random_uint256(boost::multiprecision::uint256_t &dst);

            void run();

        public:

            miner();

            ~miner();

            void start(eosio::chain::controller &controller);

            void stop(bool wait = true);

            boost::signals2::connection connect(const std::function<slot_type> &slot);
        };
    }
}

FC_REFLECT(celesos::miner::action_vote, (voter)(proxy)(wood)(producer))

#endif //EOSIO_MINER_H
