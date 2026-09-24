#ifndef LBCRYPTO_FAULT_HOOK_H
#define LBCRYPTO_FAULT_HOOK_H

// Fault-injection sites inside CKKS EvalAdd / EvalMult (ciphertext x ciphertext).
//
// The library never flips anything by itself: an external framework arms ONE site
// with a callback, calls the normal cc->EvalAdd / cc->EvalMult, and the callback
// receives the polynomial that lives at that point of the computation. The site is
// one-shot: it disarms itself right before invoking the callback.
//
// When nothing is armed every site costs one comparison.
// Not thread-safe: arm/disarm from the thread that calls the operation.

#include "lattice/lat-hal.h"

#include <cstdint>
#include <functional>

namespace lbcrypto {
namespace fi {

enum class Op : uint8_t { None, Add, Mult };

// op_step numbering. c0 = b (the part that carries the message), c1 = a.
// It follows HEAAN's addBitFlip for Add. Mult cannot mirror HEAAN step by step
// because OpenFHE uses a schoolbook tensor product and hybrid key switching.
enum AddStep : uint32_t {
    ADD_IN1_C1 = 0,  // first operand, c1
    ADD_IN2_C1 = 1,  // second operand, c1
    ADD_IN1_C0 = 2,  // first operand, c0
    ADD_IN2_C0 = 3,  // second operand, c0
    ADD_OUT_C1 = 4,  // result, c1
    ADD_OUT_C0 = 5,  // result, c0
    ADD_NUM_STEPS
};

enum MultStep : uint32_t {
    MULT_IN1_C0  = 0,   // first operand, c0 (after level alignment)
    MULT_IN1_C1  = 1,   // first operand, c1
    MULT_IN2_C0  = 2,   // second operand, c0
    MULT_IN2_C1  = 3,   // second operand, c1
    MULT_D0      = 4,   // tensor product d0 = c0*c0'
    MULT_D1      = 5,   // tensor product d1 = c0*c1' + c1*c0'
    MULT_D2      = 6,   // tensor product d2 = c1*c1' (input of key switching)
    MULT_KS0     = 7,   // key-switching output added to d0
    MULT_KS1     = 8,   // key-switching output added to d1
    MULT_OUT_C0  = 9,   // relinearized result, c0 (before rescale)
    MULT_OUT_C1  = 10,  // relinearized result, c1
    MULT_NUM_STEPS
};

using FlipFn = std::function<void(DCRTPoly&)>;

// Arms a single site. Replaces whatever was armed before.
void Arm(Op op, uint32_t step, FlipFn fn);
// Clears the armed site (no-op if it already fired).
void Disarm();
// Called by the library at every site.
void FlipIfStep(Op op, uint32_t step, DCRTPoly& poly);

}  // namespace fi
}  // namespace lbcrypto

#endif  // LBCRYPTO_FAULT_HOOK_H
