# Fabric architecture

The production dependency flow is:

```text
frontend -> FabricRuntime.dll -> production Amber adapter -> external Amber/JPM System 6 DLL
```

`include/fabric/fabric.h` defines the stable, versioned, exception-free C ABI. Runtime and session
handles are opaque; destroy operations are deterministic; snapshot and audio buffers are caller-owned.
The frontend selects `backend_kind = "amber"`, supplies a machine identifier, ROM resources, optional
configuration, and the external DLL's exact absolute path.

Internally, `FabricRuntime` owns backend providers and sessions own backend instances. Hidden C++
composition hooks support tests without exporting implementation symbols. Calls on a session are
serialized, and exceptions are caught before they reach the public DLL boundary. Capability flags
and the neutral machine model cover digital inputs, lamps, reels, character/segment displays, and PCM
audio without exposing backend-native structures.

Only `FabricRuntime` is a production shared-library target. Fake libraries are test fixtures. The
repository contains no emulator core, direct bridge product, plugin discovery, networking, or
proprietary runtime inputs.

## Manual Windows verification

The following checks remain manual and must not be inferred from cross-platform CI:

1. Perform clean Visual Studio 2022/x64 (or CMake x64) Debug and Release builds.
2. Inspect `FabricRuntime.dll` exports and confirm only documented Fabric C exports are public.
3. Launch with the real production Amber DLL and real ROMs.
4. Verify lamps, reels, character/segment displays, audio, switches, reset, stop, and restart.
5. Verify missing-DLL and missing-export diagnostics.
6. Verify unloading and partial-startup cleanup.
7. Confirm no separate bridge DLL or emulator-core DLL is built.

### Amber production families

`amber` dispatches by exact machine identifier: `jpm-system6` selects the System 6
ABI (8,000 cycles/ms), `barcrest-mpu3` selects the directly-addressed MPU3 ABI
(675 cycles/ms, switch-based coins, and no audio), while `barcrest-mpu5` selects the separately typed MPU5 ABI
(16,000 cycles/ms). Provider paths are explicit and absolute; filenames do not affect
selection. Snapshot and audio normalization are shared after platform-specific ABI
calls and output-shape validation.

Amber configuration is likewise dispatched by machine identifier. System 6 consumes
only `FabricAmberSystem6ConfigurationV2`; MPU5 consumes only
`FabricAmberMpu5ConfigurationV1`; either accepts no blob. MPU5 configuration setters
are resolved per requested section and applied before the startup reset and before every explicit
reset. The MPU5 contract includes the source-confirmed general machine options; only
specialist service controls and detailed per-hopper edit-page configuration remain
outside the current public configuration.
