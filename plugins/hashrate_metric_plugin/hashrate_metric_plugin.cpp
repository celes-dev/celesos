/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <numeric>
#include <celesos/hashrate_metric_plugin/hashrate_metric_plugin.hpp>
#include <fc/exception/exception.hpp>
#include <celesos/pow/ethash.hpp>
#include <fstream>

using namespace appbase;
using namespace celesos;

using boost::multiprecision::uint256_t;

static appbase::abstract_plugin &_hashrate_metric_plugin = app().register_plugin<hashrate_metric_plugin>();

namespace celesos {
    class hashrate_metric_plugin_impl {
    public:
        std::string _seed;
        std::string _forest;
        std::string _separator_symbol;
        uint32_t _cache_count;
        uint32_t _dataset_count;
        uint32_t _thread_priority;
        uint32_t _concurrency_count;
        std::string _collect_what;
        std::string _collect_unit;
        uint64_t _difficulty;
        boost::optional<std::string> _output_path_opt;

        hashrate_metric_plugin_impl() : _seed{},
                                        _forest{},
                                        _separator_symbol{},
                                        _cache_count{0},
                                        _dataset_count{0},
                                        _thread_priority{0},
                                        _concurrency_count{0},
                                        _collect_what{},
                                        _collect_unit{},
                                        _difficulty{0},
                                        _output_path_opt{} {
        }
    };
}

celesos::hashrate_metric_plugin::hashrate_metric_plugin() : my(new hashrate_metric_plugin_impl()) {}

celesos::hashrate_metric_plugin::~hashrate_metric_plugin() {}

void celesos::hashrate_metric_plugin::set_program_options(options_description &, options_description &cfg) {
    cfg.add_options()
            ("hashrate-seed", boost::program_options::value<string>(), "Seed to generate cache")
            ("hashrate-forest", boost::program_options::value<string>(), "Forest to about calc hash")
            ("hashrate-cache-count", boost::program_options::value<uint32_t>()->default_value(512),
             "Count of cache's element, default is 512")
            ("hashrate-dataset-count", boost::program_options::value<uint32_t>()->default_value(1024),
             "Count of dataset's element, default is 1024")
            ("hashrate-thread-priority", boost::program_options::value<uint32_t>()->default_value(1),
             "Thread priority, default is 1")
            ("hashrate-concurrency-count", boost::program_options::value<uint32_t>()->default_value(1),
             "Concurrency count for calc hash, default is 1")
            ("hashrate-separator-symbol", boost::program_options::value<std::string>()->default_value(";"),
             "Separator symbol for output data, default is ';'")
            ("hashrate-collect-what",
             boost::program_options::value<std::string>()->default_value("hash-full-count"),
             R"(Define collect what, value is one of ["hash-full-count", "hash-light-count", "valid-hash-full-count", "valid-hash-light-count"])")
            ("hashrate-collect-unit",
             boost::program_options::value<std::string>()->default_value("second"),
             R"(Define collect unit, value is one of ["second", "minute"])")
            ("hashrate-difficulty", boost::program_options::value<std::uint64_t>()->default_value(0),
             R"(Difficulty relative to collect what "valid-hash-full-count")")
            ("hashrate-output-path", boost::program_options::value<std::string>(), "Output path for csv data");
}

