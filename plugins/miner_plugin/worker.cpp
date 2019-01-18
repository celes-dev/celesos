//
// Created by yale on 8/13/18.
//

#include <vector>
#include <random>
#include <thread>
#include <celesos/pow/ethash.hpp>
#include <celesos/miner_plugin/worker.hpp>

using namespace std;
using namespace celesos;
using namespace eosio;
using std::chrono::system_clock;
using boost::multiprecision::uint256_t;

void *celesos::miner::worker::thread_run(void *arg) {
    auto worker_ptr = static_cast<miner::worker *>(arg);

    auto &logger = worker_ptr->_ctx.logger;

    fc_dlog(logger, "begin run()");

    const auto &target = *worker_ptr->_ctx.target_ptr;
    const auto &dataset = *worker_ptr->_ctx.dataset_ptr;
    const auto dataset_count = dataset.size();
    const auto &seed = *worker_ptr->_ctx.seed_ptr;
    const auto &forest = *worker_ptr->_ctx.forest_ptr;
    auto retry_count = *worker_ptr->_ctx.retry_count_ptr;
    uint256_t nonce_current = *worker_ptr->_ctx.nonce_start_ptr;
    uint32_t sleep_interval_sec = worker_ptr->_ctx.sleep_interval_sec;
    float sleep_probability = worker_ptr->_ctx.sleep_probability;
    boost::optional<uint256_t> wood_opt{};
    std::default_random_engine engine{};
    std::uniform_real_distribution<float> distribution{0.0f, 1.0f};

    auto next_check_time = system_clock::now() + std::chrono::seconds{sleep_interval_sec};
    do {
        read_lock_type lock{worker_ptr->_mutex};
        if (worker_ptr->_state == state::stopped) {
            break;
        }
        lock.unlock();

        if (sleep_interval_sec > 0 && sleep_probability > 0 && system_clock::now() >= next_check_time) {
            if (distribution(engine) <= sleep_probability) {
                std::this_thread::sleep_for(std::chrono::seconds{sleep_interval_sec});
                continue;
            }
            next_check_time = system_clock::now() + std::chrono::seconds{sleep_interval_sec};
        }

        if (ethash::hash_full(forest, nonce_current, dataset_count, dataset) <= target) {
            wood_opt = nonce_current;
            fc_dlog(logger,
                    "\n\tsuccess to compute valid wood with "
                    "\n\t\twood: ${wood} "
                    "\n\t\tseed: ${seed} "
                    "\n\t\tforest: ${forest} "
                    "\n\t\ttarget: ${target} "
                    "\n\t\tdataset_count: ${dataset_count} ",
                    ("wood", wood_opt->str(0, std::ios_base::hex).c_str())
                            ("seed", seed.c_str())
                            ("forest", forest.c_str())
                            ("target", target.str(0, std::ios_base::hex).c_str())
                            ("dataset_count", dataset_count)
            );
            worker_ptr->_ctx.io_service_ref.post(
                    [signal = worker_ptr->_ctx.signal_ptr, block_num = worker_ptr->_ctx.block_num, wood_opt = wood_opt]() {
                        auto is_success = !!wood_opt;
                        (*signal)(std::move(is_success), block_num, wood_opt);
                    });
            std::this_thread::sleep_for(std::chrono::seconds{1}); // if success,then sleep 1 second.
        }
        ++nonce_current;
    } while (--retry_count > 0);

    fc_dlog(logger, "end run()");
    pthread_exit(nullptr);
}

celesos::miner::worker::worker(worker_ctx ctx) :
        _ctx{std::move(ctx)},
        _state{state::initialized} {
}

celesos::miner::worker::~worker() {
    this->stop();
}

void celesos::miner::worker::start() {
    write_lock_type lock{this->_mutex};
    if (this->_state == state::initialized) {
        this->_state = state::started;
//        this->_alive_thread_opt.emplace(std::bind(&worker::run, this));
//        auto native_handle = this->_alive_thread_opt->native_handle();

        auto &logger = _ctx.logger;
        fc_dlog(logger, "begin create thread for mine");
        this->_alive_thread_opt.emplace();
        pthread_attr_t attr{};
        sched_param param{};
        pthread_attr_init(&attr);
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
#ifdef __APPLE__
        param.sched_priority = 1;
#else
        param.__sched_priority = 1;
#endif

        pthread_attr_setschedparam(&attr, &param);
        auto thread_ptr = this->_alive_thread_opt.get_ptr();

        pthread_create(thread_ptr, &attr, miner::worker::thread_run, this);
        fc_dlog(logger, "end create thread for mine");
        this->_alive_thread_opt.emplace(std::move(*thread_ptr));
    }
    lock.unlock();
}

void celesos::miner::worker::stop(bool wait) {
    write_lock_type lock{this->_mutex};
    if (this->_state == state::started) {
        this->_state = state::stopped;
        lock.unlock();

        if (this->_alive_thread_opt) {
            auto thread_ptr = this->_alive_thread_opt.get_ptr();
            if (wait) {
                pthread_join(*thread_ptr, nullptr);
//                this->_alive_thread_opt->join();
            } else {
                pthread_detach(*thread_ptr);
//                this->_alive_thread_opt->detach();
            }
            this->_alive_thread_opt.reset();
        }
    }
}


