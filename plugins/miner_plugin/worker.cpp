//
// Created by yale on 8/13/18.
//

#include <vector>
#include <random>
#include <eosio/miner_plugin/worker.h>
#include <celesos/pow/ethash.h>

using namespace std;
using namespace celesos;
using namespace eosio;

using boost::multiprecision::uint256_t;

namespace celesos {
    namespace miner {

        worker::worker(worker_ctx ctx) : _ctx{std::move(ctx)}, _state{state::initialized} {
        }

        worker::~worker() {
            this->stop();
        }

        void worker::start() {
            auto lock = unique_lock<shared_timed_mutex>{this->_mutex};
            if (this->_state == state::initialized) {
                this->_state = state::started;
                this->_alive_thread.emplace(thread{std::bind(&worker::run, this)});
            }
            lock.unlock();
        }

        void worker::stop(bool wait) {
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

        void worker::run() {
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
                //TODO 处理没有算出正确nonce的流程
                return;
            }

            this->_ctx.io_service->post([signal = this->_ctx.signal, &answer = nonce_current]() {
                (*signal)(answer);
            });
        }
    }
}