void celesos::hashrate_metric_plugin::plugin_initialize(const variables_map &options) {
    try {
        this->my->_seed = options["hashrate-seed"].as<std::string>();
        this->my->_forest = options["hashrate-forest"].as<std::string>();
        this->my->_cache_count = options["hashrate-cache-count"].as<uint32_t>();
        this->my->_dataset_count = options["hashrate-dataset-count"].as<uint32_t>();

        auto thread_priority = options["hashrate-thread-priority"].as<uint32_t>();
        if (thread_priority < 1 || thread_priority > 99) {
            FC_THROW("thread priority must between [1,99]");
        }
        this->my->_thread_priority = thread_priority;

        this->my->_concurrency_count = options["hashrate-concurrency-count"].as<uint32_t>();

        this->my->_separator_symbol = options["hashrate-separator-symbol"].as<std::string>();

        this->my->_collect_what = options["hashrate-collect-what"].as<std::string>();
        std::vector<std::string> collect_whats{
                "hash-full-count",
                "hash-light-count",
                "valid-hash-full-count",
                "valid-hash-light-count",
        };
        if (std::find(collect_whats.cbegin(), collect_whats.cend(), this->my->_collect_what) == collect_whats.cend()) {
            auto &&values = std::accumulate(collect_whats.cbegin(),
                                            collect_whats.cend(),
                                            std::string{},
                                            [](auto &lhs, const auto &rhs) {
                                                if (!lhs.empty()) {
                                                    lhs = lhs.append(",");
                                                }
                                                return lhs.append(",\"").append(rhs).append("\"");
                                            });
            FC_THROW("hashrate-collect-what must be one of [ ${values} ]", ("values", values));
        }

        this->my->_collect_unit = options["hashrate-collect-unit"].as<std::string>();
        std::vector<std::string> collect_units{
                "second",
                "minute",
        };
        if (std::find(collect_units.cbegin(), collect_units.cend(), this->my->_collect_unit) == collect_units.cend()) {
            auto &&values = std::accumulate(collect_units.cbegin(),
                                            collect_units.cend(),
                                            std::string{},
                                            [](auto &lhs, const auto &rhs) {
                                                if (!lhs.empty()) {
                                                    lhs = lhs.append(",");
                                                }
                                                return lhs.append("\"").append(rhs).append("\"");
                                            });
            FC_THROW("hashrate-collect-unit must be one of [ ${values} ]", ("values", values));
        }

        this->my->_difficulty = options["hashrate-difficulty"].as<uint64_t>();

        if (options.count("hashrate-output-path")) {
            this->my->_output_path_opt.emplace(options["hashrate-output-path"].as<std::string>());
        }
    }
    FC_LOG_AND_RETHROW()
}

namespace celesos {
    struct hashrate_metric_plugin_run_arg_t {
        hashrate_metric_plugin *plugin_ptr;
        int thread_id;
        std::shared_ptr<std::ofstream> stream_ptr;
        std::shared_ptr<std::vector<ethash::node>> cache_ptr;
        std::shared_ptr<std::vector<ethash::node>> dataset_ptr;
        uint256_t nonce_start;
        uint256_t nonce_stop;
    };
}

void *celesos::hashrate_metric_plugin::thread_run(void *arg_ptr) {
    auto casted_arg_ptr = static_cast<hashrate_metric_plugin_run_arg_t *>(arg_ptr);
    auto arg = *casted_arg_ptr;
    delete casted_arg_ptr;

    const auto &plugin_ptr = arg.plugin_ptr;
    const auto &thread_id = arg.thread_id;
    const auto &stream_ptr = arg.stream_ptr;
    const auto &cache = *arg.cache_ptr;
    const auto &dataset = *arg.dataset_ptr;
    const auto &nonce_start = arg.nonce_start;
    const auto &nonce_stop = arg.nonce_stop;
    const auto &forest = plugin_ptr->my->_forest;
    const auto &separator_symbol = plugin_ptr->my->_separator_symbol.c_str();
    const auto &dataset_count = plugin_ptr->my->_dataset_count;
    const auto &difficulty = plugin_ptr->my->_difficulty;
    const auto &collect_what = plugin_ptr->my->_collect_what;
    const auto &collect_unit = plugin_ptr->my->_collect_unit;
    const auto &target = uint256_t{difficulty} << 192;

    if (collect_what == "hash-full-count" || collect_what == "hash-light-count") {
        const auto &hash_func = collect_what == "hash-full-count" ? ethash::hash_full : ethash::hash_light;
        auto &relative_data = collect_what == "hash-full-count" ? dataset : cache;
        const auto threshold = collect_unit == "second" ? fc::seconds(1) : fc::minutes(1);

        uint256_t nonce{nonce_start};
        char buffer[1024];
        while (nonce <= nonce_stop) {
            const auto &start_time_in_micro = fc::time_point::now().time_since_epoch();
            uint64_t hash_count{0};
            do {
                hash_func(forest, nonce++, dataset_count, relative_data);
                ++hash_count;
            } while ((fc::time_point::now().time_since_epoch() - start_time_in_micro) < threshold);
            ilog("hashrate is: ${count}/${unit} on thread: ${id}",
                 ("unit", collect_unit)("id", thread_id)("count", hash_count));
            if (stream_ptr != nullptr) {
                auto &stream = *stream_ptr;
                auto len = sprintf(buffer,
                                   "%d%s%d%s%ld\n",
                                   thread_id,
                                   separator_symbol,
                                   fc::time_point::now().sec_since_epoch(),
                                   separator_symbol,
                                   hash_count);
                stream << std::string{buffer, static_cast<std::string::size_type>(len)};
                stream.flush();
            }
        }
    } else if (collect_what == "valid-hash-full-count" || collect_what == "valid-hash-light-count") {
        const auto &hash_func = collect_what == "valid-hash-full-count" ? ethash::hash_full : ethash::hash_light;
        auto &relative_data = collect_what == "valid-hash-full-count" ? dataset : cache;
        const auto threshold = collect_unit == "second" ? fc::seconds(1) : fc::minutes(1);

        uint256_t nonce{nonce_start};
        char buffer[1024];
        while (nonce <= nonce_stop) {
            const auto &start_time_in_micro = fc::time_point::now().time_since_epoch();
            uint64_t valid_hash_count{0};
            do {
                auto actual = hash_func(forest, nonce++, dataset_count, relative_data);
                if (actual <= target) {
                    ++valid_hash_count;
                }
            } while ((fc::time_point::now().time_since_epoch() - start_time_in_micro) < threshold);
            ilog("hashrate is: ${count}/${unit} on thread: ${id}",
                 ("unit", collect_unit)("id", thread_id)("count", valid_hash_count));


            if (stream_ptr != nullptr) {
                auto &stream = *stream_ptr;
                auto len = sprintf(buffer,
                                   "%d%s%d%s%ld\n",
                                   thread_id,
                                   separator_symbol,
                                   fc::time_point::now().sec_since_epoch(),
                                   separator_symbol,
                                   valid_hash_count);
                stream << std::string{buffer, static_cast<std::string::size_type>(len)};
                stream.flush();
            }
        }
    }

    pthread_exit(nullptr);
}

