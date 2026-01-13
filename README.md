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

### 🛠️ Compile and Install Your Application

Copy `CMakeLists.User.txt` into your project directory.

For clarity, from your project root:

```bash
mkdir build
cd build
cp $HOME/openfhe-PRNG-Control/CMakeLists.User.txt ../CMakeLists.txt
```
Add your executable target at the end of `CMakeLists.txt`:

```bash
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

The following functionality was introduced to enable deterministic PRNG behavior:

#### New Methods

1. **`SetSeed(uint64_t seed)`**
   - Forces the PRNG to start from an explicit seed.
   - Resets the internal Blake2 engine state (counter, buffer, and buffer index).
   - Useful for guaranteeing deterministic behavior (e.g., reproducible noise in experiments).

2. **`ResetToSeed()`**
   - Restarts the PRNG from the previously set seed.
   - Allows regenerating the exact same sequence of random values.

**Note:** If `SetSeed` is not called, OpenFHE's default (non-deterministic) behavior is preserved.

#### Usage Example
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

#### Implementation Details

The implementation adds virtual methods to the `PRNG` base class and implements them in `Blake2Engine`:

- **Interface declaration:** `src/core/include/utils/prng/prng.h`
  - Adds `virtual void SetSeed(uint64_t seed) = 0;`
  - Adds `virtual void ResetToSeed() = 0;`

- **Implementation:** `src/core/include/utils/prng/blake2engine.h` and `src/core/lib/utils/prng/blake2engine.cpp`
  - Implements both methods to properly reset Blake2 internal state

---
### Secure Decoding Configuration

We added support for additional configuration options in the decoding stage.

This allows:

1. Disabling the exception raised when decrypting a noisy plaintext.
2. Controlling random error injection during decoding to mitigate **Secret Key attacks**.

#### Usage Example

At the application level, **before creating any plaintext or ciphertext**, configure the decoding behavior as follows:
```cpp
auto cfg = SDCConfigHelper::MakeConfig(
    true,                            // enableDetection
    SecretKeyAttackMode::RealOnly,   // attackMode
    4.0                              // thresholdBits
);
SDCConfigHelper::SetGlobalConfig(cfg);
```

**Configuration Parameters:**

- `enableDetection` (bool): When `true`, throws an exception if decryption noise exceeds the threshold. When `false`, allows noisy decryption to proceed.
- `attackMode` (SecretKeyAttackMode): Specifies the type of random error injection to prevent Secret Key attacks.
- `thresholdBits` (double): Noise threshold in bits for triggering detection (default: 5.0).


**`SecretKeyAttackMode`**

The following modes are supported:
```cpp
enum class SecretKeyAttackMode {
    Disabled = 0,           // No error injection
    CompleteInjection = 1,  // Default: inject into both real and imaginary parts
    RealOnly = 2,           // Inject only into real part
    ImaginaryOnly = 3       // Inject only into imaginary part
};
```

Each mode defines where the random error is injected into the plaintext
(real part, imaginary part, or both) during the decoding process
to prevent Secret Key attacks.

**Detection Status**

To check whether a noisy plaintext was detected during decoding:
```cpp
bool detected = SDCConfigHelper::WasSDCDetected(result);
```

This returns `true` if the decryption noise exceeded the configured threshold,
even when `enableDetection` is set to `false` (i.e., when the exception is disabled).

---

#### Implementation Details

The implementation adds new methods to the `PlaintextImpl` base class and helper utilities:

**File:** `src/pke/include/encoding/plaintext.h`

- **`SecretKeyAttackMode` enum**: Defines the different types of error injection for Secret Key attack prevention.

- **`DecodeSDCConfig` struct**: Holds configuration parameters:
  - `enableDetection`: Controls whether to throw exceptions on noisy decryption
  - `secretKeyMode`: Specifies the error injection mode
  - `thresholdBits`: Noise detection threshold

- **`DecodeSDCState` struct**: Stores decoding state:
  - `lastSDCDetected`: Indicates whether noise exceeded threshold in the last decode operation

- **`PlaintextImpl` class additions**:
  - **Private members**:
    - `s_defaultConfig` (static): Global default configuration
    - `m_sdcConfig`: Per-instance configuration
    - `m_sdcState`: Per-instance state
    - `InitSDCConfig()`: Initializes instance config from global default

  - **Public methods**:
    - `SetDecodeSDCConfig()` / `GetDecodeSDCConfig()`: Per-instance configuration getters/setters
    - `GetDecodeSDCState()`: Returns the decoding state for this plaintext
    - `SetDefaultSDCConfig()` / `GetDefaultSDCConfig()`: Static methods for global default configuration
    - `CreateSDCConfig()`: Static factory method to create configuration structs

- **`SDCConfigHelper` class**: Convenience wrapper providing:
  - `SetGlobalConfig()`: Sets the global default configuration used by all new plaintexts
  - `Configure()`: Configures a specific plaintext instance
  - `WasSDCDetected()`: Checks if noise was detected during the last decode
  - `MakeConfig()`: Simplified factory method for creating configurations
  - `GetGlobalConfig()`: Retrieves the current global configuration

**File:** `src/pke/lib/encoding/ckkspackedencoding.cpp`

- **`Decode()` method modifications**:
  - Retrieves SDC configuration from the plaintext instance
  - Checks decryption noise against the configured threshold
  - Updates the `lastSDCDetected` state flag
  - Conditionally throws exception based on `enableDetection` setting
  - Applies random error injection according to the configured `SecretKeyAttackMode`

---

