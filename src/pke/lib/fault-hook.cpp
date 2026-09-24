#include "fault-hook.h"

#include <utility>

namespace lbcrypto {
namespace fi {

namespace {
Op g_op        = Op::None;
uint32_t g_step = 0;
FlipFn g_fn;
}  // namespace

void Arm(Op op, uint32_t step, FlipFn fn) {
    g_op   = op;
    g_step = step;
    g_fn   = std::move(fn);
}

void Disarm() {
    g_op = Op::None;
    g_fn = nullptr;
}

void FlipIfStep(Op op, uint32_t step, DCRTPoly& poly) {
    if (g_op != op || g_step != step)
        return;
    // One-shot: disarm BEFORE calling, so a site reached twice can never flip twice.
    FlipFn fn = std::move(g_fn);
    Disarm();
    if (fn)
        fn(poly);
}

}  // namespace fi
}  // namespace lbcrypto
