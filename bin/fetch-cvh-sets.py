#!/usr/bin/env python3
"""Download source CSV packs locally, without adding third-party cards to Git."""

import argparse
import csv
import io
import hashlib
import json
import os
import tempfile
from pathlib import Path
from urllib.parse import urlsplit
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "docs" / "cardpacks" / "cvh_sources.json"
OUTPUT = ROOT / "local-cardpacks" / "cards-vs-humanity"
MAX_DOWNLOAD_SIZE = 5 * 1024 * 1024
ALLOWED_HOSTS = {"www.cards-vs-humanity.de", "cards-vs-humanity.de"}
CSV_CONTENT_TYPES = {"application/csv", "text/csv", "text/plain", "application/octet-stream"}


def read_catalog():
    with CATALOG.open(encoding="utf-8") as source:
        return json.load(source)["packs"]


def validate_source_url(url):
    parts = urlsplit(url)
    if parts.scheme != "https" or parts.hostname not in ALLOWED_HOSTS:
        raise ValueError("CSV must be downloaded from the published HTTPS source")
    if not parts.path.startswith("/csv/"):
        raise ValueError("Unexpected CVH download path")


def validate_csv(contents, expected_black, expected_white):
    try:
        decoded = contents.decode("utf-8-sig")
        reader = csv.reader(io.StringIO(decoded, newline=""), strict=True)
        if next(reader, None) != ["typ", "text"]:
            raise ValueError("Unexpected CSV header (expected typ,text)")
        counts = {"b": 0, "w": 0}
        for row in reader:
            if len(row) != 2 or row[0] not in counts or not row[1].strip():
                raise ValueError("Invalid card type or empty card text")
            counts[row[0]] += 1
    except (UnicodeDecodeError, csv.Error) as error:
        raise ValueError("Invalid UTF-8 CSV file") from error
    if counts != {"b": expected_black, "w": expected_white}:
        raise ValueError(
            f"Card counts changed: found {counts['b']} black and {counts['w']} white"
        )


def download_pack(pack, directory, force=False, opener=urlopen):
    url = pack["csv_url"]
    validate_source_url(url)

    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / (pack["id"] + ".csv")
    if destination.exists() and not force:
        return destination, "already present"

    request = Request(
        url,
        headers={"User-Agent": "CAW-pack-source-downloader/1.0", "Accept": "text/csv, */*"},
    )
    with opener(request, timeout=30) as response:
        validate_source_url(response.geturl())
        content_type = response.headers.get_content_type()
        if content_type not in CSV_CONTENT_TYPES:
            raise ValueError(f"Expected CSV download, got {content_type}")
        contents = response.read(MAX_DOWNLOAD_SIZE + 1)

    if not contents:
        raise ValueError("Empty CSV download")
    if len(contents) > MAX_DOWNLOAD_SIZE:
        raise ValueError("CSV download exceeds the 5 MiB per-pack limit")
    prefix = contents.lstrip(b"\xef\xbb\xbf \r\n\t").lower()
    if prefix.startswith((b"<html", b"<!doctype html", b"%pdf")):
        raise ValueError("Source returned HTML or PDF rather than CSV")

    validate_csv(contents, pack['black_count'], pack['white_count'])

    # Write only after validation; a failed download never corrupts an existing file.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=directory, suffix=".part", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(contents)
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)

    digest = hashlib.sha256(contents).hexdigest()
    return destination, f"{len(contents)} bytes, SHA-256 {digest}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="List all known source sets")
    parser.add_argument("--set", dest="selected", action="append", help="Download this set ID")
    parser.add_argument("--force", action="store_true", help="Overwrite existing local CSVs")
    args = parser.parse_args()

    packs = read_catalog()
    if args.list:
        for pack in packs:
            print(f"{pack['id']:13} {pack['black_count']:3} black / "
                  f"{pack['white_count']:3} white  {pack['title']}")
        return 0

    available = {pack["id"]: pack for pack in packs}
    chosen = args.selected or list(available)
    for pack_id in chosen:
        if pack_id not in available:
            parser.error(f"Unknown set ID: {pack_id}")

    failures = 0
    for pack_id in chosen:
        try:
            location, result = download_pack(available[pack_id], OUTPUT, args.force)
            print(f"{pack_id}: {result} ({location})")
        except (OSError, ValueError) as error:
            failures += 1
            print(f"{pack_id}: download failed: {error}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
