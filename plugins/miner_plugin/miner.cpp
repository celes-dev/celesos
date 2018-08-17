//
// Created by yale on 8/14/18.
//

#include <random>
#include <eosio/miner_plugin/miner.hpp>
#include <eosio/chain/forest_bank.hpp>

using namespace std;
using namespace eosio;
using namespace celesos;

using celesos::miner::worker;
using celesos::miner::worker_ctx;

using boost::multiprecision::uint256_t;
using boost::signals2::connection;

celesos::miner::miner::miner() : _alive_workers{std::thread::hardware_concurrency(),
                                                vector<shared_ptr<worker>>::allocator_type()},
                                 _signal{make_shared<boost::signals2::signal<slot_type>>()},
                                 _io_thread{&celesos::miner::miner::run, this},
                                 _state{state::initialized} {
}

miner::miner::~miner() {
    this->_io_work.reset();
    this->_signal->disconnect_all_slots();
    this->stop();
}

void celesos::miner::miner::start(chain::controller &cc) {
    ilog("start() attempt");
    if (this->_state != state::initialized) {
        return;
    }
    ilog("start() begin");
    this->_state = state::started;

    auto a_connection = cc.accepted_block_header.connect([this, &cc](const chain::block_state_ptr &block) {
        this->on_forest_updated(cc);
    });
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

connection celesos::miner::miner::connect(const std::function<slot_type> &slot) {
    return _signal->connect(slot);
}

void celesos::miner::miner::on_forest_updated(chain::controller &cc) {
    ilog("on forest updated");

    //TODO 修改相关账户信息的获取方式
    const chain::name &voter_account{"yale"};
    const auto &voter_pk = chain::private_key_type::generate();
    const chain::name &producer_account{"eospacific"};

    auto &bank = *forest::forest_bank::getInstance(cc);
    forest::forest_struct forest_info{};
    if (!bank.get_forest(forest_info, voter_account)) {
        //TODO 处理异常流程
        return;
    }

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
    if (str.size() > 32) {
        //TODO 兼容string大于uint256数值范围的异常
        return;
    }

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
