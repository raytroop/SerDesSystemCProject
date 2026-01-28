#include "ams/diff_to_single.h"

namespace serdes {

DiffToSingleTdf::DiffToSingleTdf(sc_core::sc_module_name nm)
    : sca_tdf::sca_module(nm)
    , in_p("in_p")
    , in_n("in_n")
    , out("out")
{
}

void DiffToSingleTdf::set_attributes() {
    // Use default rate (1) for all ports
    // Timestep will be inherited from connected modules
}

void DiffToSingleTdf::processing() {
    // Convert differential to single-ended: out = in_p - in_n
    out.write(in_p.read() - in_n.read());
}

} // namespace serdes
