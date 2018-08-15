/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <vector>
#include <random>
#include <fc/reflect/reflect.hpp>
#include <fc/exception/exception.hpp>
#include <eosio/miner_plugin/miner_plugin.hpp>
#include <eosio/miner_plugin/miner.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <celesos/pow/ethash.h>

using namespace std;
using namespace eosio;
using namespace celesos;

using boost::multiprecision::uint256_t;

namespace eosio {
    static appbase::abstract_plugin &_miner_plugin = app().register_plugin<miner_plugin>();


    class miner_plugin_impl {
    public:
        miner::miner the_miner;

        miner_plugin_impl() {
        };
    };

    void miner_plugin::set_program_options(options_description &, options_description &cfg) {
        cfg.add_options()
                ("option-name", bpo::value<string>()->default_value("default value"),
                 "Option Description");
    }

    void miner_plugin::plugin_initialize(const variables_map &options) {
        try {
            if (options.count("option-name")) {
                // Handle the option
            }
        }
        FC_LOG_AND_RETHROW()
    }

    void miner_plugin::plugin_startup() {
        auto &cc = app().get_plugin<chain_plugin>().chain();
        this->my->the_miner.start(cc);
    }

    void miner_plugin::plugin_shutdown() {
        this->my->the_miner.stop(false);
    }
}
