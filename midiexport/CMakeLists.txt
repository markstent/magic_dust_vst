add_library(md_midiexport STATIC src/midifile.cpp)
target_include_directories(md_midiexport PUBLIC include)
target_link_libraries(md_midiexport PUBLIC md_engine)
md_target_defaults(md_midiexport)
