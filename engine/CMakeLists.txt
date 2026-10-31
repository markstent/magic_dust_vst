add_library(md_engine STATIC
  src/theory.cpp
  src/names.cpp
  src/pcg32.cpp
  src/ideacode.cpp
  src/chords.cpp
  src/bass.cpp
  src/melody.cpp
  src/validate.cpp
  src/refit.cpp
  src/generate.cpp
  src/invent.cpp)
target_include_directories(md_engine PUBLIC include)
target_compile_options(md_engine PRIVATE -ffp-contract=off)
md_target_defaults(md_engine)
