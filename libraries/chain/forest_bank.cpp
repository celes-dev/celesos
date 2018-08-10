//
// Created by huberyzhang on 2018/7/25.
//

#include <eosio/chain/forest_bank.hpp>
#include "../include/eosio/chain/fork_database.hpp"
#include "../include/eosio/chain/controller.hpp"
#include "../include/eosio/chain/block.hpp"
#include "../include/eosio/chain/block_header.hpp"


namespace celesos{
    using namespace eosio;
    using namespace chain;
    namespace forest {
        static uint32_t question_space_number = 600;//问题间隔块数
        static uint32_t question_period = 21600;//问题有效期
        forest_bank::forest_bank() {}

        forest_bank::~forest_bank() {}

        forest_bank& forest_bank::get_instance(){

            if(nullptr == instance){
                instance = new forest_bank();
            }
            return *instance;
        }


        int get_index_of_signs(char ch) {
            if(ch >= '0' && ch <= '9')
            {
                return ch - '0';
            }
            if(ch >= 'A' && ch <='F')
            {
                return ch - 'A' + 10;
            }
            if(ch >= 'a' && ch <= 'f')
            {
                return ch - 'a' + 10;
            }
            return -1;
        }
        boost::multiprecision::uint512_t hex_to_dec(const char *source) {
            boost::multiprecision::uint512_t sum = 0;
            boost::multiprecision::uint512_t t = 1;
            size_t i;

            size_t len = strlen(source);
            for(i=len-1; i>=0; i--)
            {
                sum += t * get_index_of_signs(*(source + i));
                t *= 16;
            }

            return sum;
        }
        bool forest_bank::verify_wood(uint32_t block_number, const account_name& account, const uint64_t wood){
            const controller control = controller::controller(controller::config());
            uint32_t current_block_number = control.head_block_num();
            if(block_number <= current_block_number - question_period){
                //过期了
                return false;
            } else if(block_number%question_space_number != 0){
                //不是问题
                return false;
            }else{
                //这里验证答案是否正确
                signed_block_ptr block_ptr = control.fetch_block_by_number(block_number);
                //这里获取目标难度
                boost::multiprecision::uint512_t target = 1<<58;
                //准备需要验证的参数
                uint32_t cache_number = block_number/question_period;
                if(cache_number == first_cache_pair.first){
                    string cache = first_cache_pair.second;
                }

                block_id_type block_id = control.get_block_id_for_num(block_number);
                block_id_type wood_forest = fc::sha256::hash(block_id.str()+account.to_string());
                //调用验证函数
                fc::sha256 result_hash = fc::sha256::hash("wood");

                const char *result_hash_char = result_hash.str().c_str();
                boost::multiprecision::uint256_t result_value = hex_to_dec(result_hash_char);

                //update from meet
                if(result_value <= target){
                    //难度符合目标难度
                    return true;

                }else{
                    //难度不符合目标难度
                    return false;
                }
            }
        }

        void forest_bank::update_cache(uint32_t block_number){
            //保留最近的两个cache
            uint32_t current_cache_number = block_number/question_period;
            if(first_cache_pair.second == NULL)
            {
                first_cache_pair = std::make_pair(current_cache_number, "cache feed");
            }else {
                second_scahe_pair = first_cache_pair;
                first_cache_pair = std::make_pair(current_cache_number, "cache feed");
            }
        }

        bool forest_bank::get_forest(forest_struct& forest, const account_name& account){
            const controller control = controller::controller(controller::config());
            uint32_t current_block_number = control.head_block_num();
            //试图更新cache
            update_cache(current_block_number);

            // udpate
            uint32_t current_forest_number = current_block_number/question_space_number * question_space_number;


            if((forest_struct.target == 0)
               || current_forest_number != forest_struct.block_number){
                block_id_type result_value = control.get_block_id_for_num(current_forest_number);

                forest_data.seed = fc::sha256::hash(result_value.str());
                forest_data.forest = fc::sha256::hash(result_value.str()+ account.to_string());
                forest_data.block_number = current_forest_number;

                //计算难度
                boost::multiprecision::uint512_t value = 1<<58;

                forest_data.target = value;
            }
            return true;
        }


    }
}
