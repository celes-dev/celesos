/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <appbase/application.hpp>

namespace celesos {
    class hashrate_metric_plugin : public appbase::plugin<hashrate_metric_plugin> {
    private:
        std::unique_ptr<class hashrate_metric_plugin_impl> my;

        static void* thread_run(void *arg);
    public:

        hashrate_metric_plugin();

        virtual ~hashrate_metric_plugin();

        APPBASE_PLUGIN_REQUIRES()

        virtual void
        set_program_options(appbase::options_description &, appbase::options_description &cfg) override;

        void plugin_initialize(const appbase::variables_map &options);

        void plugin_startup();

        void plugin_shutdown();
    };
}
