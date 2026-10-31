set(MD_CLANG_TIDY_EXE /opt/homebrew/opt/llvm/bin/clang-tidy CACHE FILEPATH "clang-tidy executable")

function(md_target_defaults target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(MD_CLANG_TIDY)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY ${MD_CLANG_TIDY_EXE})
  endif()
endfunction()
