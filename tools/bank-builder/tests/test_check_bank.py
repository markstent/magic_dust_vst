import json
import shutil
from pathlib import Path

import check_bank
import pytest

REPO = Path(__file__).resolve().parents[3]
FIXTURES = REPO / "tests" / "bank" / "fixtures"
INVALID = sorted((FIXTURES / "invalid").glob("*.json"))


def test_valid_fixtures_pass(capsys: pytest.CaptureFixture[str]) -> None:
    assert check_bank.main([str(FIXTURES / "valid")]) == 0
    assert capsys.readouterr().out == ""


def test_bundled_entries_pass() -> None:
    assert check_bank.main([str(REPO / "bank" / "entries")]) == 0


@pytest.mark.parametrize("fixture", INVALID, ids=lambda p: p.stem)
def test_invalid_fixture_fails_and_names_the_file(
    fixture: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    assert check_bank.main([str(fixture)]) == 1
    assert capsys.readouterr().out.startswith(fixture.name)


def test_there_are_ten_invalid_fixtures() -> None:
    assert len(INVALID) >= 10


def test_duplicate_ids_fail(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    source = FIXTURES / "valid" / "mk-dub-starter-01.json"
    shutil.copy(source, tmp_path / "a.json")
    shutil.copy(source, tmp_path / "b.json")
    assert check_bank.main([str(tmp_path)]) == 1
    assert "b.json: /id: duplicate of a.json" in capsys.readouterr().out


def test_invalid_json_fails(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    (tmp_path / "broken.json").write_text("{")
    assert check_bank.main([str(tmp_path)]) == 1
    assert "broken.json: not valid JSON" in capsys.readouterr().out


def test_empty_directory_fails(tmp_path: Path) -> None:
    assert check_bank.main([str(tmp_path)]) == 1


@pytest.mark.parametrize("degree", ["iadd9", "I11", "imaj7", "viii7"])
def test_degree_rules_the_schema_cannot_express(
    degree: str, tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    entry = json.loads((FIXTURES / "valid" / "mk-dub-starter-01.json").read_text())
    entry["progressions"][0]["degrees"][1] = degree
    (tmp_path / "entry.json").write_text(json.dumps(entry))
    assert check_bank.main([str(tmp_path)]) == 1
    assert "entry.json" in capsys.readouterr().out


def test_tempo_min_above_max_fails(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    entry = json.loads((FIXTURES / "valid" / "mk-dub-starter-01.json").read_text())
    entry["tempo"] = {"min": 130, "max": 120}
    (tmp_path / "entry.json").write_text(json.dumps(entry))
    assert check_bank.main([str(tmp_path)]) == 1
    assert "/tempo: min is above max" in capsys.readouterr().out
