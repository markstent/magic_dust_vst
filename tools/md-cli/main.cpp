#include "args.hpp"

#include "md/bank.hpp"
#include "md/generate.hpp"
#include "md/ideacode.hpp"
#include "md/midifile.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr std::string_view kUsage =
    "usage: md-cli --out <file.mid> [options]\n"
    "  --style    uk-funky, mk-dub, soulful, bassline, festival, ny-groove\n"
    "  --seed     0 to 99999 (default 48213)\n"
    "  --key      a, c#, bb ... (default a)\n"
    "  --bars     4, 8 or 16 (default 8)\n"
    "  --mood     0 to 100 (default 30)\n"
    "  --energy   0 to 100 (default 50)\n"
    "  --groove   straight, swing, uk-shuffle, heavy\n"
    "  --flavour  simple, soulful, jazzy\n"
    "  --tempo    60 to 200 (default 124)\n"
    "  --help     show this text\n";

void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary);
    std::ranges::copy(bytes, std::ostreambuf_iterator<char>(file));
    file.close();
    if (!file)
    {
        throw std::runtime_error("could not write " + path.string());
    }
}

std::string keyName(const md::Key& key)
{
    constexpr std::array<std::string_view, 12> notes{
        "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b",
    };
    constexpr std::array<std::string_view, 5> modes{
        "phrygian", "aeolian", "dorian", "mixolydian", "ionian",
    };
    return std::string(notes.at(static_cast<std::size_t>(key.root))) + " " +
           std::string(modes.at(static_cast<std::size_t>(key.mode)));
}

int run(const md::CliOptions& options)
{
    const auto idea = md::generate(md::loadBundledBank(), options.settings, options.seed);
    const int ticks = md::ideaLengthTicks(idea);

    const auto stem = options.out.stem().string();
    const auto folder = options.out.parent_path();
    if (!folder.empty())
    {
        std::filesystem::create_directories(folder);
    }

    const std::array<std::pair<std::filesystem::path, std::vector<md::MidiTrack>>, 4> files{
        std::pair{options.out, md::ideaTracks(idea)},
        std::pair{folder / (stem + " - chords.mid"),
                  std::vector{md::laneTrack(idea, md::LaneId::chords)}},
        std::pair{folder / (stem + " - bass.mid"),
                  std::vector{md::laneTrack(idea, md::LaneId::bass)}},
        std::pair{folder / (stem + " - melody.mid"),
                  std::vector{md::laneTrack(idea, md::LaneId::melody)}},
    };
    for (const auto& [path, tracks] : files)
    {
        writeFile(path, md::writeMidiFile(tracks, options.tempo, ticks));
    }

    std::string chords;
    for (const auto& chord : idea.progression)
    {
        chords += (chords.empty() ? "" : " | ") + md::chordName(idea.key, chord.symbol) + " " +
                  chord.feel;
    }
    std::cout << "spell " << idea.seed << "\n"
              << "code " << md::encodeIdea(idea.seed, idea.settings, idea.bankVersion) << "\n"
              << "key " << keyName(idea.key) << "\n"
              << chords << "\n";
    for (const auto& file : files)
    {
        std::cout << file.first.string() << "\n";
    }
    return 0;
}

int mainImpl(std::span<char*> argv)
{
    std::vector<std::string_view> args;
    for (const char* arg : argv.subspan(1))
    {
        args.emplace_back(arg);
    }
    if (args.size() == 1 && args.front() == "--help")
    {
        std::cout << kUsage;
        return 0;
    }
    const auto parsed = md::parseArgs(args);
    if (const auto* error = std::get_if<std::string>(&parsed))
    {
        std::cerr << "md-cli: " << *error << "\n" << kUsage;
        return 2;
    }
    try
    {
        return run(std::get<md::CliOptions>(parsed));
    }
    catch (const std::exception& e)
    {
        std::cerr << "md-cli: " << e.what() << "\n";
        return 1;
    }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        return mainImpl({argv, static_cast<std::size_t>(argc)});
    }
    catch (...)
    {
        return 1;
    }
}
