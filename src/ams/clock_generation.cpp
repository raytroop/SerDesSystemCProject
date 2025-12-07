#include "ams/clock_generation.h"
#include <cmath>

namespace serdes {

ClockGenerationTdf::ClockGenerationTdf(sc_core::sc_module_name nm, const ClockParams& params)
    : sca_tdf::sca_module(nm)
    , clk_phase("clk_phase")
    , m_params(params)
    , m_phase(0.0)
    , m_frequency(params.frequency)
{
}

void ClockGenerationTdf::set_attributes() {
    clk_phase.set_rate(1);
    clk_phase.set_timestep(1.0 / (m_frequency * 100.0), sc_core::SC_SEC);
}

void ClockGenerationTdf::processing() {
    // 理想时钟生成：直接输出相位值
    clk_phase.write(m_phase);
    
    // 更新相位
    m_phase += 2.0 * M_PI * m_frequency * clk_phase.get_timestep().to_seconds();
    if (m_phase >= 2.0 * M_PI) {
        m_phase -= 2.0 * M_PI;
    }
}

} // namespace serdes