void celesos::hashrate_metric_plugin::plugin_startup() {
    const auto thread_priority = this->my->_thread_priority;
    const auto concurrency_count = this->my->_concurrency_count;
    const auto &seed = this->my->_seed;
    const auto cache_count = this->my->_cache_count;
    const auto dataset_count = this->my->_dataset_count;

    vector<pthread_t> threads{};

    std::shared_ptr<std::ofstream> stream_ptr{};

    if (this->my->_output_path_opt) {
        const auto &output_path = *this->my->_output_path_opt;
        stream_ptr = std::make_shared<std::ofstream>();
        stream_ptr->open(output_path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    }

    ilog("begin calc cache with thread: ${thread_id} priority: ${priority} count: ${count} seed: ${seed}",
         ("thread_id", 0)("priority", thread_priority)("count", cache_count)("seed", seed));
    auto cache_ptr = std::make_shared<std::vector<ethash::node>>(cache_count,
                                                                 std::vector<ethash::node>::allocator_type());
    ethash::calc_cache(*cache_ptr, cache_count, seed);
    ilog("end calc cache with thread: ${thread_id} count: ${count} seed: ${seed}",
         ("thread_id", 0)("count", cache_count)("seed", seed));
    ilog("begin calc dataset with thread: ${thread_id} count: ${count}",
         ("thread_id", 0)("count", dataset_count));
    auto dataset_ptr = std::make_shared<std::vector<ethash::node>>(dataset_count,
                                                                   std::vector<ethash::node>::allocator_type());
    ethash::calc_dataset(*dataset_ptr, dataset_count, *cache_ptr);
    ilog("end calc dataset with thread: ${thread_id} count: ${count}",
         ("thread_id", 0)("count", dataset_count));

    const auto &nonce_step = uint256_t{-1} / concurrency_count - 1;
    for (int idx = 0; idx < concurrency_count; ++idx) {
        pthread_attr_t attr{};
        pthread_attr_init(&attr);
        if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
            FC_THROW("fail to set sched policy");
        }

#ifdef __APPLE__
        sched_param param{
                .sched_priority = static_cast<int>(thread_priority),
        };
#else
        sched_param param{
                .__sched_priority = static_cast<int>(thread_priority),
        };
#endif

        if (pthread_attr_setschedparam(&attr, &param) != 0) {
            FC_THROW("fail to set sched param");
        }
        auto nonce_start = idx*nonce_step;
        auto nonce_stop = nonce_start + nonce_step - 1;
        auto arg_ptr = new hashrate_metric_plugin_run_arg_t{
                .plugin_ptr = this,
                .thread_id = idx + 1,
                .stream_ptr = stream_ptr,
                .cache_ptr = cache_ptr,
                .dataset_ptr = dataset_ptr,
                .nonce_start = std::move(nonce_start),
                .nonce_stop = std::move(nonce_stop),
        };
        pthread_t a_thread{};
        pthread_create(&a_thread, &attr, celesos::hashrate_metric_plugin::thread_run, arg_ptr);
        threads.emplace_back(std::move(a_thread));
    }
    for (auto &thread : threads) {
        pthread_join(thread, nullptr);
    }
}

void celesos::hashrate_metric_plugin::plugin_shutdown() {
}
