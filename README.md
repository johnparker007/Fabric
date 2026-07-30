# Fabric

Fabric is a standalone native machine runtime with a stable, versioned C ABI. Its sole production
library is `FabricRuntime.dll` on Windows. The current `amber` backend loads the exact absolute path
to an externally supplied production Amber/JPM System 6 DLL and translates its flat C ABI into the
public Fabric model.

This repository does **not** include emulator-core source, proprietary Amber DLLs, or ROM images, and
it does not build a separate bridge DLL.

## Build and test

Use CMake 3.16 or newer and a C++17 compiler (Visual Studio 2022 x64 is the supported Windows toolchain):

```sh
cmake -S . -B out/build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/debug --parallel
ctest --test-dir out/build/debug --output-on-failure
```

Repeat with `out/build/release` and `-DCMAKE_BUILD_TYPE=Release` for an optimized build. The public C
headers are under [`include/fabric/`](include/fabric/). See
[`docs/fabric-architecture.md`](docs/fabric-architecture.md) for runtime boundaries and
[`docs/fabric-amber-backend.md`](docs/fabric-amber-backend.md) for the production adapter contract.
