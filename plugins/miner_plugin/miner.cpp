//
// Created by yale on 8/14/18.
//

#include <random>
#include <cmath>
#include <boost/algorithm/hex.hpp>
#include <eosio/chain/forest_bank.hpp>
#include <celesos/miner_plugin/miner.hpp>

using namespace std;
using namespace eosio;
using namespace celesos;

using celesos::miner::worker;
using celesos::miner::worker_ctx;

using boost::multiprecision::uint256_t;
using boost::signals2::connection;

celesos::miner::miner::miner(const fc::logger &logger,
                             boost::asio::io_service &main_io_service,
                             unsigned int worker_count,
                             uint32_t sleep_interval_sec,
                             float sleep_probability) :
        _logger{logger},
        _alive_worker_ptrs{0, vector<shared_ptr<worker>>::allocator_type()},
        _signal_ptr{make_shared<celesos::miner::mine_signal_type>()},
        _main_io_service_ref{main_io_service},
        _io_thread{&celesos::miner::miner::run, this},
        _state{state::initialized},
        _worker_count{worker_count},
        _failure_retry_interval_us{fc::milliseconds(5000)},
        _sleep_interval_sec{sleep_interval_sec},
        _sleep_probability{sleep_probability},
        _last_job_id{0} {
}

celesos::miner::miner::~miner() {
    if (this->_io_work_ptr) {
        this->_io_work_ptr.reset();
    }
    if (this->_signal_ptr) {
        this->_signal_ptr->disconnect_all_slots();
        this->_signal_ptr.reset();
    }
    this->stop();
    if (this->_sub_io_service_ptr) {
        this->_sub_io_service_ptr->stop();
        this->_sub_io_service_ptr.reset();
    }
    if (this->_io_thread.joinable()) {
        this->_io_thread.join();
    }
}

void celesos::miner::miner::start(const chain::account_name &relative_account, chain::controller &cc) {
    fc_dlog(_logger, "start() attempt");
    if (this->_state == state::started) {
        return;
    }
    fc_dlog(_logger, "start() begin");
    this->_state = state::started;

    auto slot = [this, &relative_account, &cc](const chain::block_state_ptr block_ptr) {
        if (this->_last_failure_time_us) {
            auto &&passed_time_us = fc::time_point::now().time_since_epoch() - this->_last_failure_time_us.get();
            if (passed_time_us < this->_failure_retry_interval_us) {
                return;
            }
        }

        if (this->_last_forest_info_opt && (
                this->_last_forest_info_opt->next_block_num == 0 ||
                block_ptr->block_num < this->_last_forest_info_opt->next_block_num
        )) {
            return;
        }

        bool exception_occurred = true;
        try {

            auto &bank = *forest::forest_bank::getInstance(cc);

            forest::forest_struct new_forest_info{};
            if (!bank.get_forest(new_forest_info, relative_account)) {
                FC_THROW_EXCEPTION(fc::unhandled_exception,
                                   "Fail to get forest with account: ${account}",
                                   ("account", relative_account));
            }

            bool no_old_forest = !this->_last_forest_info_opt;

            bool is_forest_updated = false;
            if (!no_old_forest) {
                auto lhs = *this->_last_forest_info_opt;
                auto &rhs = new_forest_info;
                if (lhs.seed != rhs.seed || lhs.forest != rhs.forest || lhs.target != rhs.target) {
                    is_forest_updated = true;
                }
            }

            if (no_old_forest || is_forest_updated) {
                fc_ilog(_logger, "\n\tsuccess to get latest forest_info with "
                                 "\n\t\tseed: ${seed} "
                                 "\n\t\tforest: ${forest} "
                                 "\n\t\tblock_num: ${block_num}"
                                 "\n\t\tnext_block_num: ${next_block_num}"
                                 "\n\t\ttarget: ${target}",
                        ("seed", new_forest_info.seed.str())
                                ("forest", new_forest_info.forest.str())
                                ("block_num", new_forest_info.block_number)
                                ("next_block_num", new_forest_info.next_block_num)
                                ("target", new_forest_info.target.str(0, std::ios_base::hex)));


                this->_last_job_id += 1;
                auto old_forest_info = no_old_forest ? new_forest_info : *this->_last_forest_info_opt;

                auto handler = std::bind(&miner::on_forest_updated, this,
                                         old_forest_info, new_forest_info,
                                         relative_account, no_old_forest);
                this->_sub_io_service_ptr->post(handler);
            }

            this->_last_forest_info_opt = new_forest_info;

            exception_occurred = false;
        } catch (fc::exception &er) {
            wlog("${details}", ("details", er.to_detail_string()));
        } catch (const std::exception &e) {
            fc::exception fce{FC_LOG_MESSAGE(warn, "rethrow ${what}: ", ("what", e.what())),
                              fc::std_exception_code, BOOST_CORE_TYPEID(e).name(), e.what()};
            wlog("${details}", ("details", fce.to_detail_string()));
        } catch (...) {
            fc::unhandled_exception e{FC_LOG_MESSAGE(warn, "rethrow"), std::current_exception()};
            wlog("${details}", ("details", e.to_detail_string()));
        }

        if (exception_occurred) {
            this->_last_failure_time_us = fc::time_point::now().time_since_epoch();
            fc_elog(_logger, "Fail to handle \"on_forest_update\"");
            return;
        }

        // clear last_failure_time_us for performance
        this->_last_failure_time_us.reset();
    };
    auto a_connection = cc.accepted_block_header.connect(slot);
    this->_connections.push_back(std::move(a_connection));
    fc_dlog(_logger, "start() end");
}

