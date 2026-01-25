/**
 * @file test_cdr_pai_config.cpp
 * @brief Unit test for RxCdrTdf module - PAI Configuration
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrPaiTest, PAIConfiguration) {
    CdrParams params;

    // Test default values
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;
    EXPECT_GT(params.pai.resolution, 0.0) << "Resolution should be positive";
    EXPECT_GT(params.pai.range, 0.0) << "Range should be positive";

    // Test different resolutions
    params.pai.resolution = 5e-13;  // 0.5ps
    EXPECT_DOUBLE_EQ(params.pai.resolution, 5e-13);

    params.pai.resolution = 5e-12;  // 5ps
    EXPECT_DOUBLE_EQ(params.pai.resolution, 5e-12);

    // Test different ranges
    params.pai.range = 1e-11;   // +/-10ps
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-11);

    params.pai.range = 1e-10;   // +/-100ps
    EXPECT_DOUBLE_EQ(params.pai.range, 1e-10);

    // Range should be larger than resolution
    params.pai.resolution = 1e-12;
    params.pai.range = 5e-11;
    EXPECT_GT(params.pai.range, params.pai.resolution);
}
