# Production Amber backend

The `amber` backend loads only the exact absolute DLL path supplied by the frontend. It neither searches
for a DLL nor substitutes a filename. The supplied library must implement the production flat/singleton
Amber/JPM System 6 C ABI. Required exports are resolved eagerly; an error identifies both a missing
symbol and the requested DLL. Optional exports enable sound and machine configuration features.

## Inputs and electronic coin mechanism

Digital matrix, service, and door inputs use `TurnSwitchOn(index)` and
`TurnSwitchOff(index)`. Coin actions are a different Fabric input kind and carry both a channel
(0 through 5) and denomination code (0 through 12); the adapter calls `CoinIn(channel, value)` only
on the inactive-to-active edge. Amber's source-confirmed parallel mechanism converts an accepted
coin into its internally generated switch pulse 72 through 77. Fabric and frontends must not assert
those switches to insert coins. A zero return from `CoinIn` becomes `FABRIC_INPUT_REJECTED`, which is
a handled rejection rather than a transport or session failure.

The current Amber machine configuration is version 2. After the startup reset and every explicit
reset, Fabric applies mechanism settings in this order: `SetCommStyle`, `SetCommInvert`, `SetCycles`,
and `SetEDCEnable`; it then applies `SetCoinEnable`, `SetCoinValue`, and `SetLockoutInvert` for selected
channels. Parallel communication style is raw value `0`, pulse cycles must be non-zero, and the known
working setup uses style 0, no inversion, 800000 cycles, and EDC disabled. `SetCoinValue` remains part
of channel configuration, but each `CoinIn` request still supplies its explicit denomination code.

`SetLockoutVal(index, data)` is a live output-port bitfield update made inside Amber when the emulated
DUART output changes; it is not per-channel static configuration and Fabric never calls it during
startup/reset configuration. Amber's separate `SetLockoutDrive` method is not exported by the current
DLL. Fabric therefore does not attempt to configure distinct frontend #1/#2 lockout-drive assignments,
and the coin channel's final public word remains reserved.

The production adapter requires `CoinIn`, `SetCommStyle`, `SetCommInvert`, `SetCycles`, `SetEDCEnable`,
`SetCoinEnable`, `SetCoinValue`, and `SetLockoutInvert`, in addition to its lifecycle, ROM, snapshot,
and digital-input exports. No Amber DLL change is required.

Private structure declarations for this external binary contract live in
`src/Backends/Amber/ProductionAmberAbi.h`. They preserve four-byte packing and compile-time
size/alignment checks but are not public Fabric headers or emulator source. The adapter owns the module,
releases asserted inputs, shuts down partial or complete sessions, and unloads deterministically.

Startup calls `Initialise`, loads program and optional sound ROMs, then uses the normal reset path.
Configuration is applied after that reset and every later reset. Execution accumulates nanoseconds into
1 ms ticks, calls `Run(8000)` for each complete tick, performs at most three catch-up ticks, and retains
only the sub-millisecond remainder. The native `Run` result is observational, not a progress count.

Snapshots are copied from the packed production structure into caller-owned Fabric lamps, reels,
character displays, and segment displays. A character display carries one display-wide normalized
brightness value: `0.0f` is off and `1.0f` is full brightness. Amber System 6 alpha-display
brightness is copied unchanged into that field. Audio supports the production PCM16 mono/stereo formats.
Each executed tick earns `sample_rate / 1000` frames using a fractional accumulator; reads are bounded
by earned frames, partial reads retain unused entitlement, and reset/shutdown discard it. Diagnostics
are bounded and use the launch callback, with `FABRIC_AMBER_TRACE=1` as an opt-in file/debugger fallback.

Automated tests use `FakeProductionAmber`, which includes the same private ABI declaration. No
proprietary DLL or ROM is needed. Real-DLL validation remains a manual Windows integration check as
listed in `fabric-architecture.md`.
