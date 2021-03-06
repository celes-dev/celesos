/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <vector>
#include <random>
#include <boost/optional.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/log/appender.hpp>
#include <fc/log/logger.hpp>
#include <eosio/chain/exceptions.hpp>
#include <celesos/pow/ethash.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <celesos/miner_plugin/miner.hpp>
#include <celesos/miner_plugin/miner_plugin.hpp>

using namespace std;
using namespace eosio;
using namespace celesos;

using boost::multiprecision::uint256_t;

static appbase::abstract_plugin &_miner_plugin = app().register_plugin<miner_plugin>();

namespace fc {
    extern std::unordered_map<std::string, logger> &get_logger_map();
}

class celesos::miner_plugin_impl {
public:
    using miner_type = celesos::miner::miner;
    using signature_provider_type = std::function<chain::signature_type(chain::digest_type)>;

    boost::optional<std::thread> _start_miner_thread_opt;
    boost::optional<miner_type> _miner_opt;
    account_name _voter_name;
    std::vector<account_name> _producer_names;
    std::size_t _next_producer_idx;
//    uint32_t _worker_priority;
    unsigned int _worker_count;
    float _sleep_probability;
    uint32_t _sleep_interval_sec;
    bool _has_plugin_shutdown;
    std::map<chain::public_key_type, signature_provider_type> _signature_providers;
    fc::microseconds _kcelesd_provider_timeout_us;
    fc::logger _logger;
    bool _auto_vote;

    miner_plugin_impl() : _next_producer_idx{0},
                          _worker_count{1},
                          _has_plugin_shutdown{false},
                          _auto_vote{true} {
    };

    ~miner_plugin_impl() {
        this->_has_plugin_shutdown = true;
        if (this->_start_miner_thread_opt && this->_start_miner_thread_opt->joinable()) {
            this->_start_miner_thread_opt->join();
        }
    }

