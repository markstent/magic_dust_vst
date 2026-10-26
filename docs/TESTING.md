# Testing

## Setting up your Mac

These tools are already installed.
Homebrew provides cmake, ninja, llvm (clang-format and clang-tidy) and uv.
pluginval lives in `~/Applications`.

## Building

Run these from the project folder in Terminal.
The first build downloads JUCE and takes several minutes.
Later builds only rebuild what changed.

- `cmake --preset dev` sets up the everyday build.
- `cmake --build --preset dev` builds Magic Dust and copies it to your plug-in folders, so your daw can find it.
- `cmake --preset lint && cmake --build --preset lint` builds again with the strict code checker switched on.

## Running tests

- `ctest --preset dev` runs every automatic test and prints how many passed.
- `ctest --preset dev -L plugin` runs only the tests for the plug-in itself.

All tests should say "Passed".
Anything marked "Failed" is a bug to report.

## Checking the plugin

- `scripts/validate.sh` runs the industry checkers on the built plug-in.
  It runs pluginval on the VST3 and the AU, then Apple's `auval` on the AU.
  Every check should end in a pass.

If `auval` says it cannot find Magic Dust right after a new build, macOS may be remembering an old list of plug-ins.
Run `killall -9 AudioComponentRegistrar` and then run the script again.

## Trying it in Ableton

To be filled in by a later task.
