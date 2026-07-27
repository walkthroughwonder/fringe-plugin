cmake_minimum_required(VERSION 3.22)

project(Fringe VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(FRINGE_BUILD_TESTS "Build unit tests" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.6
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

set(FRINGE_FORMATS VST3 Standalone)
if(APPLE)
  execute_process(
    COMMAND xcodebuild -version
    RESULT_VARIABLE _xcode_ok
    OUTPUT_QUIET ERROR_QUIET
  )
  if(_xcode_ok EQUAL 0)
    list(APPEND FRINGE_FORMATS AU)
  endif()
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
  COPY_PLUGIN_AFTER_BUILD TRUE
  PLUGIN_MANUFACTURER_CODE EdRo
  PLUGIN_CODE Frng
  FORMATS ${FRINGE_FORMATS}
  PRODUCT_NAME "Fringe"
  DESCRIPTION "Photonic synthesizer — play light as sound"
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

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${FRINGE_SOURCES})
