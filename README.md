# OpenFHE PRNG-Controlled Fork

This is a fork of the OpenFHE development branch for **fault-injection research on CKKS**.
It adds three things on top of upstream:

1. **Deterministic PRNG control**: fixed, resettable seed for reproducible ciphertexts.
2. **Secure decoding configuration**: the "approximation error too high" exception can be turned off, and the result tells you whether the noise threshold was exceeded.
3. **Fault-injection hooks** inside CKKS `EvalAdd` / `EvalMult`: an external tool can flip bits in the internal polynomials of an operation without copying any OpenFHE function.

⚠️ **Not suitable for real-world use**. It is intended **only for research and experimentation**.

It is the OpenFHE backend of [fi-ckks](https://github.com/mmazz/fi-ckks).

---

## 🔧 Installation

### ⚡ Quick build

```bash
make          # cmake (Release, shared, no OpenMP) + make + make install into ./install
make clean    # removes build/ and install/
```

This is what fi-ckks uses (`third_party/setup_thirdParty.sh`).

### Manual build

We recommend installing to a dedicated directory using `CMAKE_INSTALL_PREFIX`.
If you don't specify it, OpenFHE installs system-wide.

```bash
mkdir build
cd build
```

#### 🚀 Release build (for campaigns)

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
      -DCMAKE_BUILD_TYPE=Release -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF ..
```

#### 🐞 Debug build

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
      -DCMAKE_BUILD_TYPE=Debug -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF -DCMAKE_CXX_FLAGS="-g -O0 -fno-inline" ..
```

#### 🛠️ Compile and install

```bash
make -j$(nproc)
make install        # use sudo only for a system-wide install (no CMAKE_INSTALL_PREFIX)
```

`WITH_OPENMP=OFF` is intentional: the fault-injection hooks are not thread-safe (see below).

### 🛠️ Compile and install your application

Copy `CMakeLists.User.txt` into your project directory.

For clarity, from your project root:

```bash
mkdir build
cd build
cp $HOME/openfhe-PRNG-Control/CMakeLists.User.txt ../CMakeLists.txt
```

Add your executable target at the end of `CMakeLists.txt`:

```cmake
add_executable(test src/demo-simple-example.cpp)
```

Then configure and build the project:

```bash
cmake -DCMAKE_PREFIX_PATH=$HOME/openfhe-PRNG-Control/install/lib/OpenFHE \
      -DBUILD_STATIC=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-g -O3" ..
make -j$(nproc)
```

---

### ✅ Testing

To check that the PRNG control works, run the example `test_PRNG`:

```bash
./build/bin/examples/test_PRNG
```

The fault-injection hooks are tested from fi-ckks (`fi_test_openfhe` and `tests/regress.sh`). Those tests reach all 17 sites, check that each flip changes exactly one coefficient, and check that a clean run afterwards matches the golden output bit for bit.

---

## ✨ Changes in This Fork

### 1. Deterministic PRNG Control

We added **deterministic PRNG control** by modifying `blake2engine.cpp`.

It is **not necessary** to uncomment the `FIXED_SEED` macro in:

```
src/core/lib/utils/prng/blake2engine.cpp
```

#### New methods

1. **`SetSeed(uint64_t seed)`**
   - Forces the PRNG to start from an explicit seed.
   - Resets the internal Blake2 engine state (counter, buffer and buffer index).
   - Useful for guaranteeing deterministic behavior (e.g., reproducible noise in experiments).

2. **`ResetToSeed()`**
   - Restarts the PRNG from the previously set seed.
   - Allows regenerating the exact same sequence of random values.

**Note:** If `SetSeed` is not called, OpenFHE's default (non-deterministic) behavior is preserved.

#### Usage example

```cpp
PRNG& prng = PseudoRandomNumberGenerator::GetPRNG();

// Set a specific seed for reproducibility
prng.SetSeed(42);
auto c1 = cc->Encrypt(key, plaintext);

// Reset to reproduce the same random values
prng.ResetToSeed();
auto c2 = cc->Encrypt(key, plaintext);
// c1 and c2 will have identical ciphertexts
```

#### Implementation details

The implementation adds virtual methods to the `PRNG` base class and implements them in `Blake2Engine`:

- **Interface declaration:** `src/core/include/utils/prng/prng.h`
  - Adds `virtual void SetSeed(uint64_t seed) = 0;`
  - Adds `virtual void ResetToSeed() = 0;`

- **Implementation:** `src/core/include/utils/prng/blake2engine.h` and `src/core/lib/utils/prng/blake2engine.cpp`
  - Implements both methods to properly reset the Blake2 internal state.

---

### 2. Secure Decoding Configuration

We added support for additional configuration options in the decoding stage.

This allows:

1. Disabling the exception raised when decrypting a noisy plaintext.
2. Controlling random error injection during decoding to mitigate **Secret Key attacks**.

#### Usage example

At the application level, **before creating any plaintext or ciphertext**, configure the decoding behavior as follows:

```cpp
auto cfg = SDCConfigHelper::MakeConfig(
    true,                            // enableDetection
    SecretKeyAttackMode::RealOnly,   // attackMode
    4.0                              // thresholdBits
);
SDCConfigHelper::SetGlobalConfig(cfg);
```

**Configuration parameters:**

- `enableDetection` (bool): When `true`, throws an exception if decryption noise exceeds the threshold. When `false`, allows noisy decryption to proceed.
- `attackMode` (SecretKeyAttackMode): Specifies the type of random error injection used to prevent Secret Key attacks.
- `thresholdBits` (double): Noise threshold in bits for triggering detection (default: 5.0).

**`SecretKeyAttackMode`**

The following modes are supported:

```cpp
enum class SecretKeyAttackMode {
    Disabled = 0,           // No error injection
    CompleteInjection = 1,  // Default: inject into both real and imaginary parts
    RealOnly = 2,           // Inject only into the real part
    ImaginaryOnly = 3       // Inject only into the imaginary part
};
```

Each mode defines where the random error is injected into the plaintext during decoding (real part, imaginary part, or both) to prevent Secret Key attacks.

**Detection status**

To check whether a noisy plaintext was detected during decoding:

```cpp
bool detected = SDCConfigHelper::WasSDCDetected(result);
```

This returns `true` if the decryption noise exceeded the configured threshold, even when `enableDetection` is `false` (i.e., when the exception is disabled).

#### Implementation details

The implementation adds new methods to the `PlaintextImpl` base class and helper utilities.

**File:** `src/pke/include/encoding/plaintext.h`

- **`SecretKeyAttackMode` enum**: Defines the different types of error injection for Secret Key attack prevention.

- **`DecodeSDCConfig` struct**: Holds the configuration parameters:
  - `enableDetection`: Controls whether to throw exceptions on noisy decryption.
  - `secretKeyMode`: Specifies the error injection mode.
  - `thresholdBits`: Noise detection threshold.

- **`DecodeSDCState` struct**: Stores the decoding state:
  - `lastSDCDetected`: Whether the noise exceeded the threshold in the last decode operation.

- **`PlaintextImpl` class additions**:
  - **Private members**:
    - `s_defaultConfig` (static): Global default configuration.
    - `m_sdcConfig`: Per-instance configuration.
    - `m_sdcState`: Per-instance state.
    - `InitSDCConfig()`: Initializes the instance config from the global default.
  - **Public methods**:
    - `SetDecodeSDCConfig()` / `GetDecodeSDCConfig()`: Per-instance configuration getters/setters.
    - `GetDecodeSDCState()`: Returns the decoding state for this plaintext.
    - `SetDefaultSDCConfig()` / `GetDefaultSDCConfig()`: Static methods for the global default configuration.
    - `CreateSDCConfig()`: Static factory method to create configuration structs.

- **`SDCConfigHelper` class**: Convenience wrapper providing:
  - `SetGlobalConfig()`: Sets the global default configuration used by all new plaintexts.
  - `Configure()`: Configures a specific plaintext instance.
  - `WasSDCDetected()`: Checks if noise was detected during the last decode.
  - `MakeConfig()`: Simplified factory method for creating configurations.
  - `GetGlobalConfig()`: Retrieves the current global configuration.

**File:** `src/pke/lib/encoding/ckkspackedencoding.cpp`

- **`Decode()` method modifications**:
  - Retrieves the SDC configuration from the plaintext instance.
  - Checks decryption noise against the configured threshold.
  - Updates the `lastSDCDetected` state flag.
  - Conditionally throws an exception based on `enableDetection`.
  - Applies random error injection according to the configured `SecretKeyAttackMode`.

---

### 3. Fault-Injection Hooks (CKKS `EvalAdd` / `EvalMult`)

Each injection point (a *site*) inside the homomorphic operations is a call to
`fi::FlipIfStep(op, step, poly)`. The library never flips anything by itself. An external
tool **arms one site** with a callback, calls the normal `cc->EvalAdd` / `cc->EvalMult`,
and the callback receives the polynomial (`DCRTPoly`) that lives at that point of the
computation. The callback decides which limb, coefficient and bits to flip.

There are no API changes and no duplicated `EvalMultBitFlip`-style functions: the normal
call path goes through `CryptoContextImpl` → `SchemeBase` → `LeveledSHEBase` →
`LeveledSHERNS` and is left as it is.

#### API (`src/pke/include/fault-hook.h`)

```cpp
namespace lbcrypto::fi {
    enum class Op : uint8_t { None, Add, Mult };
    using FlipFn = std::function<void(DCRTPoly&)>;

    void Arm(Op op, uint32_t step, FlipFn fn);   // arms ONE site, replaces the previous one
    void Disarm();                               // no-op if the site already fired
    void FlipIfStep(Op op, uint32_t step, DCRTPoly& poly);   // called by the library
}
```

#### Usage example

```cpp
#include "openfhe.h"
#include "fault-hook.h"

// Flip bit 30 of coefficient 5, limb 0, of d2 = c1*c1' inside the next EvalMult.
fi::Arm(fi::Op::Mult, fi::MULT_D2, [](DCRTPoly& p) {
    const Format orig = p.GetFormat();
    p.SetFormat(Format::COEFFICIENT);            // or flip directly in EVALUATION (NTT) form
    auto& limb = p.GetAllElements()[0];
    limb[5] = NativeInteger(limb[5].ConvertToInt() ^ (uint64_t(1) << 30));
    p.SetFormat(orig);                           // leave it in the format it arrived in
});
auto faulty = cc->EvalMult(ct1, ct2);   // the flip happens here, exactly once
fi::Disarm();                           // in case the site was never reached

auto clean = cc->EvalMult(ct1, ct2);    // ct1 and ct2 were not modified
```

The callback writes the 64-bit word as it is: a flip above the limb's modulus leaves a
residue `>= q_j`, as it would in hardware. If you want to discard or flag those cases,
the callback has to check it (fi-ckks marks them as `out_of_range`).

#### Sites (`op_step`)

`c0` is the part that carries the message (`b`), `c1` is `a` (the same convention as HEAAN's `bx` / `ax`).

**`Op::Add`**, in `LeveledSHERNS::EvalAddInPlace` (same numbering as HEAAN's `addBitFlip`):

| step | enum          | polynomial          |
|-----:|---------------|---------------------|
| 0    | `ADD_IN1_C1`  | first operand, c1   |
| 1    | `ADD_IN2_C1`  | second operand, c1  |
| 2    | `ADD_IN1_C0`  | first operand, c0   |
| 3    | `ADD_IN2_C0`  | second operand, c0  |
| 4    | `ADD_OUT_C1`  | result, c1          |
| 5    | `ADD_OUT_C0`  | result, c0          |

**`Op::Mult`**: steps 0–3 are in `LeveledSHERNS::EvalMult` (after level alignment), and steps 4–10 are in
`LeveledSHEBase::EvalMult(ct, ct, evalKey)` (tensor product and relinearization):

| step | enum           | polynomial                                        |
|-----:|----------------|---------------------------------------------------|
| 0    | `MULT_IN1_C0`  | first operand, c0                                 |
| 1    | `MULT_IN1_C1`  | first operand, c1                                 |
| 2    | `MULT_IN2_C0`  | second operand, c0                                |
| 3    | `MULT_IN2_C1`  | second operand, c1                                |
| 4    | `MULT_D0`      | d0 = c0·c0'                                       |
| 5    | `MULT_D1`      | d1 = c0·c1' + c1·c0'                              |
| 6    | `MULT_D2`      | d2 = c1·c1' (input of key switching)              |
| 7    | `MULT_KS0`     | key-switching output added to d0                  |
| 8    | `MULT_KS1`     | key-switching output added to d1                  |
| 9    | `MULT_OUT_C0`  | relinearized result, c0 (before rescale)          |
| 10   | `MULT_OUT_C1`  | relinearized result, c1 (before rescale)          |

The Mult numbering does **not** match HEAAN's `multBitFlip` (0..25): OpenFHE computes the
tensor product term by term instead of with HEAAN's `(a1+b1)(a2+b2)` trick, and uses
hybrid key switching. A flip in an input operand (steps 0–3) is seen by every product
that reads that operand (e.g. step 0 affects d0 and d1).

#### Guarantees

- **One-shot.** The site disarms itself *before* calling the callback, so a single `Arm` can never flip twice.
- **Transient.** In CKKS, `LeveledSHERNS::EvalMult` and `EvalAddInPlace` clone the operands before the core, so input flips land on private clones and the caller's ciphertexts are never modified. The only exception is `cc->EvalAddInPlace(ct1, ct2)`: `ct1` is the output register there, so `ADD_IN1_*` modifies it, which is expected.
- **Zero cost when disarmed.** Each site costs one comparison.
- **Not thread-safe.** The armed site is a global variable. Arm, call and disarm from the same thread, and build with `WITH_OPENMP=OFF`. None of the sites is inside a parallel region.

#### Scope

- Designed and tested for CKKS only (`FIXEDMANUAL`, `FIXEDAUTO`, `FLEXIBLEAUTO`, ...). Under `NORESCALE` the Add sites and Mult steps 0–3 are skipped.
- Instrumented calls: `cc->EvalAdd(ct, ct)`, `cc->EvalAddInPlace(ct, ct)` and `cc->EvalMult(ct, ct)`. `cc->EvalMultNoRelin` only reaches Mult steps 0–3.
- Not instrumented: `EvalMultInPlace`, `EvalMultMutable`, `EvalSquare`, plaintext and scalar operations, `Rescale`, rotations and bootstrapping. If a site is armed and never reached, nothing happens; the caller should check that its callback ran.

#### Implementation details

- **New:** `src/pke/include/fault-hook.h` (API and site numbering) and `src/pke/lib/fault-hook.cpp` (the armed site state). The `.cpp` is picked up by the `GLOB_RECURSE` in `src/pke/CMakeLists.txt` and the header is installed with the rest of `pke/include`.
- **Modified:** `src/pke/lib/schemerns/rns-leveledshe.cpp`, which has the sites in `EvalAddInPlace` and in `EvalMult(ct, ct)` (inputs).
- **Modified:** `src/pke/lib/schemebase/base-leveledshe.cpp`, which has the sites in `EvalMult(ct, ct, evalKey)` (tensor product, key switching and output).
