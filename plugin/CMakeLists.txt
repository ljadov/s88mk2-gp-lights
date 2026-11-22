cmake_minimum_required(VERSION 3.22)
project(KontrolLightsMap VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MINGW)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/mingw-compat)
  # VST3 autonome : runtime GCC lie statiquement
  set(CMAKE_SHARED_MODULE_LINKER_FLAGS "${CMAKE_SHARED_MODULE_LINKER_FLAGS} -static")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")
endif()

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        7.0.12)
FetchContent_MakeAvailable(juce)

juce_add_plugin(KontrolLightsMap
  COMPANY_NAME "Kontrol Lights"
  PRODUCT_NAME "Kontrol Lights Map"
  PLUGIN_MANUFACTURER_CODE KlLt
  PLUGIN_CODE KlMp
  FORMATS VST3
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST FALSE)  # le manifeste est optionnel ; son outil ne tourne pas en compilation croisee

target_sources(KontrolLightsMap PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)
target_include_directories(KontrolLightsMap PRIVATE ../shared)
target_compile_definitions(KontrolLightsMap PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(KontrolLightsMap
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MINGW)
  target_link_options(KontrolLightsMap_VST3 PRIVATE -static -static-libgcc -static-libstdc++)
endif()
