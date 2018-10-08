//
// Created by yale on 8/13/18.
//

#include <vector>
#include <random>
#include <celesos/pow/ethash.hpp>
#include <celesos/miner_plugin/worker.hpp>

using namespace std;
using namespace celesos;
using namespace eosio;

using boost::multiprecision::uint256_t;

void *celesos::miner::worker::thread_run(void *arg) {
    auto worker_ptr = static_cast<miner::worker *>(arg);

    std::stringstream buffer{};
    buffer << std::this_thread::get_id();
    auto thread_id = buffer.str();

    ilog("begin run() on thread: ${t_id}", ("t_id", thread_id));

    const auto &target = *worker_ptr->_ctx.target_ptr;
    const auto &dataset = *worker_ptr->_ctx.dataset_ptr;
    const auto dataset_count = dataset.size();
    const auto &seed = *worker_ptr->_ctx.seed_ptr;
    const auto &forest = *worker_ptr->_ctx.forest_ptr;
    auto retry_count = *worker_ptr->_ctx.retry_count_ptr;
    uint256_t nonce_current = *worker_ptr->_ctx.nonce_start_ptr;
    boost::optional<uint256_t> wood_opt{};
    do {
        read_lock_type lock{worker_ptr->_mutex};
        if (worker_ptr->_state == state::stopped) {
            break;
        }
        lock.unlock();

        if (ethash::hash_full(forest, nonce_current, dataset_count, dataset) <= target) {
            wood_opt = nonce_current;
//            ilog(
//                    "success to compute valid wood: ${wood} with \n\t\tseed:${seed} \n\t\tforest: ${forest} \n\t\ttarget:${target} \n\t\tdataset_count:${dataset_count}",
//                    ("wood", wood_opt->str(0, std::ios_base::hex).c_str())
//                            ("seed", seed.c_str())
//                            ("forest", forest.c_str())
//                            ("target", target.str(0, std::ios_base::hex).c_str())
//                            ("dataset_count", dataset_count)
//                    );
            worker_ptr->_ctx.io_service_ptr->post(
                    [signal = worker_ptr->_ctx.signal_ptr, block_num = worker_ptr->_ctx.block_num, wood_opt = wood_opt]() {
                        auto is_success = !!wood_opt;
                        (*signal)(std::move(is_success), block_num, wood_opt);
                    });
        }
        ++nonce_current;
    } while (--retry_count > 0);

    ilog("end run() on thread: ${t_id}", ("t_id", thread_id));
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

        ilog("begin create thread for mine");
        this->_alive_thread_opt.emplace();
        pthread_attr_t attr{};
        sched_param param{};
        pthread_attr_init(&attr);
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
        param.__sched_priority = 1;
        pthread_attr_setschedparam(&attr, &param);
        auto thread_ptr = this->_alive_thread_opt.get_ptr();

        pthread_create(thread_ptr, &attr, miner::worker::thread_run, this);
        ilog("end create thread for mine");
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


