#!/usr/bin/env python3
"""Pack a small menu bootstrap, the first chamber, and later chambers."""

import hashlib
import json
import shutil
import sys
from pathlib import Path


def package_for(relative: Path) -> str:
    parts = relative.parts
    if len(parts) == 2 and parts[0] == "backgrounds":
        if relative.stem == "menu":
            return "startup"
        if relative.stem in {f"level_{n}" for n in range(2, 8)}:
            return "deferred"
    if len(parts) == 3 and parts[:2] == ("ui", "level_intros"):
        if relative.stem in {f"level_intro_{n}" for n in range(2, 8)}:
            return "deferred"
    if parts[0] == "fonts":
        return "startup"
    if len(parts) == 2 and parts[0] == "ui" and relative.stem in {
        "logo", "button", "level_select"
    }:
        return "startup"
    if len(parts) == 2 and parts[0] == "sounds" and relative.stem in {
        "menu_theme", "ui_soft_chime"
    }:
        return "startup"
    return "core"


def write_package(name: str, paths: list[Path], source: Path, release: Path) -> int:
    entries = []
    digest = hashlib.sha256()
    offset = 0
    with (release / f"{name}.data").open("wb") as archive:
        for path in paths:
            relative = path.relative_to(source)
            contents = path.read_bytes()
            archive.write(contents)
            digest.update(contents)
            entries.append({
                "path": "/assets/" + relative.as_posix(),
                "start": offset,
                "end": offset + len(contents),
            })
            offset += len(contents)
    metadata = {"sha256": digest.hexdigest(), "size": offset, "files": entries}
    (release / f"{name}.json").write_text(
        json.dumps(metadata, separators=(",", ":")) + "\n", encoding="utf-8"
    )
    return offset


def main() -> None:
    source, startup, release = (Path(arg) for arg in sys.argv[1:4])
    shutil.rmtree(startup, ignore_errors=True)
    startup.mkdir(parents=True)
    release.mkdir(parents=True, exist_ok=True)

    groups = {"startup": [], "core": [], "deferred": []}
    for path in sorted(p for p in source.rglob("*") if p.is_file()):
        groups[package_for(path.relative_to(source))].append(path)

    if len(groups["deferred"]) != 12:
        raise SystemExit(f"expected 12 deferred level images, found {len(groups['deferred'])}")

    for path in groups["startup"]:
        relative = path.relative_to(source)
        destination = startup / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)

    startup_bytes = sum(p.stat().st_size for p in groups["startup"])
    core_bytes = write_package("core", groups["core"], source, release)
    deferred_bytes = write_package("deferred", groups["deferred"], source, release)
    print(f"  menu startup assets: {startup_bytes / 1048576:.2f} MiB")
    print(f"  first chamber and extras: {core_bytes / 1048576:.2f} MiB")
    print(f"  later chamber artwork: {deferred_bytes / 1048576:.2f} MiB")


if __name__ == "__main__":
    main()
