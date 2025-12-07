#include <gtest/gtest.h>
#include <systemc-ams>

// SystemC main function (required for SystemC programs)
int sc_main(int argc, char **argv) {
    // 初始化SystemC - 禁止警告
    sc_core::sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", sc_core::SC_DO_NOTHING);
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
