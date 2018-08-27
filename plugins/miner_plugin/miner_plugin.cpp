/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <vector>
#include <random>
#include <fc/reflect/reflect.hpp>
#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>
#include <eosio/chain/exceptions.hpp>
#include <celesos/pow/ethash.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <celesos/miner_plugin/miner.hpp>
#include <celesos/miner_plugin/miner_plugin.hpp>

using namespace std;
using namespace eosio;
using namespace celesos;

using boost::multiprecision::uint256_t;

static appbase::abstract_plugin &_miner_plugin = app().register_plugin<miner_plugin>();

class celesos::miner_plugin_impl {
public:
    using miner_type = celesos::miner::miner;
    using signature_provider_type = std::function<chain::signature_type(chain::digest_type)>;

    miner_type _the_miner;
    account_name _voter_name;
    account_name _producer_name;
    std::map<chain::public_key_type, signature_provider_type> _signature_providers;
    fc::microseconds _keosd_provider_timeout_us;

    miner_plugin_impl() {
    };

    static chain::action create_action(const chain_plugin &the_plugin,
                                       vector<chain::permission_level> &&auth,
                                       chain::account_name &&account,
                                       chain::action_name &&name,
                                       fc::mutable_variant_object &&args) {
        chain_apis::read_only::abi_json_to_bin_params params{
                .code = account,
                .action = name,
                .args = args,
        };
        auto &&ret = the_plugin.get_read_only_api().abi_json_to_bin(params);
        return chain::action{auth, account, name, ret.binargs};
    }

    static signature_provider_type make_key_signature_provider(const chain::private_key_type &key) {
        return [key](const chain::digest_type &digest) {
            return key.sign(digest);
        };
    }

    static signature_provider_type make_keosd_signature_provider(const std::shared_ptr<miner_plugin_impl> &impl,
                                                                 const string &url_str,
                                                                 const public_key_type pubkey) {
        auto keosd_url = fc::url(url_str);
        std::weak_ptr<miner_plugin_impl> weak_impl = impl;

        return [weak_impl, keosd_url, pubkey](const chain::digest_type &digest) {
            auto impl = weak_impl.lock();
            if (impl) {
                fc::variant params;
                fc::to_variant(std::make_pair(digest, pubkey), params);
                auto deadline = impl->_keosd_provider_timeout_us.count() >= 0 ? fc::time_point::now() +
                                                                                impl->_keosd_provider_timeout_us
                                                                              : fc::time_point::maximum();
                return app().get_plugin<eosio::http_client_plugin>().get_client().post_sync(keosd_url, params,
                                                                                            deadline).as<chain::signature_type>();
            } else {
                return chain::signature_type();
            }
        };
    }
};

celesos::miner_plugin::miner_plugin() {
}

celesos::miner_plugin::~miner_plugin() {
}

void celesos::miner_plugin::set_program_options(options_description &, options_description &cfg) {
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
             "   KEOSD:<data>    \tis the URL where keosd is available and the approptiate wallet(s) are unlocked")
            ("miner-keosd-provider-timeout", boost::program_options::value<int32_t>()->default_value(5),
             "Limits the maximum time (in milliseconds) that is allowd for sending blocks to a keosd provider for signing");
}

