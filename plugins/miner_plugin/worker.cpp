//
// Created by yale on 8/13/18.
//

#include <vector>
#include <random>
#include <celesos/pow/ethash.h>
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
    const auto &nonce_start = *this->_ctx.nonce_start;
    const auto &target = *this->_ctx.target;
    const auto &retry_count = *this->_ctx.retry_count;
    const auto &dataset = *this->_ctx.dataset;
    const auto dataset_count = this->_ctx.dataset->size();
    const auto &forest = *this->_ctx.forest;
    uint256_t nonce_current = nonce_start;
    bool solved = false;
    do {
        auto lock = shared_lock<shared_timed_mutex>{this->_mutex};
        if (_state == state::stopped) {
            break;
        }
        lock.unlock();

        if (ethash::hash_full(forest, nonce_current, dataset_count, dataset) <= target) {
            solved = true;
            break;
        }
        ++nonce_current;
    } while (nonce_current >= nonce_start);

    if (!solved) {
        ilog("Fail to solve correct nonce");
        return;
    }

    this->_ctx.io_service->post(
            [signal = this->_ctx.signal, block_num = this->_ctx.block_num, &wood = nonce_current]() {
                (*signal)(block_num, wood);
            });
}

