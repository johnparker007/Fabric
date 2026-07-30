#pragma once

// Private declarations for the external production Amber binary contract.
// This is not emulator source and is not part of Fabric's public ABI. Keep the
// layouts, fixed-width fields, and four-byte packing synchronized with the
// externally supplied DLL. Do not include this file from public headers.

#include <stdint.h>

// The external structure names use these fixed-width aliases.
#ifndef _BASETSD_H_
typedef int8_t   INT8;
typedef uint8_t  UINT8;
typedef int16_t  INT16;
typedef uint16_t UINT16;
typedef int32_t  INT32;
typedef uint32_t UINT32;
typedef int64_t  INT64;
typedef uint64_t UINT64;
#endif

#define PA2_OUTPUT_SNAPSHOT_VERSION 2

#define PA2_MAX_MATRIX_LAMPS       512
#define PA2_MAX_DIRECT_LAMPS       32
#define PA2_MAX_FLO_LAMPS          4
#define PA2_MAX_PRISM_LAMPS        16
#define PA2_MAX_LEDS               512
#define PA2_MAX_TRIAC_LAMPS        32
#define PA2_MAX_FLUORESCENT_LAMPS  8
#define PA2_MAX_DISCO_LAMPS        64

#define PA2_NUM_REELS              8
#define PA2_NUM_ALPHA_DISPLAYS     2
#define PA2_NUM_ALPHA_CHARS        16
#define PA2_NUM_ALPHA_SEGMENTS     16
#define PA2_ALPHA_SEGMENTS_MPU4     14
#define PA2_ALPHA_SEGMENTS_IMPACT   16
#define PA2_NUM_LED_DISPLAYS       40
#define PA2_NUM_LED_SEGMENTS       8

#define PA2_MAX_ELECTRONIC_MECHS   1
#define PA2_MAX_MECHANICAL_MECHS   8
#define PA2_MAX_COIN_ENTRY_LAMPS   10

#define PA2_NUM_METERS             6
#define PA2_NUM_TUBES              8
#define PA2_NUM_DIPS               16
#define PA2_NUM_HOPPERS            2

#define PA2_AUDIO_FORMAT_VERSION   1
#define PA2_AUDIO_FORMAT_PCM_S16   1

#pragma pack(push, 4)

struct PA2_LampState
{
    UINT8 OnOff;
    UINT8 Reserved0;
    UINT8 Reserved1;
    UINT8 Reserved2;

    float Brightness;   // 0..1 normal, >1 allowed for over-bright
    float FilamentR;
    float FilamentG;
    float FilamentB;
};

struct PA2_ReelState
{
    INT32 Position;
};

struct PA2_AlphaSegmentedState
{
    // 14 = MPU4 native alpha, 16 = IMPACT/System 6 native alpha.
    // Segment masks remain native to the emulated hardware; the front-end
    // expands 14-segment MPU4 masks into its existing 16-segment renderer.
    UINT8  SegmentCount;
    UINT8  Reserved0;
    UINT8  Reserved1;
    UINT8  Reserved2;

    UINT16 Segments[PA2_NUM_ALPHA_CHARS];
    UINT8  DotComma[PA2_NUM_ALPHA_CHARS];
    float  Brightness;
};

struct PA2_AlphaDotState
{
    UINT8 Columns[PA2_NUM_ALPHA_CHARS][5];
    UINT8 DotComma[PA2_NUM_ALPHA_CHARS];
    float Brightness;
};

struct PA2_LedDisplayState
{
    UINT32 OnOff;
    float  Brightness;
};

struct PA2_ElectronicMechState
{
    UINT8 CoinLamp[2];
    UINT8 LockoutState;
    UINT8 Reserved0;
};

struct PA2_MechanicalMechState
{
    UINT8 Enabled;
    UINT8 MeterPulse;
    UINT8 LockoutState;
    UINT8 Reserved0;
};

struct PA2_AudioFormat
{
    UINT32 SizeBytes;
    UINT32 Version;
    UINT32 SampleRate;     // e.g. 48000
    UINT32 Channels;       // external API target is 2
    UINT32 BitsPerSample;  // external API target is 16
    UINT32 Format;         // PA2_AUDIO_FORMAT_PCM_S16
};

struct PA2_OutputSnapshot
{
    UINT32 SizeBytes;
    UINT32 Version;

    UINT32 MatrixLampCount;
    UINT32 DirectLampCount;
    UINT32 FloLampCount;
    UINT32 PrismLampCount;
    UINT32 LedCount;
    UINT32 TriacLampCount;
    UINT32 FluorescentLampCount;
    UINT32 DiscoLampCount;
    UINT32 ReelCount;
    UINT32 AlphaSegmentedDisplayCount;
    UINT32 AlphaDotDisplayCount;
    UINT32 LedDisplayCount;
    UINT32 ElectronicMechCount;
    UINT32 MechanicalMechCount;
    UINT32 CoinEntryLampCount;
    UINT32 MeterCount;
    UINT32 TubeCount;
    UINT32 DipCount;
    UINT32 HopperCount;