void celesos::miner::miner::stop_workers(bool wait) {
    for (auto &x : this->_alive_worker_ptrs) {
        if (x) {
            x->stop(wait);
            x.reset();
        }
    }
    this->_alive_worker_ptrs.clear();
}

void celesos::miner::miner::stop(bool wait) {
//    fc_ilog(_logger, "stop(wait = ${wait}) begin", ("wait", wait));
    if (this->_state == state::stopped) {
        return;
    }
    this->_state = state::stopped;

    for (auto &x : this->_connections) {
        x.disconnect();
    }
    this->_connections.clear();

    this->stop_workers(wait);

//    fc_ilog(_logger, "stop(wait = ${wait}) end", ("wait", wait));
}

connection celesos::miner::miner::connect(const celesos::miner::mine_slot_type &slot) {
    return _signal_ptr->connect(slot);
}

void celesos::miner::miner::on_forest_updated(const forest::forest_struct &old_forest_info,
                                              const forest::forest_struct &new_forest_info,
                                              const chain::account_name &relative_account, bool force) {
    fc_ilog(_logger, "on forest updated");

    auto current_job_id = this->_last_job_id;
//    const auto target_ptr = make_shared<uint256_t>(
//            "0x0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    const auto target_ptr = make_shared<uint256_t>(new_forest_info.target);

    const auto seed_ptr = make_shared<string>(new_forest_info.seed.str());
    const auto forest_ptr = make_shared<string>(new_forest_info.forest.str());

    // prepare cache and dataset_ptr
    const static auto is_cache_changed_func = [](const forest::forest_struct &old_forest,
                                                 const forest::forest_struct &new_forest,
                                                 const boost::optional<uint32_t> old_cache_count_opt,
                                                 uint32_t new_cache_count) {
        return !old_cache_count_opt ||
               old_forest.forest != new_forest.forest ||
               *old_cache_count_opt != new_cache_count;
    };
//    const uint32_t new_cache_count{512};

    if (this->_state == state::stopped) {
        fc_ilog(_logger, "miner has been stop, quit");
        return;
    } else if (current_job_id != this->_last_job_id) {
        fc_ilog(_logger, "forest has changed, cancel job");
        return;
    }

    const auto new_cache_count = forest::cache_count();
    const auto is_cache_changed = is_cache_changed_func(old_forest_info, new_forest_info,
                                                        this->_target_cache_count_opt, new_cache_count);

    shared_ptr<vector<ethash::node>> cache_ptr{};
    if (force || is_cache_changed) {
        fc_ilog(_logger, "begin prepare cache with count: ${count}", ("count", new_cache_count));
        cache_ptr = make_shared<vector<ethash::node>>(new_cache_count, vector<ethash::node>::allocator_type());
        ethash::calc_cache(*cache_ptr, new_cache_count, *seed_ptr);
        fc_ilog(_logger, "end prepare cache with count: ${count}", ("count", new_cache_count));
    } else {
        fc_dlog(_logger, "use cache generated");
        cache_ptr = *this->_target_cache_ptr_opt;
    }

    const static auto is_dataset_changed_func = [](bool is_cache_changed,
                                                   const boost::optional<uint32_t> &old_dataset_count_opt,
                                                   uint32_t new_dataset_count) {
        return is_cache_changed ||
               !old_dataset_count_opt ||
               old_dataset_count_opt != new_dataset_count;
    };

    if (this->_state == state::stopped) {
        fc_ilog(_logger, "miner has been stop, quit");
        return;
    } else if (current_job_id != this->_last_job_id) {
        fc_ilog(_logger, "forest has changed, quit");
        return;
    }

//    const uint32_t new_dataset_count{512 * 16};
    const auto new_dataset_count = forest::dataset_count();

    const auto is_dataset_changed = is_dataset_changed_func(is_cache_changed,
                                                            this->_target_dataset_count_opt, new_dataset_count);
    shared_ptr<vector<ethash::node>> dataset_ptr{};
    if (force || is_dataset_changed) {
        fc_ilog(_logger, "begin prepare dataset with count: ${count}", ("count", new_dataset_count));
        dataset_ptr = make_shared<vector<ethash::node>>(new_dataset_count,
                                                        vector<ethash::node>::allocator_type());

        auto &dataset = *dataset_ptr;
        auto &cache = *cache_ptr;
        dataset.resize(new_dataset_count);
        for (uint32_t i = 0; i < new_dataset_count; ++i) {
            if (i % 1000 == 0) {
                if (this->_state == state::stopped) {
                    fc_ilog(_logger, "miner has been stop, quit");
                    return;
                } else if (current_job_id != this->_last_job_id) {
                    fc_ilog(_logger, "forest has changed, quit");
                    return;
                }
            }
            dataset[i] = calc_dataset_item(cache, i);
        }

//        ethash::calc_dataset(*dataset_ptr, new_dataset_count, *cache_ptr);
        fc_ilog(_logger, "end prepare dataset with count: ${count}", ("count", new_dataset_count));
    } else {
        fc_dlog(_logger, "use dataset generated");
        dataset_ptr = *this->_target_dataset_ptr_opt;
    }

    const static auto is_work_changed_func = [](bool is_dataset_changed,
                                                const forest::forest_struct &old_forest,
                                                const forest::forest_struct &new_forest) {
        return is_dataset_changed || old_forest.forest != new_forest.forest;
    };

    if (this->_state == state::stopped) {
        fc_ilog(_logger, "miner has been stop, quit");
        return;
    } else if (current_job_id != this->_last_job_id) {
        fc_ilog(_logger, "forest has changed, quit");
        return;
    }

    const auto is_work_changed = is_work_changed_func(is_dataset_changed, old_forest_info, new_forest_info);
    if (!force && !is_work_changed) {
        fc_dlog(_logger, "work not changed,no need to restart workers");
    } else {
        fc_dlog(_logger, "restart workers to logging");
        this->stop_workers(true);

        auto retry_count_ptr = make_shared<uint256_t>(-1);
        *retry_count_ptr /= _worker_count;

        uint256_t nonce_init{0};
        gen_random_uint256(nonce_init);

        this->_alive_worker_ptrs.resize(_worker_count);
        for (int i = 0; i < _worker_count; ++i) {
            auto nonce_start_ptr = make_shared<uint256_t>(nonce_init + (*retry_count_ptr) * i);
            worker_ctx ctx{
                    .logger = _logger,
                    .dataset_ptr = dataset_ptr,
                    .seed_ptr = seed_ptr,
                    .forest_ptr = forest_ptr,
                    .nonce_start_ptr = std::move(nonce_start_ptr),
                    .retry_count_ptr = retry_count_ptr,
                    .target_ptr = target_ptr,
                    .block_num = new_forest_info.block_number,
                    .io_service_ref = this->_main_io_service_ref,
                    .signal_ptr = this->_signal_ptr,
                    .sleep_interval_sec = this->_sleep_interval_sec,
                    .sleep_probability = this->_sleep_probability,
            };
            this->_alive_worker_ptrs[i] = make_shared<worker>(std::move(ctx));
        }

        if (this->_state == state::stopped) {
            fc_ilog(_logger, "miner has been stop, quit");
            return;
        } else if (current_job_id != this->_last_job_id) {
            fc_ilog(_logger, "forest has changed, quit");
            return;
        }

        for (auto &x : this->_alive_worker_ptrs) {
            x->start();
        }
    }

    // update target fields
    this->_target_cache_ptr_opt = cache_ptr;
    this->_target_dataset_ptr_opt = dataset_ptr;
    this->_target_cache_count_opt = new_cache_count;
    this->_target_dataset_count_opt = new_dataset_count;
}

void celesos::miner::miner::run() {
    this->_sub_io_service_ptr = make_shared<boost::asio::io_service>();
    this->_io_work_ptr = make_shared<boost::asio::io_service::work>(std::ref(*this->_sub_io_service_ptr));
    this->_sub_io_service_ptr->run();
}

void celesos::miner::miner::gen_random_uint256(uint256_t &dst) {
    random_device rd{};
    mt19937_64 gen{rd()};
    uniform_int_distribution<uint64_t> dis{};
    dst = 0;
    for (int i = 0; i < 4; ++i) {
        dst <<= 64;
        dst |= dis(gen);
    }
}
