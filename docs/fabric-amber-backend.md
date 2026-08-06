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

## Machine-specific configuration

Configuration is selected strictly from `request.machine_identifier`. `jpm-system6`
accepts only the 648-byte `FabricAmberSystem6ConfigurationV2`; `barcrest-mpu5`
accepts only the 404-byte `FabricAmberMpu5ConfigurationV1` (magic `0x354D4146`,
version 1). A launch with no configuration is valid for either machine. Fabric does
not inspect size or magic to select a format and does not fall back between formats.

System 6 retains its version-2 reel, electronic-mechanism, coin-channel, coin-route,
and percentage contract. Its production configuration sequence remains
`SetCommStyle`, `SetCommInvert`, `SetCycles`, `SetEDCEnable`, followed by selected
`SetCoinEnable`, `SetCoinValue`, and `SetLockoutInvert` calls. `SetLockoutVal` is a
live emulated output update and is not startup configuration.

MPU5 version 1 contains reel, electronic-coin, and options sections. Confirmed
settings and native exports are:

* reel steps and opto start/end/inversion: `SetSteps`, `SetOptoStart`,
  `SetOptoEnd`, and `SetOptoInvert` (reels 0..7, steps 1..255);
* reel-controller jumper profiles 0..2: `SetReelJumperProfile`;
* coin communication style 0..3, inversion, nonzero pulse cycles, and EDC:
  `SetCommStyle`, `SetCommInvert`, `SetCycles`, and `SetEDCEnable`;
* selected coin-channel enable, full uint8 denomination, and lockout inversion:
  `SetCoinEnable`, `SetCoinValue`, and `SetLockoutInvert`;
* 16 DIP states: `SetDIP`; stake and prize selectors: `SetStake` and `SetPrize`;
* percentage 0..15: `SetPercent`; explicit characteriser override (including zero):
  `SetCharacteriserAddress`; physical PIC mode 1..3: `SetPICMode`;
* SEC fitted: `SetSECFitted`; and global hopper type 0..3: `SetHopperType`.

Fabric resolves only exports requested by configuration flags and option bits. A
percentage-only configuration therefore requires only `SetPercent`. Missing requested
setters identify the export, machine, provider path, and configuration stage.

MPU5 startup uses `Initialise`, program ROM load, optional sound ROM load, all requested
configuration setters, then one `Reset`. Every explicit Fabric reset likewise reapplies
all requested settings before native `Reset`, ensuring that PIC, DIP, stake, prize,
percentage, SEC, jumper, and requested-hardware state is consumed by reset. System 6
retains its established reset-then-configuration order.

There is no general MPU5 reel-enable setter, so no reel-enable field is exposed.
`SetLegacyPICMode` is not exposed alongside the explicit `SetPICMode` selection.
Specialist service controls and the detailed per-hopper edit-page configuration remain
deferred; the global hopper type is supported now.

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

## Production platform selection

The public backend identifier remains `amber`. Production loading supports the exact
machine identifiers `jpm-system6` and `barcrest-mpu5`; the identifier, never the DLL
filename, selects the native ABI adapter. The frontend must provide an absolute provider
DLL path. Fabric neither searches for nor substitutes providers.

System 6 executes `Run(8000)` for each executed millisecond and uses the native
`void Reset()` and two-argument `CoinIn(channel, denomination)` contracts. MPU5
executes `Run(16000)` and uses the separately typed `uint8_t Reset()` and
three-argument `CoinIn(mechanism, channel, denomination)` contracts. MPU5 coin input
is currently restricted to mechanism zero.

The common packed version-2 snapshot remains 24,812 bytes. System 6 normalization
publishes 512 matrix lamps, eight reels, one alpha display, and 16 segment cells from
its general LED bank. MPU5 accepts its native 320 matrix lamps and eight reels, and
publishes the reported (up to two) alpha displays and reported (up to 40) LED-display
cells. The general LED bank and LED-display cells are intentionally distinct.

MPU5's multicolour status LED and specialist test, lamp-failure, serial-hopper,
timing, and DUART diagnostic helpers are deliberately not represented by Fabric's
current output/configuration ABI.

Real provider validation remains a manual Windows integration step using the exact
provider DLL and licensed ROM set. In particular, startup/configuration export names
and reset success semantics must be confirmed against the production MPU5 build.
