#!/usr/bin/env python3
"""Focused checks for the source-only manual validator; no TeX or emulator."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_manual_sources", ROOT / "dev/python/check_manual_sources.py")
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ManualSourceTests(unittest.TestCase):
    def check_fixture(self, text, extra=None):
        with tempfile.TemporaryDirectory(prefix="sim4000-manual-source-") as temporary:
            root = Path(temporary)
            directory = root / "doc/usermanual"
            directory.mkdir(parents=True)
            (directory / "usermanual.tex").write_bytes(text.encode("utf-8"))
            for name, content in (extra or {}).items():
                (directory / name).write_text(content, encoding="utf-8")
            return TOOL.check_manual(root)

    def test_current_manual(self):
        errors, counts = TOOL.check_manual(ROOT)
        self.assertEqual(errors, [])
        self.assertGreater(counts["examples"], 0)

    def test_internal_documentation_is_not_part_of_user_manual(self):
        for reference in ("\\path{doc/impl/DISK_WORKFLOW.md}", "impl/doc/notes.txt",
                          "\\href{../impl/DEVELOPMENT.md}{Development}",
                          "\\begin{commandexample}{Example}\ndoc/impl/notes.md\n\\end{commandexample}"):
            with self.subTest(reference=reference):
                errors, _ = self.check_fixture(reference + "\n")
                self.assertTrue(any("internal implementation documentation reference" in error
                                    for error in errors))
        errors, _ = self.check_fixture("% Maintainer note: doc/impl/DEVELOPMENT.md\nText\n")
        self.assertEqual(errors, [])

    def test_valid_structure_and_ignored_text(self):
        errors, counts = self.check_fixture(
            "\\input{chapter}\n% \\label{known}\n"
            "\\manualref{known}{Procedure}\n"
            "\\begin{commandexample}{File contents: sample\\_file}\n"
            "\\label{known} \\input{missing} \\end{wrong}\n"
            "\\end{commandexample}\n"
            "\\newcommand{\\manualref}[2]{\\hyperref[#1]{#2}}\n",
            {"chapter.tex": "\\label{known}\n"})
        self.assertEqual(errors, [])
        self.assertEqual(counts["labels"], 1)
        self.assertEqual(counts["references"], 1)

    def test_duplicate_and_undefined_labels(self):
        errors, _ = self.check_fixture(
            "\\label{same}\\label{same}\\hyperref[absent]{Missing}\n")
        self.assertTrue(any("duplicate label" in error for error in errors))
        self.assertTrue(any("undefined reference" in error for error in errors))

    def test_missing_files(self):
        errors, _ = self.check_fixture(
            "\\input{missing}\\path{doc/missing.md}\n")
        self.assertEqual(len(errors), 2)

    def test_line_endings_and_example_title(self):
        errors, _ = self.check_fixture(
            "\\begin{commandexample}{sample_file}\r\n"
            "text\r\n\\end{commandexample}\r\n")
        self.assertTrue(any("LF line endings" in error for error in errors))
        self.assertTrue(any("unescaped underscore" in error for error in errors))

    def test_environment_mismatch(self):
        errors, _ = self.check_fixture("\\begin{itemize}\\end{enumerate}\n")
        self.assertTrue(any("mismatched" in error for error in errors))

    def test_unclosed_listing(self):
        errors, _ = self.check_fixture("\\begin{commandexample}{Input}\ntext\n")
        self.assertTrue(any("unclosed" in error for error in errors))

    def test_missing_main_document(self):
        with tempfile.TemporaryDirectory() as temporary:
            errors, _ = TOOL.check_manual(Path(temporary))
            self.assertTrue(any("missing main document" in error for error in errors))

    def test_unreferenced_chapter(self):
        errors, _ = self.check_fixture(
            "\\manualref{orphan}{Not in the PDF}\n",
            {"orphan.tex": "\\label{orphan}\n"})
        self.assertTrue(any("not included" in error for error in errors))

    def test_nested_inputs_with_explicit_extension(self):
        errors, _ = self.check_fixture(
            "\\input{outer.tex}\n",
            {"outer.tex": "\\input{inner}\n", "inner.tex": "Text\n"})
        self.assertEqual(errors, [])

    def test_input_cycle(self):
        errors, _ = self.check_fixture(
            "\\input{outer}\n",
            {"outer.tex": "\\input{inner}\n", "inner.tex": "\\input{outer}\n"})
        self.assertTrue(any("input cycle" in error for error in errors))

    def test_self_input_cycle(self):
        errors, _ = self.check_fixture("\\input{usermanual}\n")
        self.assertTrue(any("input cycle" in error for error in errors))

    def test_disconnected_input_cycle(self):
        errors, _ = self.check_fixture(
            "Main document\n", {"orphan.tex": "\\input{orphan}\n"})
        self.assertTrue(any("not included" in error for error in errors))
        self.assertTrue(any("input cycle" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
