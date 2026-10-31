"""Check bank entries against bank/schema.json and the rules a schema cannot express.

Usage: check_bank.py <dir or file>...
Exits 1 and prints "<file>: <problem>" for each fault. The chord rule lives in the C++ loader.
"""

import json
import re
import sys
from pathlib import Path

from jsonschema import Draft202012Validator, validators

SCHEMA_PATH = Path(__file__).resolve().parents[2] / "bank" / "schema.json"
STEPS_PER_BAR = 16
ID = re.compile(r"[a-z0-9]+(-[a-z0-9]+)*")
LOWER_DEGREE = re.compile(r"(i|ii|iii|iv|v|vi|vii)(7|9|11)?")
UPPER_DEGREE = re.compile(r"(I|II|III|IV|V|VI|VII)(7|9|maj7|maj9|6|add9|sus4)?")


def strict_validator(schema: dict) -> Draft202012Validator:
    """Like Draft202012Validator, but 122.0 is not an integer (the C++ loader agrees)."""
    checker = Draft202012Validator.TYPE_CHECKER.redefine(
        "integer",
        lambda _, value: isinstance(value, int) and not isinstance(value, bool),
    )
    return validators.extend(Draft202012Validator, type_checker=checker)(schema)


def reject_constant(name: str) -> None:
    raise ValueError(f"{name} is not valid JSON")


def schema_problems(entry: dict, validator: Draft202012Validator) -> list[str]:
    errors = sorted(validator.iter_errors(entry), key=lambda e: list(e.absolute_path))
    return [f"/{'/'.join(map(str, e.absolute_path))}: {e.message}" for e in errors]


def degree_problems(progression: dict, path: str) -> list[str]:
    problems = []
    for degree in progression["degrees"]:
        if not (LOWER_DEGREE.fullmatch(degree) or UPPER_DEGREE.fullmatch(degree)):
            problems.append(f"{path}/degrees: not a chord degree: {degree}")
    bars = progression["barsPerChord"]
    if isinstance(bars, list) and len(bars) != len(progression["degrees"]):
        problems.append(f"{path}/barsPerChord: must have one entry per degree")
    return problems


def cell_problems(cells: list[dict], path: str) -> list[str]:
    problems = []
    seen: set[str] = set()
    for i, cell in enumerate(cells):
        if cell["id"] in seen:
            problems.append(f"{path}/{i}/id: duplicate rhythm cell id {cell['id']!r}")
        seen.add(cell["id"])
        for j, (step, length) in enumerate(cell["hits"]):
            if step + length > cell["bars"] * STEPS_PER_BAR:
                problems.append(
                    f"{path}/{i}/hits/{j}: the hit runs past the end of the cell"
                )
    return problems


def rule_problems(entry: dict) -> list[str]:
    problems = []
    if not ID.fullmatch(entry["id"]):
        problems.append("/id: must be lowercase words joined by hyphens")
    for i, progression in enumerate(entry["progressions"]):
        problems += degree_problems(progression, f"/progressions/{i}")
    problems += cell_problems(entry["chordRhythms"], "/chordRhythms")
    problems += cell_problems(entry["melodyRhythms"], "/melodyRhythms")
    if entry["tempo"]["min"] > entry["tempo"]["max"]:
        problems.append("/tempo: min is above max")
    low, high = entry["voicing"]["topNote"]
    if low > high:
        problems.append("/voicing/topNote: low is above high")
    return problems


def entry_problems(entry: dict, validator: Draft202012Validator) -> list[str]:
    return schema_problems(entry, validator) or rule_problems(entry)


def entry_files(paths: list[str]) -> list[Path]:
    files: list[Path] = []
    for path in map(Path, paths):
        files += sorted(path.glob("*.json")) if path.is_dir() else [path]
    return files


def main(argv: list[str]) -> int:
    validator = strict_validator(json.loads(SCHEMA_PATH.read_text()))
    files = entry_files(argv)
    if not files:
        print("no entries found")
        return 1
    failed = False
    seen: dict[str, str] = {}
    for file in files:
        try:
            entry = json.loads(file.read_text(), parse_constant=reject_constant)
        except ValueError as error:
            problems = [f"not valid JSON: {error}"]
        else:
            problems = entry_problems(entry, validator)
            entry_id = entry.get("id") if isinstance(entry, dict) else None
            if isinstance(entry_id, str) and entry_id in seen:
                problems.append(f"/id: duplicate of {seen[entry_id]}")
            elif isinstance(entry_id, str):
                seen[entry_id] = file.name
        for problem in problems:
            print(f"{file.name}: {problem}")
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
