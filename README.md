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


### 🐞 Debug Build

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
      -DCMAKE_BUILD_TYPE=Debug -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF -DCMAKE_CXX_FLAGS="-g -O0" ..
```

### 🚀 Release Build (for campaigns)

```bash
cmake -DCMAKE_INSTALL_PREFIX=$HOME/openfhe-PRNG-Control/install -DBUILD_STATIC=OFF -DBUILD_SHARED=ON \
      -DCMAKE_BUILD_TYPE=Release -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF ..
```

### 🛠️ Compile and Install

```bash
make -j16
sudo make install
```



---

## ✨ Changes in this Fork

We added **deterministic PRNG control** by modifying `blake2engine.cpp`
→ Ensure the `FIXED_SEED` macro is **uncommented** in:

```
src/core/lib/utils/prng/blake2engine.cpp
```

1. **`SetSeed(uint64_t seed)`**
   - Forces the PRNG to start from the exact given seed.
   - Resets the internal Blake2Engine (`m_counter = 0`).
   - Use it to guarantee deterministic outputs (e.g., reproducible noise).

2. **`ResetToSeed()`**
    - Reestart the same seed.
