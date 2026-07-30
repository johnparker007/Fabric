#ifndef FABRIC_FABRIC_H
#define FABRIC_FABRIC_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(FABRIC_EXPORTS)
#define FABRIC_API __declspec(dllexport)
#elif defined(_WIN32)
#define FABRIC_API __declspec(dllimport)
#else
#define FABRIC_API
#endif
#if defined(_WIN32)
#define FABRIC_CALL __cdecl
#else
#define FABRIC_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FABRIC_ABI_VERSION_1 UINT32_C(0x00010000)
#define FABRIC_ABI_VERSION_CURRENT FABRIC_ABI_VERSION_1
#define FABRIC_IDENTIFIER_CAPACITY 64u
#define FABRIC_PATH_CAPACITY 1024u
#define FABRIC_ERROR_CAPACITY 512u
#define FABRIC_INDEX_UNAVAILABLE INT32_C(-1)
#define FABRIC_LAUNCH_REQUEST_V1_MIN_SIZE                                      \
  ((uint32_t)offsetof(FabricLaunchRequest, rom_resources))
#define FABRIC_CHARACTER_CAPACITY 16u
#define FABRIC_SEGMENT_DIGIT_CAPACITY 16u

typedef struct FabricRuntime FabricRuntime;
typedef struct FabricMachineSession FabricMachineSession;
typedef void(FABRIC_CALL *FabricDiagnosticCallback)(const char *message,
                                                    void *user_data);

typedef enum FabricResult {
  FABRIC_OK = 0,
  FABRIC_INVALID_ARGUMENT = 1,
  FABRIC_UNSUPPORTED_VERSION = 2,
  FABRIC_NOT_FOUND = 3,
  FABRIC_INVALID_STATE = 4,
  FABRIC_BUFFER_TOO_SMALL = 5,
  FABRIC_NOT_SUPPORTED = 6,
  FABRIC_BACKEND_ERROR = 7,
  FABRIC_INTERNAL_ERROR = 8
} FabricResult;

typedef enum FabricCapability {
  FABRIC_CAPABILITY_DIGITAL_INPUT = UINT64_C(1) << 0,
  FABRIC_CAPABILITY_LAMPS = UINT64_C(1) << 1,
  FABRIC_CAPABILITY_REELS = UINT64_C(1) << 2,
  FABRIC_CAPABILITY_CHARACTER_DISPLAYS = UINT64_C(1) << 3,
  FABRIC_CAPABILITY_SEGMENT_DISPLAYS = UINT64_C(1) << 4,
  FABRIC_CAPABILITY_AUDIO = UINT64_C(1) << 5
} FabricCapability;

typedef struct FabricLaunchRequest {
  uint32_t struct_size;
  uint32_t struct_version;
  char backend_kind[FABRIC_IDENTIFIER_CAPACITY];
  char machine_identifier[FABRIC_IDENTIFIER_CAPACITY];
  char backend_path[FABRIC_PATH_CAPACITY];
  const char *const *rom_paths;
  uint32_t rom_path_count;
  const void *machine_configuration;
  uint32_t machine_configuration_size;
  uint32_t reserved;
  /* Append-only v1 extension. Prefer typed resources when non-null. */
  const struct FabricRomResource *rom_resources;
  uint32_t rom_resource_count;
  /* Append-only v1 extension. Text is valid only during the callback. */
  FabricDiagnosticCallback diagnostic_callback;
  void *diagnostic_user_data;
} FabricLaunchRequest;

typedef enum FabricRomRole {
  FABRIC_ROM_ROLE_OTHER = 0,
  FABRIC_ROM_ROLE_PROGRAM = 1,
  FABRIC_ROM_ROLE_SOUND = 2
} FabricRomRole;

typedef struct FabricRomResource {
  uint32_t struct_size;
  uint32_t struct_version;
  uint32_t role;
  uint32_t slot;
  const char *path;
  uint64_t reserved[2];
} FabricRomResource;

typedef struct FabricCapabilities {
  uint32_t struct_size;
  uint32_t struct_version;
  uint64_t flags;
  uint64_t reserved[4];
} FabricCapabilities;

typedef struct FabricInput {
  uint32_t struct_size;
  uint32_t struct_version;
  char identifier[FABRIC_IDENTIFIER_CAPACITY];
  int32_t numerical_index;
  uint8_t active;
  uint8_t reserved[7];
} FabricInput;

