# OpenFHE PRNG-Controlled Fork

This is a fork of the OpenFHE development branch.
It includes **minor changes to gain fine control over the PRNG engine**, using a **fixed seed by default**.
⚠️ **Not suitable for real-world use** — intended **only for research and experimentation**.


---

## 🔧 Installation

We recommend installing to a dedicated directory using `CMAKE_INSTALL_PREFIX`.
If you don’t specify it, OpenFHE installs system-wide.

```bash
mkdir build
cd build
```


### 🚀 Release Build (for campaigns)

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
      -DCMAKE_BUILD_TYPE=Release -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF ..
```

### 🐞 Debug Build

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
                             -DCMAKE_BUILD_TYPE=Debug -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
                             -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF -DCMAKE_CXX_FLAGS="-g -O0" ..

```

### 🛠️ Compile and Install

```bash
make -j$(nproc)
sudo make install
```

---
### Testing

To verify that works, run the example test_PRNG:


```bash
./build/bin/examples/test_PRNG
```
---
### 🐞 Debug

To verify that works, run the example test_PRNG:


```bash
./build/bin/examples/test_PRNG
```


---

## ✨ Changes in This Fork

### Deterministic PRNG Control

We added **deterministic PRNG control** by modifying `blake2engine.cpp`.

It is **not necessary** to uncomment the `FIXED_SEED` macro:


```
src/core/lib/utils/prng/blake2engine.cpp
```



The following functionality was introduced:

1. **`SetSeed(uint64_t seed)`**
   - Forces the PRNG to start from an explicit seed.
   - Resets the internal Blake2 engine state (`m_counter = 0`).
   - Useful for guaranteeing deterministic behavior (e.g., reproducible noise).

2. **`ResetToSeed()`**
   - Restarts the PRNG from the previously set seed.

If `SetSeed` is not used, OpenFHE’s default (non-deterministic) behavior is preserved.

---

### Secure Decoding Configuration

We added support for additional configuration options in the decoding stage.

This allows:

1. Disabling the exception raised when decrypting a noisy plaintext.
2. Controlling random error injection during decoding to mitigate **Secret Key attacks**.

#### Usage

At the application level, **before creating any plaintext or ciphertext**, configure the decoding behavior as follows:

```cpp
auto cfg = SDCConfigHelper::MakeConfig(
    true,                           // enableDetection
    SecretKeyAttackMode::RealOnly,   // attackMode
    4.0                              // thresholdBits
);
SDCConfigHelper::SetGlobalConfig(cfg);
```

** `SecretKeyAttackMode`**

The following modes are supported:

```cpp
enum class SecretKeyAttackMode {
    Disabled = 0,
    CompleteInjection = 1, // Default
    RealOnly = 2,
    ImaginaryOnly = 3
};
```
Each mode defines where the random error is injected into the plaintext
(real part, imaginary part, or both) before completing the decoding process,
in order to prevent Secret Key attacks.

**Detection Status**

To check whether a noisy plaintext was detected during decoding:
```cpp
bool detected = SDCConfigHelper::WasSDCDetected(result);
```


