# Production Amber backend

The public Fabric backend identifier remains `amber`. Frontends select the Amber platform with the
machine identifier:

- `jpm-system6` for JPM System 6.
- `barcrest-mpu5` for Barcrest MPU5.

The `amber` backend loads only the exact absolute DLL path supplied by the frontend. It does not search
for DLLs, substitute filenames, or infer the platform from the provider filename. Required exports are
resolved eagerly; missing-export diagnostics include the symbol name and requested DLL path.

## Platform ABI differences

Digital matrix, service, and door inputs use `TurnSwitchOn(index)` and `TurnSwitchOff(index)` on both
platforms. Coin actions are a distinct Fabric input kind with a channel and denomination code. System 6
calls `CoinIn(channel, value)`. MPU5 calls its three-argument ABI as `CoinIn(0, channel, value)`; the
mechanism index is fixed to `0` because Fabric's public input ABI does not yet model multiple MPU5 coin
mechanisms. A zero coin return becomes `FABRIC_INPUT_REJECTED`, not a broken session.

Reset is platform-specific. System 6 exports `void Reset(void)`. MPU5 exports `UINT8 Reset(void)`, and
Fabric treats a zero MPU5 reset return as a backend failure so callers receive a clear startup or reset
error.

Execution retains Fabric's one-millisecond model and bounded catch-up. System 6 advances with
`Run(8000)` for each complete millisecond tick. MPU5 advances with `Run(16000)` for each complete
millisecond tick. The native `Run` return remains observational.

## Outputs

Fabric validates and publishes platform-specific output counts instead of forcing MPU5 snapshots through
System 6 assumptions:

| Machine identifier | Matrix lamps | Reels | Alpha displays | Segment display cells |
| --- | ---: | ---: | ---: | ---: |
| `jpm-system6` | 512 | 8 | 1 | 16 |
| `barcrest-mpu5` | 320 | 8 | 2 | 40 |

Matrix lamps, general LEDs, and LED display cells remain distinct collections. MPU5 snapshots may expose
40 `LedDisplays[]` cells without requiring the general `Leds[]` array to be populated. Segment display
identifiers remain stable as `amber.seven-segment.N`; MPU5 publishes `amber.seven-segment.0` through
`amber.seven-segment.39`.

MPU5's native status LED is multi-state (`0` off, `1` green, `2` red, `3` yellow). Fabric's current
public snapshot has no suitable multi-state cabinet LED output, so the adapter intentionally leaves this
status LED unsupported rather than flattening it to an incorrect boolean.

Private structure declarations for the binary snapshot contract live in
`src/Backends/Amber/ProductionAmberAbi.h`. They preserve four-byte packing and compile-time size/alignment
checks but are not public Fabric headers or emulator source.

## Configuration and audio

The existing `FabricAmberConfigurationV2` remains the current public Amber configuration shape. System 6
continues applying reel, coin-mechanism, coin-route, and percentage settings after startup reset and
after every explicit reset.

For the initial MPU5 path, Fabric applies the source-compatible settings currently represented by the
shared configuration when relevant to the MPU5 fake-provider coverage, notably the percentage switch.
Broader MPU5 settings such as DIP switches, PIC/characteriser selection, SEC fitted, hopper type, reel
jumpers/profiles, and multiple coin mechanisms are intentionally not exposed yet because they need a
focused public ABI addition and real-ROM validation.

PCM audio reuses the production PCM16 path where the provider exposes `GetAudioFormat` and
`FillAudioFrames`. Each executed tick earns frames using a fractional accumulator, reads are bounded by
earned frames, and reset/shutdown discard queued entitlement.

Automated tests use fake System 6 and MPU5 provider DLLs with their real incompatible reset and coin
function signatures. They do not require proprietary DLLs or ROMs. Real-DLL validation remains a manual
integration check as listed in `fabric-architecture.md`.
