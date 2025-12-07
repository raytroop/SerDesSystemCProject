#include "ams/channel_sparam.h"
#include <cmath>

namespace serdes {

ChannelSParamTdf::ChannelSParamTdf(sc_core::sc_module_name nm, const ChannelParams& params)
    : sca_tdf::sca_module(nm)
    , in("in")
    , out("out")
    , m_params(params)
{
}

void ChannelSParamTdf::set_attributes() {
    in.set_rate(1);
    out.set_rate(1);
}

void ChannelSParamTdf::processing() {
    // 读取输入
    double x_in = in.read();
    
    // 简化通道模型：低通滤波器 + 衰减
    // 衰减转换为线性增益
    double attenuation_linear = std::pow(10.0, -m_params.attenuation_db / 20.0);
    
    // 简单的一阶低通滤波
    static double filter_state = 0.0;
    double alpha = 0.3;  // 简化系数
    filter_state = alpha * x_in + (1.0 - alpha) * filter_state;
    
    // 应用衰减
    double y = attenuation_linear * filter_state;
    
    // 输出
    out.write(y);
}

} // namespace serdes
