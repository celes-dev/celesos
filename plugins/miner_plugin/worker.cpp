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

celesos::miner::worker::worker(worker_ctx ctx) : _ctx{std::move(ctx)}, _state{state::initialized} {
}

celesos::miner::worker::~worker() {
    this->stop();
}

void celesos::miner::worker::start() {
    auto lock = unique_lock<shared_timed_mutex>{this->_mutex};
    if (this->_state == state::initialized) {
        this->_state = state::started;
        this->_alive_thread.emplace(thread{std::bind(&worker::run, this)});
    }
    lock.unlock();
}

void celesos::miner::worker::stop(bool wait) {
    auto lock = unique_lock<shared_timed_mutex>{this->_mutex};
    if (this->_state == state::started) {
        this->_state = state::stopped;
        lock.unlock();

        if (wait) {
            if (this->_alive_thread && this->_alive_thread->joinable()) {
                this->_alive_thread->join();
            }
        }
    }
}

void celesos::miner::worker::run() {
    const auto &nonce_start = *this->_ctx.nonce_start_ptr;
    const auto &target = *this->_ctx.target_ptr;
    const auto &retry_count = *this->_ctx.retry_count_ptr;
    const auto &dataset = *this->_ctx.dataset_ptr;
    const auto dataset_count = dataset.size();
    const auto &forest = *this->_ctx.forest_ptr;
    uint256_t nonce_current = nonce_start;
    boost::optional<uint256_t> wood_opt{};
    do {
        auto lock = shared_lock<shared_timed_mutex>{this->_mutex};
        if (this->_state == state::stopped) {
            break;
        }
        lock.unlock();

        if (ethash::hash_full(forest, nonce_current, dataset_count, dataset) <= target) {
            wood_opt = nonce_current;
            break;
        }
        ++nonce_current;
    } while (nonce_current >= nonce_start);

    this->_ctx.io_service_ptr->post(
            [signal = this->_ctx.signal_ptr, block_num = this->_ctx.block_num, wood_opt = wood_opt]() {
                (*signal)(!!wood_opt, block_num, wood_opt);
            });
}

