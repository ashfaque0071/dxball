#!/usr/bin/env python3
"""Keep the first chamber in the startup bundle; pack later chambers separately."""

import hashlib
import json
import shutil
import sys
from pathlib import Path


def deferred(relative: Path) -> bool:
    parts = relative.parts
    if len(parts) == 2 and parts[0] == "backgrounds":
        return any(relative.stem == f"level_{n}" for n in range(2, 8))
    if len(parts) == 3 and parts[:2] == ("ui", "level_intros"):
        return any(relative.stem == f"level_intro_{n}" for n in range(2, 8))
    return False


def main() -> None:
    source, startup, release = (Path(arg) for arg in sys.argv[1:4])
    shutil.rmtree(startup, ignore_errors=True)
    startup.mkdir(parents=True)
    release.mkdir(parents=True, exist_ok=True)

    entries = []
    digest = hashlib.sha256()
    offset = 0
    with (release / "deferred.data").open("wb") as archive:
        for path in sorted(p for p in source.rglob("*") if p.is_file()):
            relative = path.relative_to(source)
            if not deferred(relative):
                destination = startup / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
                continue

            contents = path.read_bytes()
            archive.write(contents)
            digest.update(contents)
            entries.append({
                "path": "/assets/" + relative.as_posix(),
                "start": offset,
                "end": offset + len(contents),
            })
            offset += len(contents)

    if len(entries) != 12:
        raise SystemExit(f"expected 12 deferred level images, found {len(entries)}")

    metadata = {"sha256": digest.hexdigest(), "size": offset, "files": entries}
    (release / "deferred.json").write_text(
        json.dumps(metadata, separators=(",", ":")) + "\n", encoding="utf-8"
    )

    startup_bytes = sum(p.stat().st_size for p in startup.rglob("*") if p.is_file())
    print(f"  startup assets: {startup_bytes / 1048576:.2f} MB")
    print(f"  background levels: {offset / 1048576:.2f} MB")


if __name__ == "__main__":
    main()
