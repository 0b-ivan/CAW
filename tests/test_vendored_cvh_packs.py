"""Validate pinned, attributed third-party CSV sources without copying card text.

The originals in assets/ are CC BY-NC-SA 2.0, not test fixtures.
"""

import csv
import hashlib
import json
import unittest
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "cardpacks" / "cvh_sources.json"
PACKS = ROOT / "assets" / "cardpacks" / "cards-vs-humanity"


class VendoredCsvTests(unittest.TestCase):
    def test_all_originals_match_manifest_and_catalog(self):
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))["packs"]
        manifest = {}
        for line in (PACKS / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
            checksum, filename = line.split(maxsplit=1)
            manifest[filename.strip()] = checksum

        expected_files = {pack["id"] + ".csv" for pack in catalog}
        self.assertEqual(set(manifest), expected_files)
        self.assertEqual({file.name for file in PACKS.glob("*.csv")}, expected_files)

        totals = Counter()
        for pack in catalog:
            filename = pack["id"] + ".csv"
            source = PACKS / filename
            contents = source.read_bytes()
            self.assertEqual(hashlib.sha256(contents).hexdigest(), manifest[filename])
            with source.open(encoding="utf-8-sig", newline="") as handle:
                reader = csv.DictReader(handle, strict=True)
                self.assertEqual(reader.fieldnames, ["typ", "text"])
                counts = Counter()
                for card in reader:
                    self.assertIn(card["typ"], ("b", "w"))
                    self.assertTrue(card["text"].strip())
                    counts[card["typ"]] += 1
            self.assertEqual(counts["b"], pack["black_count"])
            self.assertEqual(counts["w"], pack["white_count"])
            totals.update(counts)

        self.assertEqual(totals["b"], 585)
        self.assertEqual(totals["w"], 1179)

    def test_license_notice_and_sources_are_present(self):
        notice = (PACKS / "ATTRIBUTION.md").read_text(encoding="utf-8")
        self.assertIn("CC BY-NC-SA 2.0", notice)
        self.assertIn("creativecommons.org/licenses/by-nc-sa/2.0/", notice)
        self.assertIn("www.cards-vs-humanity.de", notice)


if __name__ == "__main__":
    unittest.main()