    static chain::action create_action(const chain_plugin &the_plugin,
                                       vector<chain::permission_level> &&auth,
                                       chain::account_name &&account,
                                       chain::action_name &&name,
                                       fc::variant &&args) {
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

    static signature_provider_type make_kcelesd_signature_provider(const std::shared_ptr<miner_plugin_impl> &impl,
                                                                   const string &url_str,
                                                                   const public_key_type pubkey) {
        auto kcelesd_url = fc::url(url_str);
        std::weak_ptr<miner_plugin_impl> weak_impl = impl;

        return [weak_impl, kcelesd_url, pubkey](const chain::digest_type &digest) {
            auto impl = weak_impl.lock();
            if (impl) {
                fc::variant params;
                fc::to_variant(std::make_pair(digest, pubkey), params);
                auto deadline = impl->_kcelesd_provider_timeout_us.count() >= 0 ? fc::time_point::now() +
                                                                                  impl->_kcelesd_provider_timeout_us
                                                                                : fc::time_point::maximum();
                return app().get_plugin<eosio::http_client_plugin>().get_client().post_sync(kcelesd_url, params,
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
             boost::program_options::value<std::vector<string>>()->multitoken(),
             "Producer to vote when solve puzzle,program will "
             "perform vote action with answer after puzzle "
             "solved.")
            ("miner-sleep-interval-sec",
             boost::program_options::value<uint32_t>()->default_value(60),
             "Sleep interval when mining, unit is second, default is 60sec")
            ("miner-sleep-probability",
             boost::program_options::value<float>()->default_value(0.3f),
             "Probability to sleep when mining, value is in range [0,1], default is 0")
            ("miner-signature-provider",
             boost::program_options::value<vector<string>>()->composing()->multitoken(),
             "Key=Value pairs in the form <public-key>=<provider-spec>\n"
             "Where:\n"
             "   <public-key>    \tis a string form of a vaild CELESOS public key\n\n"
             "   <provider-spec> \tis a string in the form <provider-type>:<data>\n\n"
             "   <provider-type> \tis KEY, or KCELESD\n\n"
             "   KEY:<data>      \tis a string form of a valid CELESOS private key which maps to the provided public key\n\n"
             "   KCELESD:<data>    \tis the URL where kcelesd is available and the approptiate wallet(s) are unlocked")
            ("miner-kcelesd-provider-timeout", boost::program_options::value<int32_t>()->default_value(5),
             "Limits the maximum time (in milliseconds) that is allowd for sending blocks to a kcelesd provider for signing")
            ("miner-worker-count",
             boost::program_options::value<unsigned int>()->default_value(
                     std::max(std::thread::hardware_concurrency() / 2, 1U)),
             "Concurrency worker count, default is \"half_of_cpu_core_count\"");

#ifdef DEBUG
    cfg.add_options()
            ("miner-auto-vote", boost::program_options::value<bool>()->default_value(true),
             "Auto vote after compute a valid wood");
#endif
}

void celesos::miner_plugin::plugin_initialize(const variables_map &options) {
    try {
        ilog("plugin_initialize() begin");
        this->my = make_shared<miner_plugin_impl>();

        if (options.count("miner-auto-vote")) {
            this->my->_auto_vote = options["miner-auto-vote"].as<bool>();
        }

        if (options.count("miner-voter-name")) {
            this->my->_voter_name = account_name{options["miner-voter-name"].as<string>()};
        } else {
            elog("Option \"miner-voter-name\" must be provide");
        }

        if (options.count("miner-producer-name")) {
            auto &producer_names = options["miner-producer-name"].as<std::vector<string>>();
            for (auto &producer_name : producer_names) {
                this->my->_producer_names.emplace_back(producer_name);
            }
        } else if (this->my->_auto_vote) {
            elog("Option \"miner-producer-name\" must be provide");
        }

        if (options.count("miner-worker-count")) {
            auto worker_count = options["miner-worker-count"].as<unsigned int>();
            if (worker_count == 0) {
                worker_count = std::max(std::thread::hardware_concurrency() / 2, 1U);
            }
            this->my->_worker_count = worker_count;
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
                    } else if (spec_type_str == "KCELESD") {
                        auto &&make_provider = celesos::miner_plugin_impl::make_kcelesd_signature_provider;
                        this->my->_signature_providers[pubkey] = make_provider(
                                this->my, spec_data,
                                pubkey);
                    }
                } catch (...) {
                    elog("Malformed signature provider: \"${val}\", ignoring!", ("val", key_spec_pair));
                }
            }
        }

        auto &&sleep_interval_sec = options["miner-sleep-interval-sec"].as<uint32_t>();
        this->my->_sleep_interval_sec = sleep_interval_sec;
        auto sleep_probability = options["miner-sleep-probability"].as<float>();
        if (sleep_probability < 0.0f || sleep_probability > 1.0f) {
            elog("Option \"miner-sleep-probability\" must be in range [0,1]");
        }
        this->my->_sleep_probability = sleep_probability;

//        auto worker_priority = options["miner-worker-priority"].as<uint32_t>();
//        if (worker_priority < 1 || worker_priority > 99) {
//            FC_THROW("worker priority must between [1,99]");
//        }
//        this->my->_worker_priority = worker_priority;

        this->my->_kcelesd_provider_timeout_us = fc::milliseconds(
                options["miner-kcelesd-provider-timeout"].as<int32_t>());
        ilog("plugin_initialize() end");
    } FC_LOG_AND_RETHROW()
}

void celesos::miner_plugin::start_miner() {
    auto &logger = this->my->_logger;
    auto &the_chain_plugin = app().get_plugin<chain_plugin>();

    this->my->_miner_opt->start(this->my->_voter_name, the_chain_plugin.chain());
    this->my->_miner_opt->connect(
            [this, &the_chain_plugin, &logger](auto is_success,
                                               auto block_num,
                                               const auto &wood_opt) {
                fc_dlog(logger, "Receive mine callback with is_success: ${1} block_num: ${2}",
                        ("1", is_success)("2", block_num));

                //TODO 考虑系统合约未安装的情况
                if (!is_success) {
                    //TODO 完善算不出hash的流程
                    return;
                }

                auto producer_name_count = this->my->_producer_names.size();
                try {
                    if (!this->my->_auto_vote || producer_name_count == 0) {
                        std::string wood_hex{};
                        ethash::uint256_to_hex(wood_hex, wood_opt.get());
                        const auto &owner_name = this->my->_voter_name;
                        fc_ilog(logger,
                                "\n\tcollect wood with "
                                "\n\t\tblock_num: ${block_num} "
                                "\n\t\tvoter: ${owner} "
                                "\n\t\twood: ${wood} ",
                                ("block_num", block_num)
                                        ("owner", owner_name)
                                        ("wood", wood_hex.c_str()));
                    } else {
                        fc_dlog(logger, "begin prepare transaction about voteproducer");

                        auto &cc = the_chain_plugin.chain();
                        const auto &chain_id = cc.get_chain_id();
                        const auto &voter_name = this->my->_voter_name;
                        const auto &producer_name = this->my->_producer_names[this->my->_next_producer_idx];
                        this->my->_next_producer_idx =
                                (this->my->_next_producer_idx + 1) % producer_name_count;
                        std::string wood_hex{};
                        ethash::uint256_to_hex(wood_hex, wood_opt.get());

                        chain::signed_transaction tx{};
                        vector<chain::permission_level> auth{{voter_name, "active"}};
                        const auto &code = chain::config::system_account_name;
                        chain::action_name action{"voteproducer"};
                        auto args = fc::mutable_variant_object{}
                                ("voter_name", voter_name)
                                ("wood_owner_name", voter_name)
                                ("wood", wood_hex)
                                ("block_number", block_num)
                                ("producer_name", producer_name);
                        auto a_action = miner_plugin_impl::create_action(the_chain_plugin,
                                                                         std::move(auth), code,
                                                                         std::move(action), std::move(args));
                        tx.actions.push_back(std::move(a_action));
                        tx.expiration = cc.head_block_time() + fc::seconds(30);
                        tx.set_reference_block(cc.last_irreversible_block_id());
                        tx.max_cpu_usage_ms = 0;
                        tx.max_net_usage_words = 0UL;
                        tx.delay_sec = 0UL;
                        for (auto &&pair : this->my->_signature_providers) {
                            auto &&digest = tx.sig_digest(chain_id, tx.context_free_data);
                            auto &&signature = pair.second(digest);
                            tx.signatures.push_back(signature);
                        }
                        auto packed_tx_ptr = std::make_shared<chain::packed_transaction>(
                                chain::packed_transaction{tx});
                        auto tx_metadata_ptr = std::make_shared<chain::transaction_metadata>(packed_tx_ptr);
                        fc_dlog(logger, "end prepare transaction about voteproducer");
                        using method_type = chain::plugin_interface::incoming::methods::transaction_async;
                        using handler_param_type = fc::static_variant<fc::exception_ptr, chain::transaction_trace_ptr>;
                        fc_ilog(logger, "start to push transation about voteproducer with voter: ${1} producer: ${2}",
                                ("1", voter_name)("2", producer_name));
                        auto handler = [&logger, voter_name, producer_name](const handler_param_type &param) {
                            if (param.contains<fc::exception_ptr>()) {
                                auto exp_ptr = param.get<fc::exception_ptr>();
                                fc_wlog(logger,
                                        "fail to push transaction about voteproducer with error: \n${error}",
                                        ("error", exp_ptr->to_detail_string().c_str()));
                            } else {
                                auto trace_ptr = param.get<chain::transaction_trace_ptr>();
                                fc_ilog(logger,
                                        "success to push transaction about voteproducer with voter: ${1} producer: ${2}",
                                        ("1", voter_name)("2", producer_name));
                            }
                        };
                        app().get_method<method_type>()(tx_metadata_ptr, true, handler);
                    }
                } catch (fc::exception &er) {
                    wlog("${details}", ("details", er.to_detail_string()));
                } catch (const std::exception &e) {
                    fc::exception fce{FC_LOG_MESSAGE(warn, "rethrow ${what}: ", ("what", e.what())),
                                      fc::std_exception_code, BOOST_CORE_TYPEID(e).name(), e.what()};
                    wlog("${details}", ("details", fce.to_detail_string()));
                } catch (...) {
                    fc::unhandled_exception e{FC_LOG_MESSAGE(warn, "rethrow"), std::current_exception()};
                    wlog("${details}", ("details", e.to_detail_string()));
                }
            });
}

void celesos::miner_plugin::plugin_startup() {
    try {
        auto &logger_map = fc::get_logger_map();
        auto &logger = this->my->_logger;
        if (logger_map.find("miner_plugin") != logger_map.end()) {
            logger = fc::logger::get("miner_plugin");
        } else {
            for (const auto &appender : fc::logger::get().get_appenders()) {
                logger.add_appender(appender);
            }
#ifdef DEBUG
            logger.set_log_level(fc::log_level::debug);
#else
            logger.set_log_level(fc::log_level::info);
#endif
        }

        ilog("plugin_startup() begin");
        this->my->_miner_opt.emplace(logger,
                                     app().get_io_service(),
                                     this->my->_worker_count,
                                     this->my->_sleep_interval_sec,
                                     this->my->_sleep_probability);

        this->my->_start_miner_thread_opt.emplace([this, &logger]() {
            fc_dlog(logger, "begin start_miner_thread::run()");
            auto &the_chain_plugin = app().get_plugin<chain_plugin>();

            for (;;) {
                if (this->my->_has_plugin_shutdown) {
                    fc_ilog(logger, "miner_plugin has been shutdown, quit");
                    return;
                }

                if (fc::time_point::now() - the_chain_plugin.chain().head_block_time() >= fc::minutes(1)) {
                    fc_ilog(logger, "chain is syncing block, wait 10 sec");
                    auto sleep_until_time = fc::time_point::now() + fc::seconds(10);
                    while (fc::time_point::now() < sleep_until_time) {
                        std::this_thread::sleep_for(std::chrono::seconds{1});
                        if (this->my->_has_plugin_shutdown) {
                            fc_ilog(logger, "miner_plugin has been shutdown, quit");
                            return;
                        }
                    }
                } else {
                    break;
                }
            }

            auto &main_io_service = app().get_io_service();
            auto handler = std::bind(&miner_plugin::start_miner, this);
            main_io_service.post(handler);
            fc_dlog(logger, "end start_miner_thread::run()");
        });

        ilog("plugin_startup() end");
    }
    FC_LOG_AND_RETHROW()
}

void celesos::miner_plugin::plugin_shutdown() {
    try {
        ilog("plugin_shutdown() begin");
        this->my->_has_plugin_shutdown = true;
        this->my->_miner_opt->stop(false);
        this->my->_miner_opt.reset();
        if (this->my->_start_miner_thread_opt && this->my->_start_miner_thread_opt->joinable()) {
            this->my->_start_miner_thread_opt->join();
        }
        this->my->_start_miner_thread_opt.reset();
        ilog("plugin_shutdown() end");
    } FC_LOG_AND_RETHROW()
}
