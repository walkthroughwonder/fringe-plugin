cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Fringe 1.1 — multi-platform photonic synthesizer
# ---------------------------------------------------------------------------
# Options (common):
#   -DFRINGE_BUILD_CLAP=ON|OFF
#   -DFRINGE_FORCE_AU=ON|OFF          # fail if AU requested but Xcode missing
#   -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"  # universal macOS
# ---------------------------------------------------------------------------

project(Fringe VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
endif()

option(FRINGE_BUILD_TESTS "Build unit tests" OFF)
option(FRINGE_BUILD_CLAP "Build CLAP via clap-juce-extensions" ON)
option(FRINGE_FORCE_AU "Require AU format on macOS (needs full Xcode)" OFF)
option(FRINGE_COPY_PLUGIN_AFTER_BUILD "Install plugins to user dirs after build" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ---- JUCE -----------------------------------------------------------------
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.6
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

# ---- Formats --------------------------------------------------------------
set(FRINGE_FORMATS VST3 Standalone)

if(APPLE)
  execute_process(
    COMMAND xcodebuild -version
    RESULT_VARIABLE _xcode_ok
    OUTPUT_QUIET ERROR_QUIET
  )
  if(_xcode_ok EQUAL 0)
    list(APPEND FRINGE_FORMATS AU)
    message(STATUS "Full Xcode — AU + VST3 + Standalone")
  else()
    if(FRINGE_FORCE_AU)
      message(FATAL_ERROR "FRINGE_FORCE_AU=ON but full Xcode is not available (only Command Line Tools).")
    endif()
    message(STATUS "CLT only — VST3 + Standalone (no AU). Install Xcode for AU.")
  endif()

  if(CMAKE_OSX_ARCHITECTURES)
    message(STATUS "macOS architectures: ${CMAKE_OSX_ARCHITECTURES}")
  else()
    message(STATUS "macOS architectures: host default (${CMAKE_SYSTEM_PROCESSOR})")
  endif()
endif()

set(_copy_flag FALSE)
if(FRINGE_COPY_PLUGIN_AFTER_BUILD)
  set(_copy_flag TRUE)
endif()

juce_add_plugin(Fringe
  COMPANY_NAME "Edwin Rosero"
  COMPANY_WEBSITE "https://edwinrosero.com"
  COMPANY_EMAIL "edwinrosero@gmail.com"
  BUNDLE_ID com.edwinrosero.fringe
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD ${_copy_flag}
  PLUGIN_MANUFACTURER_CODE EdRo
  PLUGIN_CODE Frng
  FORMATS ${FRINGE_FORMATS}
  PRODUCT_NAME "Fringe"
  DESCRIPTION "Photonic synthesizer — play light as sound"
  VERSION ${PROJECT_VERSION}
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

juce_generate_juce_header(Fringe)

set(FRINGE_SOURCES
  src/PluginProcessor.cpp
  src/PluginProcessor.h
  src/PluginEditor.cpp
  src/PluginEditor.h
  src/core/FringeTypes.h
  src/core/FdtdSimulator.cpp
  src/core/FdtdSimulator.h
  src/core/OpticsBuilder.cpp
  src/core/OpticsBuilder.h
  src/core/DetectorVoice.cpp
  src/core/DetectorVoice.h
  src/core/FxChain.cpp
  src/core/FxChain.h
  src/core/FringeEngine.cpp
  src/core/FringeEngine.h
)

target_sources(Fringe PRIVATE ${FRINGE_SOURCES})
target_include_directories(Fringe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(Fringe
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    FRINGE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(Fringe
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ---- CLAP (unofficial extensions) ----------------------------------------
if(FRINGE_BUILD_CLAP)
  FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        main
    GIT_SUBMODULES_RECURSE TRUE
  )
  FetchContent_MakeAvailable(clap-juce-extensions)

  clap_juce_extensions_plugin(
    TARGET Fringe
    CLAP_ID "com.edwinrosero.fringe"
    CLAP_FEATURES instrument synthesizer
    CLAP_MANUAL_URL "https://edwinrosero.com/fringe/"
    CLAP_SUPPORT_URL "https://github.com/walkthroughwonder/fringe-plugin"
  )
  message(STATUS "CLAP target enabled (Fringe_CLAP)")
else()
  message(STATUS "CLAP disabled (FRINGE_BUILD_CLAP=OFF)")
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${FRINGE_SOURCES})

# Print summary
message(STATUS "Fringe ${PROJECT_VERSION} formats: ${FRINGE_FORMATS}")
