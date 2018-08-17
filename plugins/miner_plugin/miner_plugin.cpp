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
                ("miner-voter-name",
                 boost::program_options::value<string>(),
                 "Account name for fetch forest")
                ("miner-producer-name",
                 boost::program_options::value<string>(),
                 "Producer to vote when solve puzzle,program will "
                 "perform vote action with answer after puzzle "
                 "solved.")
                ("miner-signature-provider",
                 boost::program_options::value<vector<string>>()->composing()->multitoken(),
                 "Key=Value pairs in the form <public-key>=<provider-spec>\n"
                 "Where:\n"
                 "   <public-key>    \tis a string form of a vaild EOSIO public key\n\n"
                 "   <provider-spec> \tis a string in the form <provider-type>:<data>\n\n"
                 "   <provider-type> \tis KEY, or KEOSD\n\n"
                 "   KEY:<data>      \tis a string form of a valid EOSIO private key which maps to the provided public key\n\n"
                 "   KEOSD:<data>    \tis the URL where keosd is available and the approptiate wallet(s) are unlocked");
    }

    void miner_plugin::plugin_initialize(const variables_map &options) {
        try {
            ilog("plugin_initialize() begin");
            if (options.count("miner-voter-name")) {
                const auto &voter_name = options["miner-voter-name"].as<string>();
            }
            if (options.count("miner-producer-name")) {
                const auto &producer_name = options["miner-producer-name"].as<string>();
            }
            if (options.count("miner-signature-provider")) {
                const auto &signature_providers = options["miner-signature-provider"].as<vector<string>>();
            }
            ilog("plugin_initialize() end");
        } FC_LOG_AND_RETHROW()
    }

    void miner_plugin::plugin_startup() {
        try {
            ilog("plugin_startup() begin");
            this->my = make_unique<miner_plugin_impl>();
            auto &the_chain_plugin = app().get_plugin<chain_plugin>();
            auto &cc = the_chain_plugin.chain();
            this->my->the_miner.start(cc);
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
                ilog("plugin_startup() end");
            });
        } FC_LOG_AND_RETHROW()
    }

    void miner_plugin::plugin_shutdown() {
        try {
            dlog("plugin_shutdown() begin");
            this->my->the_miner.stop(false);
            dlog("plugin_shutdown() end");
        } FC_LOG_AND_RETHROW()
    }
}
