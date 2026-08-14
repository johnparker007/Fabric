# Public headers

`fabric/` contains Fabric's versioned C ABI. Backend implementation contracts must remain private.

ABI v4 consumers (including managed bindings) must declare `FabricInput` as 88 bytes with
`struct_size` at 0, `struct_version` at 4, the 64-byte identifier at 8, `numerical_index` at 72,
32-bit `kind` at 76, and the one-byte `active`, `coin_channel`, and `coin_value` fields at 80, 81,
and 82, followed by five reserved bytes. Digital inputs use kind 0 and the numerical index. Coin
inputs use kind 1 and the explicit channel/value fields. `FABRIC_INPUT_REJECTED` (9) is a handled
backend rejection; bindings must not treat it as session failure.

Amber launch configuration is selected by machine identifier, not by inspecting the
blob. `jpm-system6` accepts `FabricAmberSystem6ConfigurationV2` (648 bytes), while
`barcrest-mpu3` accepts the 48-byte `FabricAmberMpu3Config`, and
`barcrest-mpu5` accepts `FabricAmberMpu5ConfigurationV1` (404 bytes). These machines
may be launched without a configuration blob; the two configuration types are not
interchangeable.
