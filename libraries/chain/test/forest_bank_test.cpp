//
// Created by huberyzhang on 2018/8/17.
//

#define BOOST_TEST_MODULE forest_bank

#include <eosio/chain/forest_bank.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/testing/tester.hpp>


boost::optional<eosio::testing::tester> tester_opt{};

struct global_fixture {

    global_fixture() {
        tester_opt.emplace();
    }

    ~global_fixture() {
        tester_opt.reset();
    }
};

BOOST_GLOBAL_FIXTURE(global_fixture);

BOOST_AUTO_TEST_SUITE(forest_bank_test)

BOOST_AUTO_TEST_CASE(verify_wood_test)
    {
        auto &control = *tester_opt->control;

        celesos::forest::forest_bank *forest_bank = celesos::forest::forest_bank::getInstance(control);
        bool isRight = forest_bank->verify_wood(1,123,123456789);
        BOOST_REQUIRE(&isRight);

    }


    BOOST_AUTO_TEST_CASE(get_forest_test)
    {
        auto &control = *tester_opt->control;

        celesos::forest::forest_struct forest = {};

        celesos::forest::forest_bank *forest_bank = celesos::forest::forest_bank::getInstance(control);
        bool isRight = forest_bank->get_forest(forest,123456789);
        BOOST_REQUIRE(&forest);
        BOOST_REQUIRE(&isRight);
    }
BOOST_AUTO_TEST_SUITE_END()
