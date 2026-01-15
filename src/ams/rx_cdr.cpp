#include "ams/rx_cdr.h"
#include <cmath>

namespace serdes {

RxCdrTdf::RxCdrTdf(sc_core::sc_module_name nm, const CdrParams& params)
    : sca_tdf::sca_module(nm)
    , in("in")
    , phase_out("phase_out")
    , m_params(params)
    , m_phase(0.0)
    , m_prev_bit(0.0)
    , m_integral(0.0)
{
}

void RxCdrTdf::set_attributes() {
    in.set_rate(1);
    phase_out.set_rate(1);
}

void RxCdrTdf::processing() {
    // 读取输入
    double current_bit = in.read();

    // Bang-Bang相位检测（简化实现）
    double phase_error = 0.0;
    if (std::abs(current_bit - m_prev_bit) > 0.5) {  // 检测到边沿
        if (current_bit > m_prev_bit) {
            phase_error = 1.0;  // 时钟晚，需要提前
        } else {
            phase_error = -1.0;  // 时钟早，需要延后
        }
    }

    // PI控制器（正确实现：分离比例项和积分项）
    // 比例项：瞬时响应
    double prop_term = m_params.pi.kp * phase_error;
    // 积分项：累积历史误差
    m_integral += m_params.pi.ki * phase_error;
    // 总输出
    m_phase = prop_term + m_integral;

    // 限制相位范围
    if (m_phase > m_params.pai.range) {
        m_phase = m_params.pai.range;
    } else if (m_phase < -m_params.pai.range) {
        m_phase = -m_params.pai.range;
    }

    // 相位量化到分辨率
    double quantized_phase = std::round(m_phase / m_params.pai.resolution) * m_params.pai.resolution;

    m_prev_bit = current_bit;

    // 输出相位调整量（单位：秒）
    phase_out.write(quantized_phase);
}

} // namespace serdes
