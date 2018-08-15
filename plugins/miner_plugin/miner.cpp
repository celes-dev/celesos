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
using boost::signal2::signal;

celesos::miner::miner::miner() : _alive_workers{std::thread::hardware_concurrency(), vector<shared_ptr<worker>>::allocator_type()},
                                 _signal{signal<void(const uint256_t &)>{}} {
}

miner::miner::~miner() {
    this->stop();
}

void celesos::miner::miner::start(chain::controller &cc) {
    const chain::name &voter_account{"yale"};
    const auto &voter_pk = chain::private_key_type::generate();
    const chain::name &producer_account{"eospacific"};

    //TODO 以单例的形式获取forest_bank
    forest::forest_bank bank{cc};
    forest::forest_struct forest_info{};
    if (!bank.get_forest(forest_info, voter_account)) {
        //TODO 处理异常流程
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
        };
        this->_alive_workers.push_back(make_shared<worker>(std::move(ctx)));
    }

    for (auto &&x : this->_alive_workers) {
        x->start();
    }
}

void celesos::miner::miner::stop(bool wait) {
    for (auto &&x : this->_alive_workers) {
        x->stop(wait);
    }
    this->_alive_workers.clear();
    for (auto &&x : this->_connections) {
        x.disconnect();
    }
    this->_connections.clear();
}

void submit_wood(chain::controller &cc, const uint256_t &wood) {
    const chain::name &voter_account{"yale"};
    const auto &voter_pk = chain::private_key_type::generate();
    const chain::name &producer_account{"eospacific"};

    //TODO 处理算出nonce的流程
    const auto &chain_id = cc.get_chain_id();
    auto tx = chain::signed_transaction{};
    auto permission_levels = vector<chain::permission_level>{
            {voter_account, "active"}};

    auto a_action_vote = miner::action_vote{
            .producer = producer_account,
            .voter = voter_account,
            .wood = wood,
    };
    tx.actions.emplace_back(permission_levels, a_action_vote);
    tx.expiration = cc.head_block_time() + fc::seconds(30);
    tx.set_reference_block(chain_id);
    tx.sign(voter_pk, chain_id);
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

void celesos::miner::miner::gen_random_uint256(boost::multiprecision::uint256_t &dst) {
    random_device rd{};
    mt19937_64 gen{rd()};
    uniform_int_distribution<uint64_t> dis{};
    dst = 0;
    for (int i = 0; i < 4; ++i) {
        dst <<= i;
        dst |= dis(gen);
    }
}


