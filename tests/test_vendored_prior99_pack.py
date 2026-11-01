"""Structural checks for the imported Prior99 deck; card wording isn't copied into tests."""

import csv
import json
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "assets" / "cardpacks" / "prior99-fullformat-german"
SOURCE_REVISION = "92501b245f7002ac3d3875099d859a5579aa2161"


class Prior99PackTests(unittest.TestCase):
    def test_inventory_matches_pinned_source_and_caw_format(self):
        with (PACK / "cards.csv").open(encoding="utf-8", newline="") as source:
            reader = csv.reader(source, strict=True)
            self.assertEqual(next(reader), ["typ", "text"])
            rows = list(reader)

        self.assertEqual(len(rows), 717)
        self.assertTrue(all(len(row) == 2 and row[0] in {"b", "w"} for row in rows))
        self.assertEqual(Counter(row[0] for row in rows), {"b": 107, "w": 610})
        self.assertEqual(len(set(tuple(row) for row in rows)), len(rows))
        for kind, value in rows:
            self.assertTrue(value)
            self.assertLessEqual(len(value.encode("utf-8")), 300)
            self.assertTrue(all(ord(letter) >= 32 for letter in value))
            if kind == "b":
                self.assertLessEqual(value.count("___"), 3)
        blanks = Counter(value.count("___") for kind, value in rows if kind == "b")
        self.assertEqual(blanks, {0: 37, 1: 55, 2: 13, 3: 2})

    def test_manifest_is_credited_and_not_default_selected(self):
        manifest = json.loads((PACK / "packs.json").read_text(encoding="utf-8"))
        self.assertEqual(len(manifest["packs"]), 1)
        pack = manifest["packs"][0]
        self.assertEqual(pack["id"], "prior99-fullformat-german")
        self.assertEqual(pack["file"], "cards.csv")
        self.assertEqual(pack["language"], "de")
        self.assertIs(pack["defaultSelected"], False)
        self.assertIn(SOURCE_REVISION, pack["sourceUrl"])
        self.assertIn("Prior99", pack["rightsHolder"])
        self.assertIn("CC BY-NC-SA 2.0", pack["license"])
        self.assertIn("GPL-3.0-or-later", pack["license"])
        attribution = (PACK / "ATTRIBUTION.md").read_text(encoding="utf-8")
        self.assertIn(SOURCE_REVISION, attribution)
        self.assertIn("Cards Against Humanity", attribution)
        self.assertIn("Prior99", attribution)


if __name__ == "__main__":
    unittest.main()
