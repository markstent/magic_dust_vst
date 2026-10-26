add_library(md_cli_args STATIC args.cpp)
target_include_directories(md_cli_args PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(md_cli_args PUBLIC md_engine)
md_target_defaults(md_cli_args)

add_executable(md-cli main.cpp)
target_link_libraries(md-cli PRIVATE md_cli_args md_bank md_midiexport)
md_target_defaults(md-cli)
