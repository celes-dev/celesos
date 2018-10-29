//
// Created by huberyzhang on 2018/7/25.
//

#include <eosio/chain/forest_bank.hpp>

namespace celesos
{
using namespace eosio;
using namespace chain;
namespace forest
{
//        static uint32_t question_space_number = 600;//问题间隔块数
//        static uint32_t question_period = 21600;//问题有效期

uint32_t dataset_count()
{
             return 1024*1024*1024/64;
  // return 512 * 16;
}

uint32_t cache_count()
{
             return 1024*1024*16/64;
  // return 512;
}

static forest_bank *instance = nullptr;

forest_bank *forest_bank::getInstance(controller &control)
{
  if (instance == nullptr)
  {
    instance = new forest_bank(control);
  }
  return instance;
}

forest_bank::forest_bank(controller &control) : chain(control)
{

  std::vector<celesos::ethash::node> vector;
  std::pair<uint32_t, std::vector<celesos::ethash::node>> temp_cache =
      std::make_pair((uint32_t)0, vector);
  first_cache_pair = std::make_shared<cache_pair_type>(temp_cache);
  second_cache_pair = std::make_shared<cache_pair_type>(std::move(temp_cache));

  if (chain.head_block_num() > cache_period_number())
  {
    forest_bank::update_cache_with_block_number(chain.head_block_num() -
                                                cache_period_number());
  }

  forest_bank::update_cache_with_block_number(chain.head_block_num());

  chain.db().add_index<forest_info_multi_index>();
  forest_bank::loadBlockCacheInfo();

  // set accepted_block signal function
  chain.accepted_block.connect(
      boost::bind(&forest_bank::update_cache, this, _1));
  chain.accepted_block.connect(
      boost::bind(&forest_bank::update_forest, this, _1));
}

forest_bank::~forest_bank() {}

uint32_t forest_bank::forest_period_number()
{
  uint32_t result_value = 24 * 60 * 21 * 6;
#ifdef DEBUG
  result_value = 60 * 21 * 6;
#endif
  return result_value;
}

uint32_t forest_bank::cache_period_number()
{
   uint32_t result_value = 24 * 60 * 21 * 6 * 3;
#ifdef DEBUG
   result_value = 60 * 21 * 6 * 2;
#endif
   return result_value;
}

uint32_t forest_bank::forest_space_number()
{
  uint32_t result_value = 21 * 6 * 10;
#ifdef DEBUG
  result_value = 21 * 6;
#endif
  return result_value;
}

eosio::chain::block_id_type
forest_bank::getBlockIdFromCache(const uint32_t block_number)
{
  auto iter = block_cache.find(block_number);
  if (iter != block_cache.end())
  {
    return block_cache[block_number].first;
  }
  else
  {
    elog("can not fount block id from cache:${block_number}",
         ("block_number", block_number));
    eosio::chain::block_id_type block_id;
    return block_id;
  }
}

double forest_bank::getBlockDiffFromCache(const uint32_t block_number)
{
  auto iter = block_cache.find(block_number);
  if (iter != block_cache.end())
  {
    return block_cache[block_number].second;
  }
  else
  {
    elog("can not fount diff from cache:${block_number}",
         ("block_number", block_number));
    return 1.0;
  }
}

void forest_bank::loadBlockCacheInfo()
{
  const auto &idx =
      chain.db().get_index<forest_info_multi_index, by_block_number>();

  auto itr = idx.cbegin();
  while (itr != idx.cend())
  {
    ilog("load block cache block "
         "number:${block_number},block_id:${block_id},diff:${diff}",
         ("block_number", itr->block_number)("block_id",
                                             itr->block_id)("diff", itr->diff));

    forest_bank::update_forest_with_block_number(itr->block_number + 1,
                                                 itr->diff,
                                                 false);
    block_cache[itr->block_number] = std::make_pair(itr->block_id, itr->diff);
    itr++;
  }
}

void forest_bank::cacheBlockInfo(const uint32_t block_number,
                                 const eosio::chain::block_id_type block_id,
                                 const double diff)
{
  const auto &idx =
      chain.db().get_index<forest_info_multi_index, by_block_number>();
  auto &mutable_idx = chain.db().get_mutable_index<forest_info_multi_index>();
  auto obj = idx.lower_bound(block_number);
  if (obj != idx.cend())
  {
    mutable_idx.remove(*obj);
  }

  chain.db().create<forest_info_cache_object>(
      [&](forest_info_cache_object &obj) {
        obj.block_number = block_number;
        obj.block_id = block_id;
        obj.diff = diff;
      });

  block_cache[block_number] = std::make_pair(block_id, diff);
  cleanBlockCache(block_number);
}

void forest_bank::cleanBlockCache(const uint32_t block_number)
{
  if (block_number <= 2 * forest_period_number() + 1)
    return;

  std::vector<uint32_t> cachevector;
  auto iter = block_cache.cbegin();
  while (iter != block_cache.cend())
  {
    if (iter->first + 2 * forest_period_number() + 1 < block_number)
    {
      cachevector.emplace_back(iter->first);
    }
    iter++;
  }

  for (auto iter = cachevector.cbegin(); iter != cachevector.cend(); iter++)
  {
    ilog("block_cache:erase for key:${key}", ("key", *iter));
    block_cache.erase(*iter);
  }

  std::vector<uint32_t> dbvector;

  const auto &idx =
      chain.db().get_index<forest_info_multi_index, by_block_number>();
  auto itr = idx.cbegin();
  while (itr != idx.cend())
  {
    if (itr->block_number + 2 * forest_period_number() + 1 < block_number)
    {
      dbvector.emplace_back(itr->block_number);
      itr++;
    }
    else
    {
      break;
    }
  }

  auto &mutable_idx = chain.db().get_mutable_index<forest_info_multi_index>();

  for (auto iter = dbvector.cbegin(); iter != dbvector.cend(); iter++)
  {
    ilog("db_block_cache:erase for key:${key}", ("key", *iter));
    auto obj = idx.find(*iter);
    mutable_idx.remove(*obj);
  }
}

bool forest_bank::verify_wood(uint32_t block_number,
                              const account_name &account, const char *wood)
{
  std::ostringstream oss;
  oss << boost::this_thread::get_id();
  std::string stid = oss.str();

  dlog("verify wood at threadid:${threadid}", ("threadid", stid));
  dlog("verify wood 1 at time: ${time}",
       ("time", fc::time_point::now().time_since_epoch().count()));
  uint32_t current_block_number = chain.head_block_num();

  if (block_number + forest_period_number() <= current_block_number ||
      block_number >= current_block_number)
  {
    // wood is past due
    return false;
  }
  else if (block_number > 1 &&
           (block_number - 1) % forest_space_number() != 0)
  {
    // not forest
    return false;
  }
  else
  {
    // get forest target

    dlog("verify wood 2 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));
    double double_target = forest_bank::getBlockDiffFromCache(block_number);
    double temp_double_target = static_cast<double>(chain.origin_difficulty());
    temp_double_target = temp_double_target / double_target;
    uint256_t target = static_cast<uint256_t>(temp_double_target);

    // prepare parameter for ethash
    uint32_t cache_number =
        (block_number - 1) / cache_period_number() * cache_period_number() +
        1;
    std::vector<celesos::ethash::node> *cache_data_ptr = &(first_cache_pair->second);
    if (cache_number == first_cache_pair->first)
    {
       // do nothing...
    }
    else if (cache_number == second_cache_pair->first)
    {
      cache_data_ptr = &(second_cache_pair->second);
    }
    else
    {
      // not found matched period
      return false;
    }

    std::vector<celesos::ethash::node> &cache_data = *cache_data_ptr;

    dlog("verify wood 3 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));
    block_id_type block_id = forest_bank::getBlockIdFromCache(block_number);
    dlog("verify wood 4 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));
    block_id_type wood_forest =
        fc::sha256::hash(block_id.str() + account.to_string());
    dlog("verify wood 5 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));
    uint32_t data_set_count = dataset_count();

    auto block_id_tmp =
        forest_bank::getBlockIdFromCache(first_cache_pair->first).str();
    block_id_type seed = fc::sha256::hash(block_id_tmp);
    ilog("calc seed with "
         "\n\t\tblock_num: ${block_num}"
         "\n\t\tblock_id: ${block_id}"
         "\n\t\tseed: ${seed}",
         ("block_num", first_cache_pair->first)("block_id", block_id_tmp)(
             "seed", seed.str()));
    dlog("verify wood 6 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));

    // call ethash verify wood
    auto result_value = celesos::ethash::hash_light_hex(
        wood_forest, string(wood), data_set_count, cache_data);
    dlog("verify wood 7 at time: ${time}",
         ("time", fc::time_point::now().time_since_epoch().count()));

    dlog("forest_bank::verify_wood block_value:${block_number}",
         ("block_number", block_number));
    dlog("forest_bank::verify_wood seed:${seed}", ("seed", seed.str()));
    dlog("forest_bank::verify_wood double_target:${double_target}",
         ("double_target", double_target));
    dlog("forest_bank::verify_wood temp_double_target:${temp_double_target}",
         ("temp_double_target", target.str(0, std::ios_base::hex)));
    dlog("forest_bank::verify_wood target_value:${target}",
         ("target", target.str(0, std::ios_base::hex)));
    dlog("forest_bank::verify_wood wood_forest:${wood_forest}",
         ("wood_forest", wood_forest));
    dlog("forest_bank::verify_wood wood:${wood}", ("wood", wood));
    dlog("forest_bank::verify_wood data_set_count:${data_set_count}",
         ("data_set_count", data_set_count));
    dlog("forest_bank::verify_wood result_value:${result_value}",
         ("result_value", result_value.str(0, std::ios_base::hex)));

    return result_value <= target;
  }
}

void forest_bank::update_cache(const block_state_ptr &block)
{
  forest_bank::update_cache_with_block_number(chain.head_block_num());
}

void forest_bank::update_cache_with_block_number(uint32_t current_blck_number)
{
  // store current and last period feed cache for verify wood
  if (current_blck_number <= 1)
  {
    return;
  }

  uint32_t block_number = current_blck_number - 2;
  uint32_t current_cache_number =
      block_number / cache_period_number() * cache_period_number() + 1;

  if (!(first_cache_pair->second.empty()) &&
      first_cache_pair->first == current_cache_number)
  {
    // in same period not need update
    return;
  }

  std::vector<celesos::ethash::node> node_vector;
  uint32_t dataset_count = cache_count();
  block_id_type seed =
      fc::sha256::hash(chain.get_block_id_for_num(current_cache_number).str());

  if (celesos::ethash::calc_cache(node_vector, dataset_count, seed.str()))
  {
    if (!(first_cache_pair->second.empty()) &&
        first_cache_pair->second.size() > 0)
    {
      if (!(second_cache_pair->second.empty()) &&
          second_cache_pair->second.size() > 0)
      {
        second_cache_pair->second.clear();
      }
      second_cache_pair = first_cache_pair;
    }

    std::pair<uint32_t, std::vector<celesos::ethash::node>> temp_cache =
        std::make_pair(current_cache_number, std::move(node_vector));
    //                first_cache_pair = &temp_cache;

    first_cache_pair = std::make_shared<cache_pair_type>(std::move(temp_cache));
  }
}

void forest_bank::update_forest(const block_state_ptr &block)
{
  forest_bank::update_forest_with_block_number(chain.head_block_num(),
                                               chain.get_forest_diff(),
                                               true
                                               );
}

void forest_bank::update_forest_with_block_number(const uint32_t current_block_number,
                                                  const double diff,const bool bcache)
{
  if (current_block_number <= 2)
  {
    return;
  }

  uint32_t current_forest_number = (current_block_number - 2) /
                                       forest_space_number() *
                                       forest_space_number() +
                                   1;

  if (current_forest_number > forest_data.block_number)
  {
    block_id_type result_value =
        chain.get_block_id_for_num(current_forest_number);
    block_id_type seed_value;
    if (first_cache_pair->first == current_forest_number)
    {
      seed_value = result_value;
    }
    else
    {
      seed_value = chain.get_block_id_for_num(first_cache_pair->first);
    }

    //计算难度
    double temp_double_target = static_cast<double>(chain.origin_difficulty());
    temp_double_target = temp_double_target / diff;
    uint256_t value = static_cast<uint256_t>(temp_double_target);

    forestLock.lock();

    forest_data.seed = fc::sha256::hash(seed_value.str());
    forest_data.forest = result_value;
    forest_data.block_number = current_forest_number;
    forest_data.next_block_num = current_forest_number + forest_space_number();
    forest_data.target = value;

    ilog("calc seed with"
         "\n\t\tseed: ${seed} "
         "\n\t\tblock_num_1: ${block_num_1} "
         "\n\t\tblock_num_2: ${block_num_2} "
         "\n\t\ttarget: ${target}",
         ("seed", forest_data.seed.str())(
             "block_num_1", first_cache_pair->first)("block_num_2",
                                                     forest_data.block_number)(
             "target", forest_data.target.str(0, std::ios_base::hex)));

    forestLock.unlock();

    std::ostringstream oss;
    oss << boost::this_thread::get_id();
    std::string stid = oss.str();

    if(bcache)
    {
      dlog("cacheBlockInfo at threadId:${threadid}", ("threadid", stid));
      forest_bank::cacheBlockInfo(current_forest_number, result_value, diff);
    }

    ilog("update forest with "
         "\n\t\tnumber: ${current_forest_number} "
         "\n\t\tresult_value: ${result_value} "
         "\n\t\tdiff: ${diff} "
         "\n\t\ttarget: ${target}",
         ("current_forest_number", current_forest_number)(
             "result_value", result_value)("diff", diff)("target", value));
  }
}

bool forest_bank::get_forest(forest_struct &forest,
                             const account_name &account)
{
  uint32_t current_block_number = chain.head_block_num();
  if (current_block_number <= 1 || forest_data.block_number == 0)
  {
    return false;
  }

  forestLock.lock();

  forest.seed = forest_data.seed;
  forest.forest =
      fc::sha256::hash(forest_data.forest.str() + account.to_string());
  forest.block_number = forest_data.block_number;
  forest.next_block_num = forest_data.next_block_num;
  forest.target = forest_data.target;

  forestLock.unlock();

  return true;
}
} // namespace forest
} // namespace celesos
