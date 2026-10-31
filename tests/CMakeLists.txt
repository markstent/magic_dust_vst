list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(md_plugin_tests plugin/test_processor.cpp plugin/test_playback.cpp
  plugin/test_audition.cpp plugin/test_instruments.cpp plugin/test_sender.cpp
  plugin/test_editor.cpp plugin/test_dragfiles.cpp plugin/test_idea_state.cpp plugin/AllocationGuard.cpp)
target_link_libraries(md_plugin_tests PRIVATE MagicDust Catch2::Catch2WithMain)
md_target_defaults(md_plugin_tests)
catch_discover_tests(md_plugin_tests PROPERTIES LABELS plugin)

add_executable(md_engine_tests
  engine/test_theory.cpp
  engine/test_names.cpp
  engine/test_pcg32.cpp
  engine/test_ideacode.cpp
  engine/test_validate.cpp
  engine/test_generate.cpp
  engine/test_refit.cpp
  engine/test_pins.cpp
  engine/test_properties.cpp
  engine/test_variety.cpp
  engine/test_invent.cpp
  engine/test_sprinkle.cpp)
target_link_libraries(md_engine_tests PRIVATE md_bank md_midiexport Catch2::Catch2WithMain)
target_include_directories(md_engine_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src)
if(MD_PERF_ASSERT)
  target_compile_definitions(md_engine_tests PRIVATE MD_PERF_ASSERT)
endif()
md_target_defaults(md_engine_tests)
catch_discover_tests(md_engine_tests PROPERTIES LABELS unit)

add_executable(md_bank_tests bank/test_bank.cpp)
target_link_libraries(md_bank_tests PRIVATE md_bank Catch2::Catch2WithMain)
target_compile_definitions(md_bank_tests PRIVATE
  MD_BANK_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/bank/fixtures")
md_target_defaults(md_bank_tests)
catch_discover_tests(md_bank_tests PROPERTIES LABELS unit)

add_executable(md_midiexport_tests midiexport/test_midifile.cpp)
target_link_libraries(md_midiexport_tests PRIVATE md_bank md_midiexport Catch2::Catch2WithMain)
md_target_defaults(md_midiexport_tests)
catch_discover_tests(md_midiexport_tests PROPERTIES LABELS unit)

add_executable(md_cli_tests cli/test_args.cpp)
target_link_libraries(md_cli_tests PRIVATE md_cli_args Catch2::Catch2WithMain)
md_target_defaults(md_cli_tests)
catch_discover_tests(md_cli_tests PROPERTIES LABELS unit)

# Each test gets its own folder, ${MD_CLI_TMP}/<name>, so they are safe in parallel.
set(MD_CLI_TMP ${CMAKE_BINARY_DIR}/md-cli-test)
function(md_cli_test name)
  cmake_parse_arguments(T "" "EXPECT;STDOUT;STDERR;CHECK_FILES;BLOCKER" "ARGS" ${ARGN})
  add_test(NAME md_cli_${name}
    COMMAND ${CMAKE_COMMAND}
      -DCLI=$<TARGET_FILE:md-cli> -DDIR=${MD_CLI_TMP}/${name} -DEXPECT=${T_EXPECT}
      "-DARGS=${T_ARGS}" "-DSTDOUT=${T_STDOUT}" "-DSTDERR=${T_STDERR}"
      -DCHECK_FILES=${T_CHECK_FILES} -DBLOCKER=${T_BLOCKER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cli/run_cli.cmake)
  set_tests_properties(md_cli_${name} PROPERTIES LABELS unit)
endfunction()

md_cli_test(writes_four_files EXPECT 0 CHECK_FILES 1
  ARGS --seed 48213 --out ${MD_CLI_TMP}/writes_four_files/idea.mid)
md_cli_test(rejects_bad_bars EXPECT 2 ARGS --bars 5 --out ${MD_CLI_TMP}/rejects_bad_bars/x.mid)
md_cli_test(help EXPECT 0 STDOUT "usage: md-cli" ARGS --help)
md_cli_test(help_anywhere EXPECT 0 STDOUT "usage: md-cli" ARGS --bars 5 --help)
md_cli_test(flat_key_name EXPECT 0 STDOUT "key bb " ARGS --key bb --out ${MD_CLI_TMP}/flat_key_name/bb.mid)
md_cli_test(write_failure EXPECT 1 BLOCKER 1 STDERR "blocker"
  ARGS --out ${MD_CLI_TMP}/write_failure/blocker/idea.mid)

add_executable(md_golden_tests golden/test_golden.cpp)
target_link_libraries(md_golden_tests PRIVATE md_bank md_midiexport Catch2::Catch2WithMain)
target_compile_definitions(md_golden_tests PRIVATE
  MD_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
md_target_defaults(md_golden_tests)
catch_discover_tests(md_golden_tests PROPERTIES LABELS unit)
