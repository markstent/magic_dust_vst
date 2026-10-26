#include "cases.hpp"

#include "md/bank.hpp"
#include "md/generate.hpp"
#include "md/midifile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace md;

namespace
{
std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary);
    std::ranges::copy(bytes, std::ostreambuf_iterator<char>(file));
    file.close();
    REQUIRE(file);
}
} // namespace

TEST_CASE("golden: generated MIDI matches the approved files", "[golden]")
{
    const Bank bank = loadBundledBank();
    const std::filesystem::path folder = MD_GOLDEN_DIR;
    const char* update = std::getenv("MD_UPDATE_GOLDEN");
    const bool updating = update != nullptr && std::string(update) == "1";

    for (int i = 0; i < golden::kCaseCount; ++i)
    {
        const golden::Case c = golden::makeCase(i);
        const Idea idea = generate(bank, c.settings, c.seed);
        const auto bytes =
            writeMidiFile(ideaTracks(idea), golden::kTempoBpm, ideaLengthTicks(idea));
        const auto path = folder / c.fileName;

        INFO(c.fileName);
        if (updating)
        {
            writeFile(path, bytes);
            continue;
        }
        INFO("missing or different golden file; the owner runs scripts/update-golden.sh to review "
             "and approve a change");
        REQUIRE(std::filesystem::exists(path));
        REQUIRE(readFile(path) == bytes);
    }
}
