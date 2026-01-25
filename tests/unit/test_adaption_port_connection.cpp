/**
 * @file test_adaption_port_connection.cpp
 * @brief Unit test for AdaptionDe - Port Connection Verification
 */

#include "adaption_test_common.h"

using namespace serdes;
using namespace serdes::test;

TEST(AdaptionBasicTest, PortConnection) {
    AdaptionParams params;
    AdaptionBasicTestbench* tb = new AdaptionBasicTestbench("tb", params);
    
    // Run short simulation
    sc_core::sc_start(100, sc_core::SC_NS);
    
    // Verify port connections are working (no crash)
    SUCCEED() << "Port connection test passed";
    
    // Clean up: stop and delete
    delete tb;
}