typedef struct FabricLamp {
  uint32_t struct_size;
  uint32_t struct_version;
  char identifier[FABRIC_IDENTIFIER_CAPACITY];
  int32_t numerical_index;
  uint8_t logical_state;
  uint8_t reserved[3];
  float brightness;
} FabricLamp;

typedef struct FabricReel {
  uint32_t struct_size;
  uint32_t struct_version;
  char identifier[FABRIC_IDENTIFIER_CAPACITY];
  int32_t numerical_index;
  int32_t position;
} FabricReel;

typedef struct FabricCharacterDisplay {
  uint32_t struct_size;
  uint32_t struct_version;
  char identifier[FABRIC_IDENTIFIER_CAPACITY];
  uint32_t character_count;
  uint32_t character_capacity;
  uint32_t characters[FABRIC_CHARACTER_CAPACITY];
  uint8_t attributes[FABRIC_CHARACTER_CAPACITY];
} FabricCharacterDisplay;

typedef struct FabricSegmentDisplay {
  uint32_t struct_size;
  uint32_t struct_version;
  char identifier[FABRIC_IDENTIFIER_CAPACITY];
  uint32_t digit_count;
  uint32_t digit_capacity;
  uint64_t segment_masks[FABRIC_SEGMENT_DIGIT_CAPACITY];
} FabricSegmentDisplay;

/* The caller owns all arrays and sets their capacities before each call. */
typedef struct FabricMachineSnapshot {
  uint32_t struct_size;
  uint32_t struct_version;
  uint64_t sequence;
  FabricLamp *lamps;
  uint32_t lamp_capacity;
  uint32_t lamp_count;
  FabricReel *reels;
  uint32_t reel_capacity;
  uint32_t reel_count;
  FabricCharacterDisplay *character_displays;
  uint32_t character_display_capacity;
  uint32_t character_display_count;
  FabricSegmentDisplay *segment_displays;
  uint32_t segment_display_capacity;
  uint32_t segment_display_count;
} FabricMachineSnapshot;

typedef struct FabricAudioFormat {
  uint32_t struct_size;
  uint32_t struct_version;
  uint32_t sample_rate;
  uint16_t channel_count;
  uint16_t bits_per_sample;
  uint8_t interleaved;
  uint8_t signed_samples;
  uint8_t little_endian;
  uint8_t reserved;
} FabricAudioFormat;

FABRIC_API FabricResult FABRIC_CALL
FabricCreateRuntime(uint32_t requested_version, FabricRuntime **out_runtime);
FABRIC_API void FABRIC_CALL FabricDestroyRuntime(FabricRuntime *runtime);
FABRIC_API FabricResult FABRIC_CALL
FabricRuntimeGetLastError(FabricRuntime *runtime, char *buffer,
                          uint32_t buffer_size, uint32_t *required_size);
FABRIC_API FabricResult FABRIC_CALL
FabricCreateSession(FabricRuntime *runtime, const FabricLaunchRequest *request,
                    FabricMachineSession **out_session);
FABRIC_API void FABRIC_CALL FabricDestroySession(FabricMachineSession *session);
FABRIC_API FabricResult FABRIC_CALL
FabricSessionInitialise(FabricMachineSession *session);
FABRIC_API FabricResult FABRIC_CALL
FabricSessionReset(FabricMachineSession *session);
FABRIC_API FabricResult FABRIC_CALL FabricSessionAdvance(
    FabricMachineSession *session, uint64_t elapsed_nanoseconds);
FABRIC_API FabricResult FABRIC_CALL
FabricSessionShutdown(FabricMachineSession *session);
FABRIC_API FabricResult FABRIC_CALL FabricSessionSubmitInput(
    FabricMachineSession *session, const FabricInput *input);
FABRIC_API FabricResult FABRIC_CALL FabricSessionGetCapabilities(
    FabricMachineSession *session, FabricCapabilities *capabilities);
FABRIC_API FabricResult FABRIC_CALL FabricSessionGetSnapshot(
    FabricMachineSession *session, FabricMachineSnapshot *snapshot);
FABRIC_API FabricResult FABRIC_CALL FabricSessionGetAudioFormat(
    FabricMachineSession *session, FabricAudioFormat *format);
FABRIC_API FabricResult FABRIC_CALL
FabricSessionReadAudio(FabricMachineSession *session, int16_t *samples,
                       uint32_t frame_capacity, uint32_t *frames_written);
FABRIC_API FabricResult FABRIC_CALL
FabricSessionGetLastError(FabricMachineSession *session, char *buffer,
                          uint32_t buffer_size, uint32_t *required_size);

#ifdef __cplusplus
}
#endif
#endif
