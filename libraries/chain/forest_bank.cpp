//
// Created by huberyzhang on 2018/7/25.
//

#include <eosio/chain/forest_bank.hpp>
#include "eosio/chain/fork_database.hpp"
#include "eosio/chain/block.hpp"
#include "eosio/chain/block_header.hpp"

namespace celesos{
    using namespace eosio;
    using namespace chain;
    namespace forest {
        static uint32_t question_space_number = 600;//问题间隔块数
        static uint32_t question_period = 21600;//问题有效期
        uint256_t original_target("0x0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

        uint32_t dataset_count(){
//            return 1024*1024*1024/64;
            return 512*16;

        }
        uint32_t cache_count(){
//            return 1024*1024*16/64;
            return 512;
        }

        static forest_bank *instance = nullptr;
        forest_bank* forest_bank::getInstance(controller &control){
            if (instance == nullptr)
            {
                instance = new forest_bank(control);
            }
            return instance;
        }

        forest_bank::forest_bank(controller &control) : chain(control) {

            //set accepted_block signal function
            chain.accepted_block.connect(boost::bind(&forest_bank::update_cache,this,_1));
        }

        forest_bank::~forest_bank(){}


        bool forest_bank::verify_wood(uint32_t block_number, const account_name& account, const uint64_t wood){
            uint32_t current_block_number = chain.head_block_num();
            if(block_number <= current_block_number - question_period){
                //wood is past due
                return false;
            } else if(block_number%question_space_number != 0){
                //not forest
                return false;
            }else{
                //in here verify wood is validity
                signed_block_ptr block_ptr = chain.fetch_block_by_number(block_number);
                //get forest target
                optional<double> diff = block_ptr->difficulty;
                double double_target = *diff;
                uint256_t target_int = static_cast<uint256_t>(double_target*100);
                uint256_t target = (original_target)/target_int/100;
                //prepare parameter for ethash
                uint32_t cache_number = block_number/question_period;
                std::vector<celesos::ethash::node> cache_data;
                if(cache_number == first_cache_pair.first){
                    cache_data = first_cache_pair.second;
                }else if(cache_number == second_scahe_pair.first){
                    cache_data = second_scahe_pair.second;
                }else{
                    //not found matched period
                    return false;
                }

                block_id_type block_id = chain.get_block_id_for_num(block_number);
                block_id_type wood_forest = fc::sha256::hash(block_id.str()+account.to_string());
                uint32_t data_set_count = dataset_count();
                //call ethash verify wood
                auto result_value = celesos::ethash::hash_light(wood_forest,wood,data_set_count,cache_data);

                return result_value <= target;
            }
        }

        void forest_bank::update_cache(const block_state_ptr& block){
            //store current and last period feed cache for verify wood
            uint32_t block_number = chain.head_block_num();
            uint32_t current_cache_number = block_number/question_period;
            if(first_cache_pair.first == current_cache_number){
                //in same period not need update
                return;
            }

            std::vector<celesos::ethash::node> node_vector;
            uint32_t dataset_count = cache_count();
            block_id_type seed = chain.get_block_id_for_num(current_cache_number);
            if(celesos::ethash::calc_cache(node_vector,dataset_count,seed.str())){
                first_cache_pair = std::make_pair(current_cache_number, node_vector);
            }

            if(!(first_cache_pair.second.empty()))
            {
                second_scahe_pair = first_cache_pair;
            }
        }

        bool forest_bank::get_forest(forest_struct& forest, const account_name& account){
            uint32_t current_block_number = chain.head_block_num();

            uint32_t current_forest_number = current_block_number/question_space_number * question_space_number+1;

            if((forest.target == 0) || (current_forest_number != forest.block_number)){
                block_id_type result_value = chain.get_block_id_for_num(current_forest_number);

                forest_data.seed = fc::sha256::hash(result_value.str());
                forest_data.forest = fc::sha256::hash(result_value.str()+ account.to_string());
                forest_data.block_number = current_forest_number;

                //计算难度
                signed_block_ptr block_ptr = chain.fetch_block_by_number(current_forest_number);
                optional<double> diff = block_ptr->difficulty;
                double double_target = 1.0;
                if(diff.valid()){
                    double_target = *diff;
                }
                
                uint256_t target_int = static_cast<uint256_t>(double_target*100);
                uint256_t value = (original_target)/target_int/100;

                forest_data.target = value;
            }
            forest = forest_data;
            return true;
        }
    }
}
