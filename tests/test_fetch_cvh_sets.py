"""Offline tests for the external-source CSV downloader (synthetic contents only)."""

import importlib.util
import io
import json
import tempfile
import unittest
from email.message import Message
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "fetch-cvh-sets.py"
spec = importlib.util.spec_from_file_location("fetch_cvh_sets", SCRIPT)
downloader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(downloader)


class FakeResponse(io.BytesIO):
    def __init__(self, contents, url, content_type="application/csv"):
        super().__init__(contents)
        self.url = url
        self.headers = Message()
        self.headers.add_header("Content-Type", content_type)

    def geturl(self):
        return self.url


class FetchSetsTests(unittest.TestCase):
    def setUp(self):
        self.pack = downloader.read_catalog()[0]

    def test_catalog_has_six_metadata_entries_without_cards(self):
        packs = downloader.read_catalog()
        self.assertEqual(len(packs), 6)
        self.assertEqual(sum(pack["black_count"] for pack in packs), 585)
        self.assertEqual(sum(pack["white_count"] for pack in packs), 1179)
        self.assertEqual(len({pack["id"] for pack in packs}), 6)
        for pack in packs:
            downloader.validate_source_url(pack["csv_url"])
            self.assertNotIn("cards", pack)

    def test_reject_external_redirect(self):
        with self.assertRaises(ValueError):
            downloader.validate_source_url("https://example.org/csv/other/")

    def test_download_preserves_synthetic_csv_bytes(self):
        data = b"type;text\nblack;Test question ___\nwhite;Test response\n"
        def opener(request, timeout):
            self.assertEqual(timeout, 30)
            return FakeResponse(data, request.full_url)
        with tempfile.TemporaryDirectory() as folder:
            destination, summary = downloader.download_pack(
                self.pack, Path(folder), opener=opener
            )
            self.assertEqual(destination.read_bytes(), data)
            self.assertIn("SHA-256", summary)
            self.assertEqual(destination.suffix, ".csv")

    def test_reject_html_without_writing_a_file(self):
        def opener(request, timeout):
            return FakeResponse(b"<html>Unexpected response</html>", request.full_url)
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError):
                downloader.download_pack(self.pack, Path(folder), opener=opener)
            self.assertEqual(list(Path(folder).iterdir()), [])

    def test_keep_existing_file_without_force(self):
        with tempfile.TemporaryDirectory() as folder:
            destination = Path(folder) / (self.pack["id"] + ".csv")
            destination.write_bytes(b"old fixture")
            def unexpected_network(request, timeout):
                self.fail("Existing source CSV must not be downloaded again")
            actual, status = downloader.download_pack(
                self.pack, Path(folder), opener=unexpected_network
            )
            self.assertEqual(actual, destination)
            self.assertEqual(status, "already present")
            self.assertEqual(destination.read_bytes(), b"old fixture")

    def test_failed_forced_update_does_not_truncate_existing_file(self):
        with tempfile.TemporaryDirectory() as folder:
            destination = Path(folder) / (self.pack["id"] + ".csv")
            destination.write_bytes(b"existing fixture")
            def opener(request, timeout):
                return FakeResponse(b"", request.full_url)
            with self.assertRaises(ValueError):
                downloader.download_pack(self.pack, Path(folder), force=True, opener=opener)
            self.assertEqual(destination.read_bytes(), b"existing fixture")


if __name__ == "__main__":
    unittest.main()
