/**
 * @file test_cdr_edge_threshold_config.cpp
 * @brief Unit test for RxCdrTdf module - Edge Threshold Configuration
 */

#include "cdr_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(CdrEdgeTest, EdgeThresholdConfiguration) {
    CdrParams params;
    
    // Test default edge threshold
    EXPECT_DOUBLE_EQ(params.pi.edge_threshold, 0.5);
    
    // Test custom thresholds
    params.pi.edge_threshold = 0.3;
    EXPECT_DOUBLE_EQ(params.pi.edge_threshold, 0.3);
    
    params.pi.edge_threshold = 0.8;
    EXPECT_DOUBLE_EQ(params.pi.edge_threshold, 0.8);
    
    // Test adaptive threshold flag
    EXPECT_FALSE(params.pi.adaptive_threshold);
    params.pi.adaptive_threshold = true;
    EXPECT_TRUE(params.pi.adaptive_threshold);
}