void celesos::miner_plugin::plugin_initialize(const variables_map &options) {
    try {
        ilog("plugin_initialize() begin");
        this->my = make_shared<miner_plugin_impl>();
        if (options.count("miner-voter-name")) {
            this->my->_voter_name = account_name{options["miner-voter-name"].as<string>()};
        } else {
            elog("Option \"miner-voter-name\" must be provide");
        }

        if (options.count("miner-producer-name")) {
            this->my->_producer_name = account_name{options["miner-producer-name"].as<string>()};
        } else {
            elog("Option \"miner-producer-name\" must be provide");
        }

        if (options.count("miner-signature-provider")) {
            auto &&key_spec_pairs = options["miner-signature-provider"].as<std::vector<std::string>>();
            for (const auto &key_spec_pair : key_spec_pairs) {
                try {
                    auto delim = key_spec_pair.find('=');
                    EOS_ASSERT(delim != std::string::npos,
                               chain::plugin_config_exception,
                               "Missing \"=\" in the key spec pair");
                    auto pub_key_str = key_spec_pair.substr(0, delim);
                    auto spec_str = key_spec_pair.substr(delim + 1);

                    auto spec_delim = spec_str.find(":");
                    EOS_ASSERT(spec_delim != std::string::npos,
                               chain::plugin_config_exception,
                               "Missing \":\" in the key spec pair");
                    auto spec_type_str = spec_str.substr(0, spec_delim);
                    auto spec_data = spec_str.substr(spec_delim + 1);

                    auto pubkey = public_key_type(pub_key_str);

                    if (spec_type_str == "KEY") {
                        auto &&make_provider = celesos::miner_plugin_impl::make_key_signature_provider;
                        this->my->_signature_providers[pubkey] = make_provider(
                                chain::private_key_type(spec_data));
                    } else if (spec_type_str == "KEOSD") {
                        auto &&make_provider = celesos::miner_plugin_impl::make_keosd_signature_provider;
                        this->my->_signature_providers[pubkey] = make_provider(
                                this->my, spec_data,
                                pubkey);
                    }
                } catch (...) {
                    elog("Malformed signature provider: \"${val}\", ignoring!", ("val", key_spec_pair));
                }
            }
        }

        this->my->_keosd_provider_timeout_us = fc::milliseconds(
                options["miner-keosd-provider-timeout"].as<int32_t>());
        ilog("plugin_initialize() end");
    } FC_LOG_AND_RETHROW()
}

void celesos::miner_plugin::plugin_startup() {
    try {
        ilog("plugin_startup() begin");

        auto &the_chain_plugin = app().get_plugin<chain_plugin>();
        this->my->_the_miner.start(this->my->_voter_name, the_chain_plugin.chain());
        this->my->_the_miner.connect(
                [this, &the_chain_plugin](auto is_success,
                                          auto block_num,
                                          const auto &wood_opt) {
                    //TODO 考虑系统合约未安装的情况
                    if (!is_success) {
                        //TODO 完善算不出hash的流程
                        return;
                    }
                    try {
                        auto &cc = the_chain_plugin.chain();
                        const auto &chain_id = cc.get_chain_id();
                        const auto &voter_name = this->my->_voter_name;
                        const auto &producer_name = this->my->_producer_name;

                        chain::signed_transaction tx{};
                        vector<chain::permission_level> auth{{voter_name, "active"}};
                        const auto &code = chain::config::system_account_name;
                        chain::action_name action{"voteproducer"};
                        auto args = fc::mutable_variant_object{}
                                ("voter_name", voter_name)
                                ("wood_owner_name", voter_name)
                                ("wood_info", fc::mutable_variant_object{}
                                        ("block_number", block_num)
                                        ("wood", wood_opt.get()))
                                ("producer_name", producer_name);
                        auto a_action = miner_plugin_impl::create_action(the_chain_plugin,
                                                                         std::move(auth), code,
                                                                         std::move(action), std::move(args));
                        tx.actions.push_back(std::move(a_action));
                        tx.expiration = cc.head_block_time() + fc::seconds(30);
                        tx.set_reference_block(chain_id);
                        for (auto &&pair : this->my->_signature_providers) {
                            auto &&digest = tx.sig_digest(chain_id, tx.context_free_data);
                            auto &&signature = pair.second(digest);
                            tx.signatures.push_back(signature);
                        }
                    } FC_LOG_AND_RETHROW()
                });
        ilog("plugin_startup() end");
    } FC_LOG_AND_RETHROW()
}

void celesos::miner_plugin::plugin_shutdown() {
    try {
        dlog("plugin_shutdown() begin");
        this->my->_the_miner.stop(false);
        dlog("plugin_shutdown() end");
    } FC_LOG_AND_RETHROW()
}