    PA2_LampState MatrixLamps[PA2_MAX_MATRIX_LAMPS];
    PA2_LampState DirectLamps[PA2_MAX_DIRECT_LAMPS];
    PA2_LampState FloLamps[PA2_MAX_FLO_LAMPS];
    PA2_LampState PrismLamps[PA2_MAX_PRISM_LAMPS];
    PA2_LampState Leds[PA2_MAX_LEDS];
    PA2_LampState TriacLamps[PA2_MAX_TRIAC_LAMPS];
    PA2_LampState FluorescentLamps[PA2_MAX_FLUORESCENT_LAMPS];
    PA2_LampState DiscoLamps[PA2_MAX_DISCO_LAMPS];

    PA2_ReelState Reels[PA2_NUM_REELS];

    PA2_AlphaSegmentedState AlphaSegmented[PA2_NUM_ALPHA_DISPLAYS];
    PA2_AlphaDotState AlphaDot[PA2_NUM_ALPHA_DISPLAYS];

    PA2_LedDisplayState LedDisplays[PA2_NUM_LED_DISPLAYS];

    PA2_ElectronicMechState ElectronicMechs[PA2_MAX_ELECTRONIC_MECHS];
    PA2_MechanicalMechState MechanicalMechs[PA2_MAX_MECHANICAL_MECHS];
    PA2_LampState CoinEntryLamps[PA2_MAX_COIN_ENTRY_LAMPS];

    UINT32 Meters[PA2_NUM_METERS];

    UINT32 TubeLevel[PA2_NUM_TUBES];
    UINT32 TubeFullLevel[PA2_NUM_TUBES];
    UINT32 TubeLoLevel[PA2_NUM_TUBES];
    UINT32 TubeHiLevel[PA2_NUM_TUBES];

    UINT8 Dips[PA2_NUM_DIPS];

    UINT32 HopperLevel[PA2_NUM_HOPPERS];
    UINT32 HopperFullLevel[PA2_NUM_HOPPERS];
    UINT32 HopperLoLevel[PA2_NUM_HOPPERS];
    UINT32 HopperHiLevel[PA2_NUM_HOPPERS];
    UINT32 HopperCoinsIn[PA2_NUM_HOPPERS];
    UINT32 HopperCoinsOut[PA2_NUM_HOPPERS];
    UINT32 HopperCoinsRefilled[PA2_NUM_HOPPERS];

    UINT8 StatusLED;
    UINT8 Reserved0;
    UINT8 Reserved1;
    UINT8 Reserved2;
};

#pragma pack(pop)

#ifdef __cplusplus
#ifdef _WIN32
#define FABRIC_PRODUCTION_AMBER_CALL __cdecl
#else
#define FABRIC_PRODUCTION_AMBER_CALL
#endif
struct ProductionAmberApi {
  uint8_t (FABRIC_PRODUCTION_AMBER_CALL *Initialise)() = nullptr;
  uint8_t (FABRIC_PRODUCTION_AMBER_CALL *Shutdown)() = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *Reset)() = nullptr;
  int32_t (FABRIC_PRODUCTION_AMBER_CALL *Run)(uint32_t) = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *LoadROM)(uint8_t *, uint8_t *, uint8_t *, uint8_t *) = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *GetOutputSnapshotSize)() = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *GetOutputSnapshot)(void *, uint32_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *TurnSwitchOn)(uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *TurnSwitchOff)(uint8_t) = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *LoadSoundROM)(uint8_t *, uint8_t *, uint8_t *, uint8_t *) = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *GetAudioFormat)(PA2_AudioFormat *, uint32_t) = nullptr;
  uint32_t (FABRIC_PRODUCTION_AMBER_CALL *FillAudioFrames)(int16_t *, uint32_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetOptoInvert)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetOptoStart)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetOptoEnd)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetSteps)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetCoinValue)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetCoinEnable)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetLockoutVal)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetLockoutInvert)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetEnable)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetCounterIn)(uint8_t, uint32_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetCounterOut)(uint8_t, uint32_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetPortIndex)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetCoin)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetLevel)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetFullLevel)(uint8_t, uint8_t) = nullptr;
  void (FABRIC_PRODUCTION_AMBER_CALL *SetPercent)(uint8_t) = nullptr;
};
#undef FABRIC_PRODUCTION_AMBER_CALL

static_assert(alignof(PA2_OutputSnapshot) == 4, "external snapshot packing");
static_assert(sizeof(PA2_OutputSnapshot) == 24812, "external snapshot size");
static_assert(sizeof(PA2_AudioFormat) == 24, "external audio format size");
#endif
