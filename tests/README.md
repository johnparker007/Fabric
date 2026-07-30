# Tests

Generic runtime tests use the internal fake backend. `AmberProductionBackendTests` and
`AmberProductionAudioTests` load test-only `FakeProductionAmber` variants, which consume the same
private packed ABI declaration as the production adapter. They cover lifecycle, ROM/configuration
translation, fixed-tick execution, snapshots, inputs, diagnostics, audio accounting, partial reads,
missing exports, repeated creation, and unload/reload without proprietary files.

The C and C++ header tests validate the public contract independently of a production backend.
