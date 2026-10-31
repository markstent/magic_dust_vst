include(${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake)

file(GLOB entries CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/entries/*.json)

add_library(md_bank STATIC src/bank.cpp)
target_include_directories(md_bank PUBLIC include)
target_link_libraries(md_bank PUBLIC md_engine PRIVATE nlohmann_json::nlohmann_json)
md_embed_files(md_bank md::embedded ${entries})
md_target_defaults(md_bank)
