# JUCE module targets carry their .cpp/.mm files as INTERFACE sources, so linking
# one compiles JUCE into the consumer. Our warning flags and clang-tidy must
# never touch that code, so our own code gets only the module headers and
# definitions, gathered here, and never links a JUCE module itself.
function(md_juce_headers_only target)
  set(pending juce::juce_audio_utils)
  set(seen)
  while(pending)
    list(POP_FRONT pending module)
    if(NOT TARGET ${module} OR module IN_LIST seen)
      continue()
    endif()
    list(APPEND seen ${module})
    get_target_property(includes ${module} INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(system_includes ${module} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    get_target_property(definitions ${module} INTERFACE_COMPILE_DEFINITIONS)
    if(includes)
      target_include_directories(${target} SYSTEM INTERFACE ${includes})
    endif()
    if(system_includes)
      target_include_directories(${target} SYSTEM INTERFACE ${system_includes})
    endif()
    if(definitions)
      target_compile_definitions(${target} INTERFACE ${definitions})
    endif()
    get_target_property(dependencies ${module} INTERFACE_LINK_LIBRARIES)
    if(dependencies)
      list(APPEND pending ${dependencies})
    endif()
  endwhile()
endfunction()

add_library(md_juce_headers INTERFACE)
md_juce_headers_only(md_juce_headers)
target_link_libraries(md_juce_headers INTERFACE juce::juce_recommended_config_flags)
target_compile_definitions(md_juce_headers
  INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

juce_add_binary_data(MdFonts
  HEADER_NAME MdFontData.h
  NAMESPACE MdFontData
  SOURCES
    ${CMAKE_SOURCE_DIR}/ui/fonts/SpaceGrotesk500.ttf
    ${CMAKE_SOURCE_DIR}/ui/fonts/SpaceGrotesk700.ttf
    ${CMAKE_SOURCE_DIR}/ui/fonts/SpaceMonoRegular.ttf
    ${CMAKE_SOURCE_DIR}/ui/fonts/SpaceMonoBold.ttf)

add_library(md_plugin_core STATIC PluginProcessor.cpp PluginEditor.cpp Playback.cpp
  AuditionSynth.cpp Link.cpp DragFiles.cpp
  ${CMAKE_SOURCE_DIR}/ui/Fonts.cpp ${CMAKE_SOURCE_DIR}/ui/ChordBubble.cpp ${CMAKE_SOURCE_DIR}/ui/LaneView.cpp ${CMAKE_SOURCE_DIR}/ui/AuraPad.cpp
  ${CMAKE_SOURCE_DIR}/ui/StatusFields.cpp ${CMAKE_SOURCE_DIR}/ui/Readout.cpp ${CMAKE_SOURCE_DIR}/ui/TopLine.cpp ${CMAKE_SOURCE_DIR}/ui/ScreenComponent.cpp
  ${CMAKE_SOURCE_DIR}/ui/KeyButton.cpp ${CMAKE_SOURCE_DIR}/ui/SprinkleKey.cpp)
target_include_directories(md_plugin_core PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(md_plugin_core
  PUBLIC md_juce_headers md_engine md_bank md_midiexport
  PRIVATE MdFonts juce::juce_recommended_warning_flags)
# The generated font header is not our code, so lint treats it as a system header.
get_target_property(md_font_includes MdFonts INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(MdFonts PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
target_include_directories(md_plugin_core SYSTEM PRIVATE ${md_font_includes})
md_target_defaults(md_plugin_core)

juce_add_plugin(MagicDust
  PRODUCT_NAME "Magic Dust"
  COMPANY_NAME "Mark Stent"
  BUNDLE_ID com.markstent.magicdust
  PLUGIN_MANUFACTURER_CODE Mdus
  PLUGIN_CODE Mdst
  FORMATS AU VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Generator
  COPY_PLUGIN_AFTER_BUILD ${MD_COPY_PLUGIN})

target_link_libraries(MagicDust
  PUBLIC
    md_plugin_core
  PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
