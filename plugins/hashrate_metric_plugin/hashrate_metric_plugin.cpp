/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
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
        boost::optional<std::string> _output_path_opt;

        hashrate_metric_plugin_impl() : _seed{},
                                        _forest{},
                                        _separator_symbol{},
                                        _cache_count{0},
                                        _dataset_count{0},
                                        _thread_priority{0},
                                        _concurrency_count{0},
                                        _output_path_opt{} {
        }
    };
}

celesos::hashrate_metric_plugin::hashrate_metric_plugin() : my(new hashrate_metric_plugin_impl()) {}

celesos::hashrate_metric_plugin::~hashrate_metric_plugin() {}

void celesos::hashrate_metric_plugin::set_program_options(options_description &, options_description &cfg) {
    cfg.add_options()
            ("hashrate-seed", boost::program_options::value<string>(), "seed to generate cache")
            ("hashrate-forest", boost::program_options::value<string>(), "forest to about calc hash")
            ("hashrate-cache-count", boost::program_options::value<uint32_t>()->default_value(512),
             "count of cache's element")
            ("hashrate-dataset-count", boost::program_options::value<uint32_t>()->default_value(1024),
             "count of dataset's element")
            ("hashrate-thread-priority", boost::program_options::value<uint32_t>()->default_value(1),
             "thread priority")
            ("hashrate-concurrency-count", boost::program_options::value<uint32_t>()->default_value(1),
             "concurrency count for calc hash")
            ("hashrate-separator-symbol", boost::program_options::value<std::string>()->default_value(";"),
             "separator symbol for output data")
            ("hashrate-output-path", boost::program_options::value<std::string>(), "output path for csv data");
}

void celesos::hashrate_metric_plugin::plugin_initialize(const variables_map &options) {
    try {
        if (!options.count("hashrate-seed")) {
            FC_THROW("hashrate-seed must be set");
        } else if (!options.count("hashrate-forest")) {
            FC_THROW("hashrate-forest must be set");
        } else if (!options.count("hashrate-cache-count")) {
            FC_THROW("hashrate-cache-count must be set");
        } else if (!options.count("hashrate-dataset-count")) {
            FC_THROW("hashrate-dataset-count must be set");
        }

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
    };
}

void *celesos::hashrate_metric_plugin::thread_run(void *arg) {
    auto casted_arg = static_cast<hashrate_metric_plugin_run_arg_t *>(arg);
    auto plugin_ptr = casted_arg->plugin_ptr;
    auto thread_id = casted_arg->thread_id;
    auto stream_ptr = casted_arg->stream_ptr;
    auto &cache = *casted_arg->cache_ptr;
    const auto &forest = plugin_ptr->my->_forest;
    const auto separator_symbol = plugin_ptr->my->_separator_symbol.c_str();
    const auto dataset_count = plugin_ptr->my->_dataset_count;

    char buffer[1024];
    for (;;) {
        const auto &start_time_in_micro = fc::time_point::now().time_since_epoch();
        static const auto threashold = fc::seconds(1);
        uint64_t hash_count{0};
        uint256_t nonce{0};
        do {
            ethash::hash_light(forest, nonce++, dataset_count, cache);
            ++hash_count;
        } while ((fc::time_point::now().time_since_epoch() - start_time_in_micro) < threashold);
        ilog("hashrate is: ${hash_count}/sec on thread: ${thread_id}",
             ("thread_id", thread_id)("hash_count", hash_count));
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

    for (int thread_id = 1; thread_id <= concurrency_count; ++thread_id) {
        pthread_attr_t attr{};
        pthread_attr_init(&attr);
        if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
            FC_THROW("fail to set sched policy");
        }
        sched_param param{
                .__sched_priority = static_cast<int>(thread_priority),
        };

        if (pthread_attr_setschedparam(&attr, &param) != 0) {
            FC_THROW("fail to set sched param");
        }
        hashrate_metric_plugin_run_arg_t arg{
                .plugin_ptr = this,
                .thread_id = thread_id,
                .stream_ptr = stream_ptr,
                .cache_ptr = cache_ptr,
                .dataset_ptr = dataset_ptr,
        };
        pthread_t a_thread{};
        pthread_create(&a_thread, &attr, celesos::hashrate_metric_plugin::thread_run, &arg);
        threads.emplace_back(std::move(a_thread));
    }
    for (auto &thread : threads) {
        void *thread_ret;
        pthread_join(thread, &thread_ret);
    }
}

void celesos::hashrate_metric_plugin::plugin_shutdown() {
}
