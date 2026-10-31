# Attribution and license — Cards vs Humanity original CSV card decks

**Third-party card content, not CAW application source code.** This directory
contains the six **unchanged** UTF-8 source CSV decks offered on 2026-10-31 by
[Cards vs Humanity](https://www.cards-vs-humanity.de/), a German fan adaptation
of [Cards Against Humanity](https://www.cardsagainsthumanity.com/).

**Attribution:** Cards vs Humanity, for its German card sets and fan translation;
original game Cards Against Humanity. No endorsement or affiliation with CAW is
claimed. The source site identifies the decks collectively, without naming every
individual fan-card contributor. Additional individual attribution should be
preserved if learned later.

**License:** [Creative Commons Attribution–NonCommercial–ShareAlike 2.0
(CC BY-NC-SA 2.0)](https://creativecommons.org/licenses/by-nc-sa/2.0/).
The source explicitly offers sharing and remixing, but **not commercial use**
without authorization. Preserve attribution; adaptations of this material
must meet ShareAlike requirements. The license is separate from any license
applied to the CAW application code. Do not relabel these cards MIT/CC0.

**Modifications:** none. The six CSV files below are byte-for-byte source
downloads. This attribution file and the checksum manifest were added by CAW.
The files have not been normalized, translated, filtered, or edited.

**Source index:** https://www.cards-vs-humanity.de/set/list/

| Original set / source | File | Black | White | SHA-256 |
|---|---|---:|---:|---|
| [Basisspiel 1 – Minions mit Genitalherpes](https://www.cards-vs-humanity.de/csv/Basisspiel%2B1%2B-%2BMinions%2Bmit%2BGenitalherpes/1/) | `base-1.csv` | 45 | 216 | `71544d28ab6f8fcbbc38e92c4613a29df2b6ffb65264d4888f8120a077cb5491` |
| [Basisspiel 2 – Heidi Klums Hungersnot](https://www.cards-vs-humanity.de/csv/Basisspiel%2B2%2B-%2BHeidi%2BKlums%2BHungersnot/2/) | `base-2.csv` | 45 | 216 | `4d7127e09f7393acda97f38ce5f21310171bd2bc8ab15625e4c167077cb23a8e` |
| [Erweiterung 1 – Wir werden bösartig](https://www.cards-vs-humanity.de/csv/Erweiterung%2B1%2B-%2BWir%2Bwerden%2Bb%C3%B6sartig/3/) | `expansion-1.csv` | 18 | 153 | `4279d61bb124edf1745ab9ab11a70c50d5660e571d90d056593f84bcf88d388b` |
| [Erweiterung 2 – Black Cards Matter](https://www.cards-vs-humanity.de/csv/Erweiterung%2B2%2B-%2BBlack%2BCards%2BMatter/4/) | `expansion-2.csv` | 144 | 0 | `a4f9128c8a79067da1201e02783c4385c1e851b36bdf88c661f91187ddeb3ca6` |
| [Fan-Erweiterung 1 – That's what they said](https://www.cards-vs-humanity.de/csv/Fan-Erweiterung%2B1%2B-%2BThat%27s%2Bwhat%2Bthey%2Bsaid/5/) | `fan-1.csv` | 45 | 180 | `650fe3de8d762b36a5df8435814c456631b773cc03dc1ff699444b48593716e5` |
| [Fan-Erweiterung 2 – Deine Mudda!](https://www.cards-vs-humanity.de/csv/Fan-Erweiterung%2B2%2B-%2BDeine%2BMudda%21/6/) | `fan-2.csv` | 288 | 414 | `b046ed180b0ed2400381d444364177c3e4564e3143f7274bf7a6159d1d83f728` |

All source files have CSV header `typ,text`; type `b` means black, and
`w` means white. There are 585 black and 1,179 white cards in total.

**Use in CAW:** Storing source CSVs here does **not** make them gameplay packs.
Conversion, card-placeholder validation, pack metadata persistence, and
in-game license attribution are tracked in milestone M5. Public/commercial
deployment needs an appropriate rights review, including any third-party
materials contained in fan submissions.
