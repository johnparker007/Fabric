cmake_minimum_required(VERSION 3.16)
project(Fabric LANGUAGES C CXX)

# Apply the language contract to every C++ target, including test executables
# shown by Visual Studio's CMake Targets view.  Keeping this at project scope
# prevents a newly added target from silently falling back to MSVC's default
# language mode when it consumes C++17 headers such as <filesystem>.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE FABRIC_SOURCE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT FABRIC_SOURCE_REVISION)
  set(FABRIC_SOURCE_REVISION unknown)
endif()

add_library(FabricRuntimeCore OBJECT
  src/FabricRuntime.cpp
  src/Backends/Amber/AmberDynamicLibrary.cpp
  src/Backends/Amber/AmberTrace.cpp
  src/Backends/Amber/ProductionAmberAdapter.cpp
  src/Backends/Amber/AmberBackendProvider.cpp)
target_include_directories(FabricRuntimeCore PRIVATE include src src/Backends/Amber)
target_compile_definitions(FabricRuntimeCore PRIVATE FABRIC_EXPORTS FABRIC_SOURCE_REVISION="${FABRIC_SOURCE_REVISION}")
target_compile_features(FabricRuntimeCore PRIVATE cxx_std_17)
set_target_properties(FabricRuntimeCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(FabricRuntimeCore PRIVATE /W4 /WX)
else()
  target_compile_options(FabricRuntimeCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(FabricRuntime SHARED $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(FabricRuntime PUBLIC include)
target_compile_features(FabricRuntime PUBLIC cxx_std_17)
set_target_properties(FabricRuntime PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
if(CMAKE_DL_LIBS)
  target_link_libraries(FabricRuntime PRIVATE ${CMAKE_DL_LIBS})
endif()

enable_testing()
function(add_production_amber_fake target)
  add_library(${target} SHARED tests/fakes/FakeProductionAmber.cpp)
  target_include_directories(${target} PRIVATE src/Backends/Amber)
  target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()
add_production_amber_fake(FakeProductionAmber)
add_production_amber_fake(FakeProductionAmberMissingRequired)
target_compile_definitions(FakeProductionAmberMissingRequired PRIVATE FAKE_AMBER_OMIT_INITIALISE)
add_production_amber_fake(FakeProductionAmberMissingConfig)
target_compile_definitions(FakeProductionAmberMissingConfig PRIVATE FAKE_AMBER_OMIT_SET_STEPS)
add_production_amber_fake(FakeProductionAmber48Stereo)
target_compile_definitions(FakeProductionAmber48Stereo PRIVATE FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=2)
add_production_amber_fake(FakeProductionAmber48Mono)
target_compile_definitions(FakeProductionAmber48Mono PRIVATE FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=1)
add_production_amber_fake(FakeProductionAmberPartial)
target_compile_definitions(FakeProductionAmberPartial PRIVATE FAKE_AMBER_SAMPLE_RATE=48000 FAKE_AMBER_CHANNELS=2 FAKE_AMBER_MAX_FRAMES_PER_READ=7)

add_executable(FabricRuntimeTests tests/fabric_runtime_tests.cpp tests/fakes/FakeBackend.cpp $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(FabricRuntimeTests PRIVATE include src tests/fakes)
target_compile_definitions(FabricRuntimeTests PRIVATE FABRIC_EXPORTS)
if(CMAKE_DL_LIBS)
  target_link_libraries(FabricRuntimeTests PRIVATE ${CMAKE_DL_LIBS})
endif()
add_test(NAME FabricRuntimeTests COMMAND FabricRuntimeTests)

add_executable(AmberProductionBackendTests tests/amber_production_backend_tests.cpp)
target_include_directories(AmberProductionBackendTests PRIVATE include)
target_link_libraries(AmberProductionBackendTests PRIVATE FabricRuntime)
target_compile_definitions(AmberProductionBackendTests PRIVATE
  FAKE_PRODUCTION_AMBER_PATH="$<TARGET_FILE:FakeProductionAmber>"
  FAKE_PRODUCTION_AMBER_MISSING_CONFIG_PATH="$<TARGET_FILE:FakeProductionAmberMissingConfig>"
  FAKE_PRODUCTION_AMBER_MISSING_REQUIRED_PATH="$<TARGET_FILE:FakeProductionAmberMissingRequired>")
add_dependencies(AmberProductionBackendTests FakeProductionAmber FakeProductionAmberMissingConfig FakeProductionAmberMissingRequired)
add_test(NAME AmberProductionBackendTests COMMAND AmberProductionBackendTests)

add_executable(AmberProductionAudioTests tests/amber_production_audio_tests.cpp)
target_include_directories(AmberProductionAudioTests PRIVATE include)
target_link_libraries(AmberProductionAudioTests PRIVATE FabricRuntime)
target_compile_definitions(AmberProductionAudioTests PRIVATE
  FAKE_AMBER_441_STEREO_PATH="$<TARGET_FILE:FakeProductionAmber>"
  FAKE_AMBER_48_STEREO_PATH="$<TARGET_FILE:FakeProductionAmber48Stereo>"
  FAKE_AMBER_48_MONO_PATH="$<TARGET_FILE:FakeProductionAmber48Mono>"
  FAKE_AMBER_PARTIAL_PATH="$<TARGET_FILE:FakeProductionAmberPartial>")
add_dependencies(AmberProductionAudioTests FakeProductionAmber FakeProductionAmber48Stereo FakeProductionAmber48Mono FakeProductionAmberPartial)
add_test(NAME AmberProductionAudioTests COMMAND AmberProductionAudioTests)

add_executable(AmberTraceTests tests/amber_trace_tests.cpp $<TARGET_OBJECTS:FabricRuntimeCore>)
target_include_directories(AmberTraceTests PRIVATE include src src/Backends/Amber)
target_compile_definitions(AmberTraceTests PRIVATE FABRIC_EXPORTS FAKE_PRODUCTION_AMBER_PATH="$<TARGET_FILE:FakeProductionAmber>")
target_compile_features(AmberTraceTests PRIVATE cxx_std_17)
add_dependencies(AmberTraceTests FakeProductionAmber)
if(CMAKE_DL_LIBS)
  target_link_libraries(AmberTraceTests PRIVATE ${CMAKE_DL_LIBS})
endif()
foreach(mode explicit default disabled invalid)
  add_test(NAME AmberTraceTests-${mode} COMMAND AmberTraceTests ${mode})
endforeach()

add_test(NAME FabricExportInspection COMMAND ${CMAKE_COMMAND} -DLIBRARY=$<TARGET_FILE:FabricRuntime> -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fabric_exports.cmake)
add_executable(FabricHeaderC tests/fabric_header_c.c)
target_include_directories(FabricHeaderC PRIVATE include)
target_link_libraries(FabricHeaderC PRIVATE FabricRuntime)
set_property(TARGET FabricHeaderC PROPERTY C_STANDARD 11)
add_executable(FabricHeaderCpp tests/fabric_header_cpp.cpp)
target_include_directories(FabricHeaderCpp PRIVATE include)
target_link_libraries(FabricHeaderCpp PRIVATE FabricRuntime)
target_compile_features(FabricHeaderCpp PRIVATE cxx_std_17)
add_test(NAME FabricHeaderC COMMAND FabricHeaderC)
add_test(NAME FabricHeaderCpp COMMAND FabricHeaderCpp)
