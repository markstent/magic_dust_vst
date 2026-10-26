list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(md_plugin_tests plugin/test_processor.cpp)
target_link_libraries(md_plugin_tests PRIVATE MagicDust Catch2::Catch2WithMain)
md_target_defaults(md_plugin_tests)
catch_discover_tests(md_plugin_tests PROPERTIES LABELS plugin)
