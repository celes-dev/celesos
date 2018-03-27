#pragma once
#include <eosio/chain/types.hpp>
#include <eosio/chain/config.hpp>

#include "multi_index_includes.hpp"


namespace eosio { namespace chain { namespace resource_limits {

   namespace impl {
      template<typename T>
      struct ratio {
         T numerator;
         T denominator;
      };

      template<typename T>
      ratio<T> make_ratio(T n, T d) {
         return ratio<T>{n, d};
      }

      template<typename T>
      T operator* (T value, const ratio<T>& r) {
         return (value * r.numerator) / r.denominator;
      }

      /**
       *  This class accumulates and exponential moving average based on inputs
       *  This accumulator assumes there are no drops in input data
       *
       *  The value stored is Precision times the sum of the inputs.
       */
      template<uint64_t Precision = config::rate_limiting_precision>
      struct exponential_moving_average_accumulator
      {
         exponential_moving_average_accumulator()
         : last_ordinal(0)
         , value(0)
         {
         }

         uint32_t   last_ordinal;
         uint64_t   value;

         /**
          * return the average value in rate_limiting_precision
          */
         uint64_t average() const { return value; }

         void add( uint64_t units, uint32_t ordinal, uint32_t window_size )
         {
            if( last_ordinal != ordinal ) {
               const auto decay = make_ratio(
                  (uint64_t) window_size - 1,
                  (uint64_t) window_size
               );

               value = value * decay;
            }

            value += units * Precision;
         }
      };
   }

   using usage_accumulator = impl::exponential_moving_average_accumulator<>;
   using ratio = impl::ratio<uint64_t>;

   /**
    * Every account that authorizes a transaction is billed for the full size of that transaction. This object
    * tracks the average usage of that account.
    */
   struct resource_limits_object : public chainbase::object<resource_limits_object_type, resource_limits_object> {

      OBJECT_CTOR(resource_limits_object)

      id_type id;
      account_name owner;
      bool pending = false;

      int64_t net_weight = -1;
      int64_t cpu_weight = -1;
      int64_t ram_bytes = -1;

   };

   struct by_owner;

   using resource_limits_index = chainbase::shared_multi_index_container<
      resource_limits_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<resource_limits_object, resource_limits_object::id_type, &resource_limits_object::id>>,
         ordered_unique<tag<by_owner>,
            composite_key<resource_limits_object,
               BOOST_MULTI_INDEX_MEMBER(resource_limits_object, bool, pending),
               BOOST_MULTI_INDEX_MEMBER(resource_limits_object, account_name, owner)
            >
         >
      >
   >;

   struct resource_usage_object : public chainbase::object<resource_usage_object_type, resource_usage_object> {
      OBJECT_CTOR(resource_usage_object)

      id_type id;
      account_name owner;

      usage_accumulator        net_usage;
      usage_accumulator        cpu_usage;
      uint64_t                 ram_usage = 0;
   };

   using resource_usage_index = chainbase::shared_multi_index_container<
      resource_usage_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<resource_usage_object, resource_usage_object::id_type, &resource_usage_object::id>>,
         ordered_unique<tag<by_owner>, member<resource_usage_object, account_name, &resource_usage_object::owner> >
      >
   >;

   struct elastic_limit_parameters {
      uint64_t target;           // the desired usage
      uint64_t max;              // the maximum usage
      uint32_t periods;          // the number of aggregation periods that contribute to the average usage

      uint32_t max_multiplier;   // the multiplier by which virtual space can oversell usage when uncongested
      ratio    contract_rate;    // the rate at which a congested resource contracts its limit
      ratio    expand_rate;       // the rate at which an uncongested resource expands its limits
   };

   class resource_limits_config_object : public chainbase::object<resource_limits_config_object_type, resource_limits_config_object> {
      OBJECT_CTOR(resource_limits_config_object);
      id_type id;

      elastic_limit_parameters cpu_limit_parameters = {config::default_target_block_acts, config::default_max_block_acts, config::compute_average_window_ms / config::block_interval_ms, 1000, {99, 100}, {1000, 999}};
      elastic_limit_parameters net_limit_parameters = {config::default_target_block_size, config::default_max_block_size, config::blocksize_average_window_ms / config::block_interval_ms, 1000, {99, 100}, {1000, 999}};
   };

   using resource_limits_config_index = chainbase::shared_multi_index_container<
      resource_limits_config_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<resource_limits_config_object, resource_limits_config_object::id_type, &resource_limits_config_object::id>>
      >
   >;

   class resource_limits_state_object : public chainbase::object<resource_limits_state_object_type, resource_limits_state_object> {
      OBJECT_CTOR(resource_limits_state_object);
      id_type id;

      /**
       * Track the average netusage for blocks
       */
      usage_accumulator average_block_net_usage;

      /**
       * Track the average cpu usage for blocks
       */
      usage_accumulator average_block_cpu_usage;

      void update_virtual_net_limit( const resource_limits_config_object& cfg );
      void update_virtual_cpu_limit( const resource_limits_config_object& cfg );

      uint64_t total_net_weight = 0;
      uint64_t total_cpu_weight = 0;
      uint64_t total_ram_bytes = 0;

      /* TODO: we have to guarantee that we don't over commit our ram.  This can be tricky if multiple threads are
       * updating limits in parallel.  For now, we are not running in parallel so this is a temporary measure that
       * allows us to quarantine this guarantee from the rest of the otherwise parallel-compatible code
       */
      uint64_t speculative_ram_bytes = 0;

      /**
       * The virtual number of bytes that would be consumed over blocksize_average_window_ms
       * if all blocks were at their maximum virtual size. This is virtual because the
       * real maximum block is less, this virtual number is only used for rate limiting users.
       *
       * It's lowest possible value is max_block_size * blocksize_average_window_ms / block_interval
       * It's highest possible value is 1000 times its lowest possible value
       *
       * This means that the most an account can consume during idle periods is 1000x the bandwidth
       * it is gauranteed under congestion.
       *
       * Increases when average_block_size < target_block_size, decreases when
       * average_block_size > target_block_size, with a cap at 1000x max_block_size
       * and a floor at max_block_size;
       **/
      uint64_t virtual_net_limit = 0;

      /**
       *  Increases when average_bloc
       */
      uint64_t virtual_cpu_limit = 0;

   };

   using resource_limits_state_index = chainbase::shared_multi_index_container<
      resource_limits_state_object,
      indexed_by<
         ordered_unique<tag<by_id>, member<resource_limits_state_object, resource_limits_state_object::id_type, &resource_limits_state_object::id>>
      >
   >;

} } } /// eosio::chain::resource_limits

CHAINBASE_SET_INDEX_TYPE(eosio::chain::resource_limits::resource_limits_object,        eosio::chain::resource_limits::resource_limits_index)
CHAINBASE_SET_INDEX_TYPE(eosio::chain::resource_limits::resource_usage_object,         eosio::chain::resource_limits::resource_usage_index)
CHAINBASE_SET_INDEX_TYPE(eosio::chain::resource_limits::resource_limits_config_object, eosio::chain::resource_limits::resource_limits_config_index)
CHAINBASE_SET_INDEX_TYPE(eosio::chain::resource_limits::resource_limits_state_object,  eosio::chain::resource_limits::resource_limits_state_index)
