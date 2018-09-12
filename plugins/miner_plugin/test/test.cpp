//
// Created by yalez on 2018/8/21.
//

#define BOOST_TEST_MODULE miner_plugin

#ifndef CELESOS_TEST_MOUDLE_MINER_PLUGIN_ON
#define CELESOS_TEST_MOUDLE_MINER_PLUGIN_ON
#endif //CELESOS_TEST_MOUDLE_MINER_PLUGIN_ON

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <eosio/testing/tester.hpp>
#include <celesos/pow/ethash.hpp>
#include <celesos/miner_plugin/worker.hpp>
#include <celesos/miner_plugin/miner.hpp>
#include <celesos/miner_plugin/miner_plugin.hpp>
#include <eosio/chain/forest_bank.hpp>

using namespace celesos;
using namespace eosio;

using boost::multiprecision::uint256_t;

BOOST_AUTO_TEST_SUITE(worker_suite)

    BOOST_AUTO_TEST_CASE(worker_test) {
        const static uint32_t HASH_BYTES = 64;
        const static uint64_t CACHE_BYTES = 1024;
        const static uint64_t DATASET_BYTES = 1024 * 32;

        const static auto CACHE_COUNT = static_cast<uint32_t>(CACHE_BYTES / HASH_BYTES);
        const static auto DATASET_COUNT = static_cast<uint32_t>(DATASET_BYTES / HASH_BYTES);

        auto target_ptr = std::make_shared<uint256_t>(197);
        auto &target = *target_ptr;
        target |= uint256_t{90} << 1;
        for (int i = 2; i < 31; ++i) {
            target |= uint256_t{255} << i * 8;
        }

        auto nonce_start_ptr = std::make_shared<uint256_t>();
        miner::miner::gen_random_uint256(*nonce_start_ptr);

        auto retry_count_ptr = std::make_shared<uint256_t>(-1);

        auto io_service_ptr = std::make_shared<boost::asio::io_service>();
        auto signal_ptr = std::make_shared<miner::mine_signal_type>();
        signal_ptr->connect([](auto is_success, auto block_num, auto wood) {
            BOOST_CHECK_EQUAL(is_success, true);
        });

        auto seed_ptr = std::make_shared<std::string>("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        auto forest_ptr = seed_ptr;

        BOOST_TEST_MESSAGE("begin prepare cache with count: " << CACHE_COUNT);
        auto cache_ptr = std::make_shared<std::vector<ethash::node>>(CACHE_COUNT);
        ethash::calc_cache(*cache_ptr, CACHE_COUNT, *seed_ptr);
        BOOST_TEST_MESSAGE("end prepare cache with count: " << CACHE_COUNT);

        BOOST_TEST_MESSAGE("begin prepare dataset with count: " << DATASET_COUNT);
        auto dataset_ptr = std::make_shared<std::vector<ethash::node>>(DATASET_COUNT);
        ethash::calc_dataset(*dataset_ptr, DATASET_COUNT, *cache_ptr);
        BOOST_TEST_MESSAGE("end prepare dataset with count: " << DATASET_COUNT);

        miner::worker_ctx ctx{
                .block_num = 1024,
                .io_service_ptr = io_service_ptr,
                .signal_ptr = signal_ptr,
                .nonce_start_ptr = nonce_start_ptr,
                .retry_count_ptr = retry_count_ptr,
                .forest_ptr = forest_ptr,
                .target_ptr = target_ptr,
                .dataset_ptr = dataset_ptr,
        };
        miner::worker worker{std::move(ctx)};
        worker.start();

        BOOST_TEST_MESSAGE("begin solve nonce");
        boost::asio::io_service::work work{*io_service_ptr};
        io_service_ptr->run_one();
        BOOST_TEST_MESSAGE("end solve nonce");
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(miner_suite)

    BOOST_FIXTURE_TEST_CASE(miner_test, testing::tester) {
        auto &controller_ref = *this->control;

        std::mutex mutex{};
        std::unique_lock<std::mutex> lock{mutex};
        std::condition_variable stop_signal{};
        auto bank = forest::forest_bank::getInstance(controller_ref);
        miner::miner miner{};
        miner.connect([&stop_signal, bank](bool is_success, chain::block_num_type block_num,
                                           const boost::optional<uint256_t> &wood_opt) {
            BOOST_CHECK_EQUAL(is_success, true);
            std::string wood_hex{};
            ethash::uint256_to_hex(wood_hex, *wood_opt);
            BOOST_CHECK_EQUAL(bank->verify_wood(block_num, "yale", wood_hex.c_str()), true);
            stop_signal.notify_all();
        });

        BOOST_TEST_MESSAGE("begin solve wood by miner");
        chain::account_name relative_account{"yale"};
        miner.start(std::move(relative_account), controller_ref);
        produce_blocks(1024);
        stop_signal.wait(lock);
        BOOST_TEST_MESSAGE("after wait");
        miner.stop();
        BOOST_TEST_MESSAGE("end solve wood by miner");
    }

BOOST_AUTO_TEST_SUITE_END()

//BOOST_AUTO_TEST_SUITE(miner_plugin_suite)
//
//BOOST_AUTO_TEST_SUITE_END()
