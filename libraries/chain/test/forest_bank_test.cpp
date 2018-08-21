//
// Created by huberyzhang on 2018/8/17.
//

#define BOOST_TEST_MODULE forest_bank

#include <eosio/chain/forest_bank.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/testing/tester.hpp>

#include "boost/test/included/unit_test.hpp"
#include "boost/multiprecision/cpp_int.hpp"


using tester = eosio::testing::tester;

BOOST_AUTO_TEST_SUITE(forest_bank_test)

BOOST_AUTO_TEST_CASE(get_forest_test)
{
     tester test;
     eosio::chain::controller *control = test.control.get();

     celesos::forest::forest_struct forest = {};

     celesos::forest::forest_bank *forest_bank = celesos::forest::forest_bank::getInstance(*control);
     forest_bank->get_forest(forest,123456789);
     BOOST_REQUIRE(&forest);
}

    BOOST_AUTO_TEST_CASE(verify_wood_test)
    {
        tester test;
        eosio::chain::controller *control = test.control.get();

        celesos::forest::forest_bank *forest_bank = celesos::forest::forest_bank::getInstance(*control);
        bool isRight = forest_bank->verify_wood(1,123,123456789);
        BOOST_REQUIRE(&isRight);
    }

BOOST_AUTO_TEST_SUITE_END()