# Testbench CMakeLists.txt

# Helper function to create testbench executables
function(add_serdes_testbench target_name source_file)
    add_executable(${target_name} ${source_file})
    
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(${target_name}
        serdes_lib
        SystemC::systemc
        SystemC::systemc-ams
    )
    
    # Set output directory
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endfunction()

# ============================================================================
# Simple link testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/simple_link_tb.cpp)
    add_serdes_testbench(simple_link_tb simple_link_tb.cpp)
endif()

# ============================================================================
# Full system testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/full_system_tb.cpp)
    add_serdes_testbench(full_system_tb full_system_tb.cpp)
endif()

# ============================================================================
# CTLE transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rx/ctle/ctle_tran_tb.cpp)
    add_serdes_testbench(ctle_tran_tb rx/ctle/ctle_tran_tb.cpp)
endif()

# ============================================================================
# VGA transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rx/vga/vga_tran_tb.cpp)
    add_serdes_testbench(vga_tran_tb rx/vga/vga_tran_tb.cpp)
endif()

# ============================================================================
# Sampler transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rx/sampler/sampler_tran_tb.cpp)
    add_serdes_testbench(sampler_tran_tb rx/sampler/sampler_tran_tb.cpp)
endif()

# ============================================================================
# CDR transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rx/cdr/cdr_tran_tb.cpp)
    add_serdes_testbench(cdr_tran_tb rx/cdr/cdr_tran_tb.cpp)
endif()

# ============================================================================
# Wave Generation transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/periphery/wave_gen/wave_gen_tran_tb.cpp)
    add_serdes_testbench(wave_gen_tran_tb periphery/wave_gen/wave_gen_tran_tb.cpp)
endif()

# ============================================================================
# TX Top transient testbench
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tx/tx_tran_tb.cpp)
    add_serdes_testbench(tx_tran_tb tx/tx_tran_tb.cpp)
endif()

# ============================================================================
# RX Link transient testbench (complete RX chain)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rx/rx_link_tb.cpp)
    add_serdes_testbench(rx_link_tb rx/rx_link_tb.cpp)
endif()

# ============================================================================
# SerDes Link testbench (full link with ClockGen)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/top/serdes_link_tb.cpp)
    add_serdes_testbench(serdes_link_tb top/serdes_link_tb.cpp)
endif()
