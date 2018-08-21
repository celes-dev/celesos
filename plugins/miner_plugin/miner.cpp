//
// Created by yale on 8/14/18.
//

#include <random>
#include <eosio/chain/forest_bank.hpp>
#include <celesos/miner_plugin/miner.hpp>

using namespace std;
using namespace eosio;
using namespace celesos;

using celesos::miner::worker;
using celesos::miner::worker_ctx;

using boost::multiprecision::uint256_t;
using boost::signals2::connection;

celesos::miner::miner::miner() : _alive_workers{std::thread::hardware_concurrency(),
                                                vector<shared_ptr<worker>>::allocator_type()},
                                 _signal{make_shared<celesos::miner::mine_signal_type>()},
                                 _io_thread{&celesos::miner::miner::run, this},
                                 _state{state::initialized},
                                 _failure_retry_interval_us{fc::milliseconds(5000)} {
}

celesos::miner::miner::~miner() {
    this->_io_work.reset();
    this->_signal->disconnect_all_slots();
    this->stop();
}

void celesos::miner::miner::start(const chain::account_name &relative_account, chain::controller &cc) {
    ilog("start() attempt");
    if (this->_state != state::initialized) {
        return;
    }
    ilog("start() begin");
    this->_state = state::started;

    auto slot = [this, &relative_account, &cc](const chain::block_state_ptr &block) {
        if (this->_last_failure_time_us) {
            auto &&passed_time_us = fc::time_point::now().time_since_epoch() - this->_last_failure_time_us.get();
            if (passed_time_us < this->_failure_retry_interval_us) {
                return;
            }
        }

        if (this->_next_block_num && block->block_num < this->_next_block_num.get()) {
            return;
        }

        bool exception_occured = true;
        try {
            this->on_forest_updated(relative_account, cc);
            exception_occured = false;
        } FC_LOG_AND_DROP()

        if (exception_occured) {
            this->_last_failure_time_us = fc::time_point::now().time_since_epoch();
            elog("Fail to handle \"on_forest_update\"");
            return;
        }

        // clear last_failure_time_us for performance
        this->_last_failure_time_us.reset();
    };
    auto a_connection = cc.accepted_block_header.connect(std::move(slot));
    this->_connections.push_back(std::move(a_connection));
    ilog("start() end");
}

void celesos::miner::miner::stop(bool wait) {
    ilog("stop(wait = ${wait}) begin", ("wait", wait));
    if (this->_state == state::stopped) {
        return;
    }

    this->_state = state::stopped;
    for (auto &&x : this->_alive_workers) {
        x->stop(wait);
    }
    this->_alive_workers.clear();
    for (auto &&x : this->_connections) {
        x.disconnect();
    }
    this->_connections.clear();
    ilog("stop(wait = ${wait}) end", ("wait", wait));
}

connection celesos::miner::miner::connect(const celesos::miner::mine_slot_type &slot) {
    return _signal->connect(slot);
}

void celesos::miner::miner::on_forest_updated(const chain::account_name &relative_account, chain::controller &cc) {
    ilog("on forest updated");

    auto &bank = *forest::forest_bank::getInstance(cc);
    forest::forest_struct forest_info{};
    if (!bank.get_forest(forest_info, relative_account)) {
        //TODO 考虑是否需要定制一个exception
        FC_THROW_EXCEPTION(fc::unhandled_exception,
                           "Fail to get forest with account: ${account}",
                           ("account", relative_account));
    }

    this->_next_block_num = forest_info.next_block_num;
    ilog("update field \"next_block_number\" with value: ${block_num}", ("block_num", forest_info.next_block_num));

    const auto target = make_shared<uint256_t>(0);
    string_to_uint256_little(*target, forest_info.target.str());
    const auto forest = make_shared<string>(forest_info.forest.str());

    // prepare cache and dataset
    const auto cache_count = forest::cache_count();
    const auto cache = make_shared<vector<ethash::node>>(cache_count, vector<ethash::node>::allocator_type());
    ethash::calc_cache(*cache, cache_count, forest_info.seed);
    const auto dataset_count = forest::dataset_count();
    const auto dataset = make_shared<vector<ethash::node>>(dataset_count, vector<ethash::node>::allocator_type());
    ethash::calc_dataset(*dataset, dataset_count, *cache);

    this->stop(true);

    const auto &core_count = std::thread::hardware_concurrency();

    auto retry_count = make_shared<uint256_t>(static_cast<uint256_t>(-1));
    *retry_count /= core_count;

    uint256_t nonce_init{0};
    gen_random_uint256(nonce_init);

    this->_alive_workers.resize(core_count);
    for (int i = 0; i < core_count; ++i) {
        auto &&nonce_start = make_shared<uint256_t>(nonce_init + (*retry_count) * i);
        worker_ctx ctx{
                .forest = forest,
                .target = target,
                .dataset = dataset,
                .retry_count = retry_count,
                .nonce_start = nonce_start,
                .signal = this->_signal,
                .io_service = this->_io_service,
        };
        this->_alive_workers.push_back(make_shared<worker>(std::move(ctx)));
    }

    for (auto &&x : this->_alive_workers) {
        x->start();
    }
}

void celesos::miner::miner::run() {
    this->_io_service = make_shared<boost::asio::io_service>();
    this->_io_work = make_shared<boost::asio::io_service::work>(*this->_io_service);
    this->_io_service->run();
}

void celesos::miner::miner::string_to_uint256_little(uint256_t &dst, const std::string &str) {
    EOS_ASSERT(str.size() <= 32, chain::invalid_arg_exception, "string size() must lge 32");

    dst = 0;

    uint256_t tmp{0};
    auto count = str.size();
    for (int i = 0; i < count; ++i) {
        tmp = str[i];
        tmp <<= i * 8;
        dst |= tmp;
    }
}

void celesos::miner::miner::gen_random_uint256(uint256_t &dst) {
    random_device rd{};
    mt19937_64 gen{rd()};
    uniform_int_distribution<uint64_t> dis{};
    dst = 0;
    for (int i = 0; i < 4; ++i) {
        dst <<= i;
        dst |= dis(gen);
    }
}
