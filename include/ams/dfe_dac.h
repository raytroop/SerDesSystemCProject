/**
 * @file dfe_dac.h
 * @brief DFE DAC - 将采样器数字输出转换为模拟反馈电压
 * 
 * 在真实电路中，DFE 反馈路径包含一个 DAC：
 * - 输入：数字判决 (0/1)
 * - 输出：模拟电压 (例如 -Vref 到 +Vref)
 * 
 * 这个 DAC 的行为是：
 * - 当输入为 0 时，输出 -voltage_level
 * - 当输入为 1 时，输出 +voltage_level
 * 
 * 注意：DFE Summer 本身已经包含了抽头系数乘法，
 * 所以 DAC 只需要输出固定的电压电平，由 DFE Summer 进行加权求和。
 */

#ifndef SERDES_DFE_DAC_H
#define SERDES_DFE_DAC_H

#include <systemc-ams>

namespace serdes {

/**
 * @class DfeDacTdf
 * @brief DFE DAC TDF 模块
 * 
 * 将采样器的数字输出 (0.0/1.0) 转换为模拟电压：
 * - 0 → -voltage_level (或 0.0，取决于配置)
 * - 1 → +voltage_level
 * 
 * 这个模块模拟真实 DFE 中的 IDAC 或 VDAC。
 */
class DfeDacTdf : public sca_tdf::sca_module {
public:
    // 输入：数字判决 (0.0 或 1.0)
    sca_tdf::sca_in<double> digital_in;
    
    // 输出：模拟电压
    sca_tdf::sca_out<double> analog_out;
    
    /**
     * @brief 构造函数
     * @param nm 模块名称
     * @param voltage_level DAC 输出电压电平 (V)
     * @param map_mode 映射模式: "pm1" (±1) 或 "01" (0/1)
     * 
     * pm1 模式: 0→-V, 1→+V (差分信号)
     * 01 模式: 0→0, 1→V (单端信号)
     */
    DfeDacTdf(sc_core::sc_module_name nm, 
              double voltage_level = 1.0,
              const std::string& map_mode = "pm1")
        : sca_tdf::sca_module(nm)
        , digital_in("digital_in")
        , analog_out("analog_out")
        , m_voltage_level(voltage_level)
        , m_map_mode(map_mode)
    {}
    
    void set_attributes() override {
        digital_in.set_rate(1);
        analog_out.set_rate(1);
    }
    
    void processing() override {
        double digital_val = digital_in.read();
        double analog_val;
        
        if (m_map_mode == "pm1") {
            // ±1 映射：0 → -V, 1 → +V
            analog_val = (digital_val > 0.5) ? m_voltage_level : -m_voltage_level;
        } else {
            // 0/1 映射：0 → 0, 1 → V
            analog_val = (digital_val > 0.5) ? m_voltage_level : 0.0;
        }
        
        analog_out.write(analog_val);
    }

private:
    double m_voltage_level;
    std::string m_map_mode;
};

} // namespace serdes

#endif // SERDES_DFE_DAC_H
