/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#ifndef CELESOS_MINER_PLUGIN_H
#define CELESOS_MINER_PLUGIN_H

#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/http_client_plugin/http_client_plugin.hpp>

namespace celesos {

    class miner_plugin : public appbase::plugin<miner_plugin> {
    public:
        miner_plugin();

        virtual ~miner_plugin();

        APPBASE_PLUGIN_REQUIRES((eosio::chain_plugin) (eosio::http_client_plugin))

        void set_program_options(appbase::options_description &, appbase::options_description &cfg) override;

        void plugin_initialize(const appbase::variables_map &options);

        void plugin_startup();

        void plugin_shutdown();

    private:
        std::shared_ptr<class miner_plugin_impl> my;
    };
}

#endif //CELESOS_MINER_PLUGIN_H
