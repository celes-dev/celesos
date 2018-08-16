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

    miner_plugin::miner_plugin() {

    }

    miner_plugin::~miner_plugin() {
    }

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
        this->my->the_miner.connect([&cc](const uint256_t &wood) {
            //TODO 修改相关账户信息的获取方式
            const chain::name &voter_account{"yale"};
            const auto &voter_pk = chain::private_key_type::generate();
            const chain::name &producer_account{"eospacific"};

            //TODO 处理算出nonce的流程
            const auto &chain_id = cc.get_chain_id();
            auto tx = chain::signed_transaction{};
            auto permission_levels = vector<chain::permission_level>{
                    {voter_account, "active"}};

            auto a_action_vote = miner::action_vote{
                    .producer = producer_account,
                    .voter = voter_account,
                    .wood = wood,
            };
            tx.actions.emplace_back(permission_levels, a_action_vote);
            tx.expiration = cc.head_block_time() + fc::seconds(30);
            tx.set_reference_block(chain_id);
            tx.sign(voter_pk, chain_id);
        });
        this->my->the_miner.start(cc);
    }

    void miner_plugin::plugin_shutdown() {
        this->my->the_miner.stop(false);
    }
}
