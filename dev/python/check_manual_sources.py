#!/usr/bin/env python3
"""Check the manual's supported TeX source conventions without building a PDF."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


LISTING = re.compile(
    r"(\\begin\{(lstlisting|commandexample)\}[^\n]*\n)"
    r"(.*?)(\\end\{\2\})", re.DOTALL)


def source_structure(text: str) -> str:
    """Ignore verbatim bodies and comments, retaining listing boundaries."""
    text = LISTING.sub(
        lambda match: match[1] + "\n" * match[3].count("\n") + match[4], text)
    return re.sub(r"(?<!\\)%[^\n]*", "", text)


def check_manual(repository: Path) -> tuple[list[str], dict[str, int]]:
    repository = repository.resolve()
    directory = repository / "doc/usermanual"
    errors: list[str] = []
    labels: dict[str, str] = {}
    references: list[tuple[str, str]] = []
    counts = {"files": 0, "labels": 0, "references": 0, "examples": 0}
    files = sorted(directory.rglob("*.tex"))
    sources = {path.resolve() for path in files}
    inputs: dict[Path, list[Path]] = {path: [] for path in sources}
    if not (directory / "usermanual.tex").is_file():
        errors.append("doc/usermanual/usermanual.tex: missing main document")
    for path in files:
        name = str(path.relative_to(repository))
        counts["files"] += 1
        try:
            raw = path.read_bytes()
            text = raw.decode("utf-8")
        except (OSError, UnicodeError) as error:
            errors.append(f"{name}: cannot read UTF-8 source: {error}")
            continue
        if b"\r" in raw:
            errors.append(f"{name}: use LF line endings")
        # The published manual must not depend on internal contributor notes.
        # Keep listing bodies here: references printed in examples count too.
        published = re.sub(r"(?<!\\)%[^\n]*", "", text)
        if re.search(r"\b(?:doc/impl|impl/doc)(?:/|\b)|(?:\.\./)+impl/", published):
            errors.append(f"{name}: internal implementation documentation reference in user manual")
        counts["examples"] += len(re.findall(r"\\begin\{commandexample\}", text))
        for title in re.findall(
                r"\\begin\{commandexample\}(?:\[[^\n]*?\])?\{([^\n]*)\}", text):
            if re.search(r"(?<!\\)_", title):
                errors.append(f"{name}: unescaped underscore in example title")
        structure = source_structure(text)
        stack: list[str] = []
        for action, environment in re.findall(r"\\(begin|end)\{([^}]+)\}", structure):
            if action == "begin":
                stack.append(environment)
            elif not stack or stack.pop() != environment:
                errors.append(f"{name}: mismatched end of {environment}")
        if stack:
            errors.append(f"{name}: unclosed environments: {', '.join(stack)}")
        for label in re.findall(r"\\label\{([^}]+)\}", structure):
            if label in labels:
                errors.append(f"{name}: duplicate label {label} (also in {labels[label]})")
            else:
                labels[label] = name
        for first, second in re.findall(
                r"\\manualref\{([^}]+)\}|\\hyperref\[([^\]]+)\]", structure):
            reference = first or second
            if not reference.startswith("#"):
                references.append((name, reference))
        for included in re.findall(r"\\input\{([^}]+)\}", structure):
            target = directory / included
            if not target.suffix:
                target = target.with_suffix(".tex")
            if not target.is_file():
                errors.append(f"{name}: missing input {included}")
            elif target.resolve() not in sources:
                errors.append(f"{name}: input outside manual TeX sources: {included}")
            else:
                inputs[path.resolve()].append(target.resolve())
        for document in re.findall(r"\\path\{(doc/[^}]+)\}", structure):
            if not (repository / document).is_file():
                errors.append(f"{name}: missing repository document {document}")
    main_document = (directory / "usermanual.tex").resolve()
    reachable: set[Path] = set()
    pending = [main_document] if main_document in sources else []
    while pending:
        path = pending.pop()
        if path not in reachable:
            reachable.add(path)
            pending.extend(inputs[path])
    for path in sorted(sources - reachable):
        errors.append(f"{path.relative_to(repository)}: not included by main document")

    # Check all components, including accidentally disconnected source files.
    state: dict[Path, int] = {}
    for start in sorted(sources):
        traversal = [(start, False)]
        while traversal:
            path, leaving = traversal.pop()
            if leaving:
                state[path] = 2
            elif state.get(path, 0) == 0:
                state[path] = 1
                traversal.append((path, True))
                for target in reversed(inputs[path]):
                    if state.get(target, 0) == 1:
                        errors.append(
                            f"{path.relative_to(repository)}: input cycle to "
                            f"{target.relative_to(repository)}")
                    else:
                        traversal.append((target, False))
    for name, reference in references:
        if reference not in labels:
            errors.append(f"{name}: undefined reference {reference}")
    counts["labels"] = len(labels)
    counts["references"] = len(references)
    return errors, counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2],
                        help="repository root (default: this tool's source tree)")
    arguments = parser.parse_args()
    errors, counts = check_manual(arguments.root)
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print("Manual sources passed: " + ", ".join(
        f"{value} {key}" for key, value in counts.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
