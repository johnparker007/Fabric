# Production Amber backend

The `amber` backend loads only the exact absolute DLL path supplied by the frontend. It neither searches
for a DLL nor substitutes a filename. The supplied library must implement the production flat/singleton
Amber/JPM System 6 C ABI. Required exports are resolved eagerly; an error identifies both a missing
symbol and the requested DLL. Optional exports enable sound and machine configuration features.

Private structure declarations for this external binary contract live in
`src/Backends/Amber/ProductionAmberAbi.h`. They preserve four-byte packing and compile-time
size/alignment checks but are not public Fabric headers or emulator source. The adapter owns the module,
releases asserted inputs, shuts down partial or complete sessions, and unloads deterministically.

Startup calls `Initialise`, loads program and optional sound ROMs, then uses the normal reset path.
Configuration is applied after that reset and every later reset. Execution accumulates nanoseconds into
1 ms ticks, calls `Run(8000)` for each complete tick, performs at most three catch-up ticks, and retains
only the sub-millisecond remainder. The native `Run` result is observational, not a progress count.

Snapshots are copied from the packed production structure into caller-owned Fabric lamps, reels,
character displays, and segment displays. Audio supports the production PCM16 mono/stereo formats.
Each executed tick earns `sample_rate / 1000` frames using a fractional accumulator; reads are bounded
by earned frames, partial reads retain unused entitlement, and reset/shutdown discard it. Diagnostics
are bounded and use the launch callback, with `FABRIC_AMBER_TRACE=1` as an opt-in file/debugger fallback.

Automated tests use `FakeProductionAmber`, which includes the same private ABI declaration. No
proprietary DLL or ROM is needed. Real-DLL validation remains a manual Windows integration check as
listed in `fabric-architecture.md`.
