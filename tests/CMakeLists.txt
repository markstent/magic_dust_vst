list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(md_plugin_tests plugin/test_processor.cpp)
target_link_libraries(md_plugin_tests PRIVATE MagicDust Catch2::Catch2WithMain)
md_target_defaults(md_plugin_tests)
catch_discover_tests(md_plugin_tests PROPERTIES LABELS plugin)

add_executable(md_engine_tests
  engine/test_theory.cpp
  engine/test_pcg32.cpp
  engine/test_ideacode.cpp)
target_link_libraries(md_engine_tests PRIVATE md_engine Catch2::Catch2WithMain)
md_target_defaults(md_engine_tests)
catch_discover_tests(md_engine_tests PROPERTIES LABELS unit)

add_executable(md_bank_tests bank/test_bank.cpp)
target_link_libraries(md_bank_tests PRIVATE md_bank Catch2::Catch2WithMain)
target_compile_definitions(md_bank_tests PRIVATE
  MD_BANK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/bank/fixtures")
md_target_defaults(md_bank_tests)
catch_discover_tests(md_bank_tests PROPERTIES LABELS unit)

add_executable(md_midiexport_tests midiexport/test_midifile.cpp)
target_link_libraries(md_midiexport_tests PRIVATE md_midiexport Catch2::Catch2WithMain)
md_target_defaults(md_midiexport_tests)
catch_discover_tests(md_midiexport_tests PROPERTIES LABELS unit)
