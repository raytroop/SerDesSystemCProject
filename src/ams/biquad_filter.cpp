#include "ams/biquad_filter.h"
#include <cmath>
#include <complex>

namespace serdes {

BiquadSection::BiquadSection()
    : m_b0(0.0), m_b1(0.0), m_b2(0.0)
    , m_a1(0.0), m_a2(0.0)
    , m_b0d(0.0), m_b1d(0.0), m_b2d(0.0)
    , m_a1d(0.0), m_a2d(0.0)
    , m_x1(0.0), m_x2(0.0)
    , m_y1(0.0), m_y2(0.0)
    , m_timestep(1.0)
    , m_initialized(false)
{
}

void BiquadSection::initialize(double b0, double b1, double b2, double a1, double a2, double timestep) {
    // Store continuous-time coefficients
    m_b0 = b0;
    m_b1 = b1;
    m_b2 = b2;
    m_a1 = a1;
    m_a2 = a2;
    m_timestep = timestep;
    
    // Bilinear transform: s = (2/T) * (z-1)/(z+1) = K * (z-1)/(z+1)
    // where K = 2/T
    double K = 2.0 / timestep;
    double K2 = K * K;
    
    // Transform numerator: b0*s^2 + b1*s + b2
    // s = K*(z-1)/(z+1)
    // s^2 = K^2*(z-1)^2/(z+1)^2 = K^2*(z^2 - 2z + 1)/(z^2 + 2z + 1)
    //
    // N(s) = b0*K^2*(z^2-2z+1)/(z+1)^2 + b1*K*(z-1)/(z+1) + b2
    // Multiply by (z+1)^2:
    // N(z) = b0*K^2*(z^2-2z+1) + b1*K*(z-1)*(z+1) + b2*(z+1)^2
    //      = b0*K^2*(z^2-2z+1) + b1*K*(z^2-1) + b2*(z^2+2z+1)
    //
    // Coefficients of z^2, z^1, z^0:
    double nb2 = b0 * K2 - b1 * K + b2;         // z^0 coefficient (after multiplying by z^-2)
    double nb1 = -2.0 * b0 * K2 + 2.0 * b2;      // z^-1 coefficient  
    double nb0 = b0 * K2 + b1 * K + b2;         // z^-2 coefficient
    
    // Transform denominator: s^2 + a1*s + a2
    // Similar calculation:
    double da2 = K2 - a1 * K + a2;              // z^0 coefficient
    double da1 = -2.0 * K2 + 2.0 * a2;           // z^-1 coefficient
    double da0 = K2 + a1 * K + a2;              // z^-2 coefficient
    
    // Normalize by da2 to make denominator start with 1
    if (std::abs(da2) > 1e-15) {
        m_b0d = nb0 / da2;
        m_b1d = nb1 / da2;
        m_b2d = nb2 / da2;
        m_a1d = da1 / da2;
        m_a2d = da0 / da2;
    } else {
        // Degenerate case - fallback to simple gain
        m_b0d = 0.0;
        m_b1d = 0.0;
        m_b2d = b2 / a2;
        m_a1d = 0.0;
        m_a2d = 0.0;
    }
    
    // Reset state
    reset();
    
    m_initialized = true;
}

double BiquadSection::process(double input) {
    if (!m_initialized) {
        return input;
    }
    
    // Direct Form II implementation:
    // w[n] = x[n] - a1d*w[n-1] - a2d*w[n-2]
    // y[n] = b0d*w[n] + b1d*w[n-1] + b2d*w[n-2]
    //
    // Using state variables:
    // m_x1 = w[n-1], m_x2 = w[n-2]
    // After computing w[n], shift: w[n-2] = w[n-1], w[n-1] = w[n]
    
    double w = input - m_a1d * m_x1 - m_a2d * m_x2;
    double output = m_b0d * w + m_b1d * m_x1 + m_b2d * m_x2;
    
    // Shift states
    m_x2 = m_x1;
    m_x1 = w;
    
    return output;
}

void BiquadSection::reset() {
    m_x1 = 0.0;
    m_x2 = 0.0;
    m_y1 = 0.0;
    m_y2 = 0.0;
}

} // namespace serdes
