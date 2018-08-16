/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <appbase/application.hpp>

namespace eosio {

using namespace appbase;

/**
 *  This is a template plugin, intended to serve as a starting point for making new plugins
 */
class miner_plugin : public appbase::plugin<miner_plugin> {
public:
   miner_plugin();
   virtual ~miner_plugin();
 
   APPBASE_PLUGIN_REQUIRES()
   void set_program_options(options_description&, options_description& cfg) override;
 
   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   std::unique_ptr<class miner_plugin_impl> my;
};

}
