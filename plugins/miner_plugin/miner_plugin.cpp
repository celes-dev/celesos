/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/miner_plugin/miner_plugin.hpp>

namespace eosio {
   static appbase::abstract_plugin& _miner_plugin = app().register_plugin<miner_plugin>();

class miner_plugin_impl {
   public:
};

miner_plugin::miner_plugin():my(new miner_plugin_impl()){}
miner_plugin::~miner_plugin(){}

void miner_plugin::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
         ("option-name", bpo::value<string>()->default_value("default value"),
          "Option Description")
         ;
}

void miner_plugin::plugin_initialize(const variables_map& options) {
   try {
      if( options.count( "option-name" )) {
         // Handle the option
      }
   }
   FC_LOG_AND_RETHROW()
}

void miner_plugin::plugin_startup() {
   // Make the magic happen
}

void miner_plugin::plugin_shutdown() {
   // OK, that's enough magic
}

}
