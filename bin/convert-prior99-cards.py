#!/usr/bin/env python3
"""Convert a locally obtained Prior99 LaTeX source into an optional CAW pack.

Third-party cards are deliberately never downloaded or checked into CAW by this tool.
The operator must review redistribution rights before creating a playable manifest.
"""

import argparse
import csv
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATEGORIES = (
    "basis", "boese", "drogen", "eklig", "fsk16", "fsk18",
    "geschichte", "kultur", "politik", "religion", "werbung",
)
SOURCE_COMMIT = "92501b245f7002ac3d3875099d859a5579aa2161"
SOURCE_URL = "https://github.com/Prior99/cah-fullformat-german"
PACK_ID = "prior99-fullformat-german"
CARD_COMMAND = re.compile(r"\\(blackcard|whitecard)\s*(?=\{)")
TEX_COMMAND = re.compile(r"\\([A-Za-z]+)")


class ConversionError(ValueError):
    """Unsupported or malformed source content; fail instead of dropping cards."""


def strip_comments(source):
    # TeX comments end at newline. A literal \% must remain in the card text.
    return "\n".join(re.sub(r"(?<!\\)%.*$", "", line) for line in source.splitlines())


def read_group(source, position):
    if position >= len(source) or source[position] != "{":
        raise ConversionError("Expected an opening brace")
    depth = 0
    start = position
    for index in range(position, len(source)):
        character = source[index]
        if index > 0 and source[index - 1] == "\\":
            continue
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:index], index + 1
    raise ConversionError("Unclosed card argument")


def convert_text(source):
    text = re.sub(r"\\underl\b", " ___ ", source)
    text = re.sub(r"\\newline\b", " ", text)
    text = re.sub(r"\\SymbReg\b", "®", text)
    text = re.sub(r"\\([&%#$])", r"\1", text)
    unknown = TEX_COMMAND.findall(text)
    if unknown:
        raise ConversionError(f"Unsupported LaTeX command: {unknown[0]}")
    text = " ".join(text.replace("{", "").replace("}", "").split())
    text = re.sub(r"\s+([.,;:!?])", r"\1", text)
    if not text or len(text.encode("utf-8")) > 300:
        raise ConversionError("Empty or oversized card")
    if any(ord(character) < 32 for character in text):
        raise ConversionError("Control character in card")
    return text


def parse_cards(source):
    text = strip_comments(source)
    cards = []
    for match in CARD_COMMAND.finditer(text):
        kind = match.group(1)
        argument_count = 3 if kind == "blackcard" else 2
        position = match.end()
        arguments = []
        for _ in range(argument_count):
            while position < len(text) and text[position].isspace():
                position += 1
            argument, position = read_group(text, position)
            arguments.append(argument)
        raw_card = arguments[-1]
        card_text = convert_text(raw_card)
        if kind == "blackcard":
            try:
                pick = int(arguments[0].strip())
            except ValueError as error:
                raise ConversionError("Non-numeric black-card pick count") from error
            blanks = len(re.findall(r"\\underl\b", raw_card))
            # CAW appends a single answer if a Pick-1 question has no blank.
            if pick not in (1, 2, 3) or (blanks != pick and not (pick == 1 and blanks == 0)):
                raise ConversionError(f"Pick {pick} does not match {blanks} placeholders")
        cards.append(("b" if kind == "blackcard" else "w", card_text))
    return cards


def read_source(directory):
    cards = []
    seen = set()
    category_counts = {}
    for category in CATEGORIES:
        source = directory / "categories" / (category + ".tex")
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as error:
            raise ConversionError(f"Cannot read {source}") from error
        current = parse_cards(text)
        black = sum(kind == "b" for kind, _ in current)
        white = sum(kind == "w" for kind, _ in current)
        if not black or not white:
            raise ConversionError(f"Missing question or answer cards in {category}")
        category_counts[category] = {"black": black, "white": white}
        for record in current:
            key = (record[0], record[1].casefold())
            if key in seen:
                continue
            seen.add(key)
            cards.append(record)
    return cards, category_counts


def write_pack(destination, cards, license_name, rights_holder):
    destination.mkdir(parents=True, exist_ok=True)
    with (destination / "cards.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(("typ", "text"))
        writer.writerows(cards)
    manifest = {"packs": [{
        "id": PACK_ID,
        "file": "cards.csv",
        "title": "NSFW",
        "language": "de",
        "license": license_name,
        "rightsHolder": rights_holder,
        "sourceUrl": f"{SOURCE_URL}/tree/{SOURCE_COMMIT}",
        "defaultSelected": False,
    }]}
    (destination / "packs.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )


def verify_checkout(directory):
    """Never attribute modified or different upstream bytes to the pinned commit."""
    command = ["git", "-C", str(directory)]
    try:
        head = subprocess.run(command + ["rev-parse", "HEAD"], capture_output=True,
                              text=True, check=True).stdout.strip()
        changes = subprocess.run(command + ["status", "--porcelain", "--untracked-files=no"],
                                 capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise ConversionError("Source must be a clean Git checkout") from error
    if head != SOURCE_COMMIT or changes:
        raise ConversionError(f"Expected clean source commit {SOURCE_COMMIT}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Local checkout of the upstream repository")
    parser.add_argument("--destination", type=Path, default=ROOT / "local-cardpacks" / PACK_ID)
    parser.add_argument("--rights-confirmed", action="store_true", help="Confirm that the selected license and third-party permissions have been checked")
    parser.add_argument("--license", help="Verified redistribution license (not inferred from upstream README)")
    parser.add_argument("--rights-holder", help="Verified attribution of relevant rights holders")
    args = parser.parse_args()
    try:
        verify_checkout(args.source)
        cards, counts = read_source(args.source)
    except ConversionError as error:
        parser.error(str(error))
    black = sum(kind == "b" for kind, _ in cards)
    white = len(cards) - black
    print(f"Checked {black} black and {white} white cards across {len(counts)} categories.")
    if not args.rights_confirmed:
        print("No pack written: third-party rights need review. See docs/cardpacks/PRIOR99.md")
        return
    if not args.license or not args.rights_holder:
        parser.error("--rights-confirmed requires --license and --rights-holder")
    write_pack(args.destination, cards, args.license, args.rights_holder)
    print(f"Local pack written to {args.destination} (not tracked in Git).")


if __name__ == "__main__":
    main()
