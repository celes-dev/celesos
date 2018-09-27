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
        this->_alive_thread_opt.emplace(std::bind(&worker::run, this));
    }
    lock.unlock();
}

void celesos::miner::worker::stop(bool wait) {
    write_lock_type lock{this->_mutex};
    if (this->_state == state::started) {
        this->_state = state::stopped;
        lock.unlock();

        if (this->_alive_thread_opt && this->_alive_thread_opt->joinable()) {
            if (wait) {
                this->_alive_thread_opt->join();
            } else {
                this->_alive_thread_opt->detach();
            }
            this->_alive_thread_opt.reset();
        }
    }
}

void celesos::miner::worker::run() {
    std::stringstream buffer{};
    buffer << std::this_thread::get_id();
    auto thread_id = buffer.str();

    ilog("begin run() on thread: ${t_id}", ("t_id", thread_id));

    const auto &target = *this->_ctx.target_ptr;
    const auto &dataset = *this->_ctx.dataset_ptr;
    const auto dataset_count = dataset.size();
    const auto &seed = *this->_ctx.seed_ptr;
    const auto &forest = *this->_ctx.forest_ptr;
    auto retry_count = *this->_ctx.retry_count_ptr;
    uint256_t nonce_current = *this->_ctx.nonce_start_ptr;
    boost::optional<uint256_t> wood_opt{};
    do {
        read_lock_type lock{this->_mutex};
        if (this->_state == state::stopped) {
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
            this->_ctx.io_service_ptr->post(
                    [signal = this->_ctx.signal_ptr, block_num = this->_ctx.block_num, wood_opt = wood_opt]() {
                        auto is_success = !!wood_opt;
                        (*signal)(std::move(is_success), block_num, wood_opt);
                    });
        }
        ++nonce_current;
    } while (--retry_count > 0);

    ilog("end run() on thread: ${t_id}", ("t_id", thread_id));
}

