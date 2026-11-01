"""Synthetic-only tests for the optional Prior99 converter."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "bin" / "convert-prior99-cards.py"
SPEC = importlib.util.spec_from_file_location("prior99_converter", SCRIPT)
converter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(converter)


class ConverterTests(unittest.TestCase):
    def test_parses_pick_counts_and_tex_escapes(self):
        latex = r"""
        % \whitecard{Test}{COMMENTED OUT}
        \whitecard{Test}{Beispiel mit \&-Zeichen.} % ignored
        \blackcard{2}{Test}{Erster \underl und zweiter \underl.}
        \blackcard{1}{Test}{Noch eine Frage ohne Lücke?}
        \blackcard{3}{Test}{\underl, \underl und \underl.}
        """
        self.assertEqual(converter.parse_cards(latex), [
            ("w", "Beispiel mit &-Zeichen."),
            ("b", "Erster ___ und zweiter ___."),
            ("b", "Noch eine Frage ohne Lücke?"),
            ("b", "___, ___ und ___."),
        ])

    def test_rejects_broken_or_unsupported_cards(self):
        for source in (
            r"\blackcard{2}{Test}{Nur \underl.}",
            r"\blackcard{4}{Test}{\underl \underl \underl \underl}",
            r"\whitecard{Test}{Eine \textbf{unbekannte} Funktion}",
            r"\whitecard{Test}{",
            "\\whitecard{Test}{" + "x" * 301 + "}",
        ):
            with self.subTest(source=source[:35]):
                with self.assertRaises(converter.ConversionError):
                    converter.parse_cards(source)

    def test_import_remains_local_and_manifest_has_credits(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "upstream"
            (source / "categories").mkdir(parents=True)
            for category in converter.CATEGORIES:
                (source / "categories" / (category + ".tex")).write_text(
                    rf"\blackcard{{1}}{{Test}}{{Frage {category}: \underl.}}" + "\n" +
                    rf"\whitecard{{Test}}{{Antwort {category}}}" + "\n", encoding="utf-8"
                )
            cards, counts = converter.read_source(source)
            self.assertEqual(len(counts), 11)
            self.assertEqual(len(cards), 22)
            destination = Path(directory) / "local-cardpacks" / converter.PACK_ID
            self.assertFalse(destination.exists())
            converter.write_pack(destination, cards, "TEST-LICENSE", "Synthetic Test")
            manifest = json.loads((destination / "packs.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["packs"][0]["rightsHolder"], "Synthetic Test")
            self.assertEqual(manifest["packs"][0]["title"], "NSFW")
            self.assertEqual(manifest["packs"][0]["license"], "TEST-LICENSE")
            self.assertIs(manifest["packs"][0]["defaultSelected"], False)
            self.assertIn("typ,text", (destination / "cards.csv").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
