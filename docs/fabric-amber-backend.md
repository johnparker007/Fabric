# Production Amber backend

The `amber` backend loads only the exact absolute DLL path supplied by the frontend. It neither searches
for a DLL nor substitutes a filename. The supplied library must implement the production flat/singleton
Amber/JPM System 6 C ABI. Required exports are resolved eagerly; an error identifies both a missing
symbol and the requested DLL. Optional exports enable sound and machine configuration features.

Amber has two distinct input paths. `FABRIC_INPUT_DIGITAL` represents a
multiplexed machine switch (Start, Hold, Door, Refill, and similar inputs) and
is forwarded to `TurnSwitchOn`/`TurnSwitchOff`. `FABRIC_INPUT_COIN` represents
one coin-mechanism action and is forwarded to `CoinIn(coin_channel,
coin_value)` on the active transition only. Coin release clears Fabric's
de-duplication state and makes no Amber call; a repeated active submission is
also ignored. Fabric never translates a coin into raw matrix switches 72--77.
Amber acceptance returns `FABRIC_OK`; a valid Amber lockout/configuration
rejection returns `FABRIC_INPUT_REJECTED`, while malformed channel/value input
returns `FABRIC_INVALID_ARGUMENT`.

Private structure declarations for this external binary contract live in
`src/Backends/Amber/ProductionAmberAbi.h`. They preserve four-byte packing and compile-time
size/alignment checks but are not public Fabric headers or emulator source. The adapter owns the module,
releases asserted inputs, shuts down partial or complete sessions, and unloads deterministically.

Startup calls `Initialise`, loads program and optional sound ROMs, then uses the normal reset path.
Configuration is applied after that reset and every later reset. Execution accumulates nanoseconds into
1 ms ticks, calls `Run(8000)` for each complete tick, performs at most three catch-up ticks, and retains
only the sub-millisecond remainder. The native `Run` result is observational, not a progress count.

Coin configuration includes per-channel enable, value, lockout value, and
lockout inversion plus the global communication style, communication
inversion, pulse cycles, and EDC enable state. The adapter reapplies
`SetCommStyle`, `SetCommInvert`, `SetCycles`, and `SetEDCEnable` after every
reset before accepting input. `FABRIC_AMBER_COIN_COMMUNICATION_PARALLEL` is
raw value `0`, matching `Parallel` in the supplied Amber CoinMech
`CommunicationStyle` enumeration; the known frontend configuration uses this
value with inversion off, 800000 pulse cycles, and EDC off. Fabric transports
the frontend's raw denomination code unchanged. The available Amber source
does not establish a stable public denomination-code table, so Fabric does not
invent or publish denomination labels.

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
