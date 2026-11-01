# Prior99 CAH Fullformat German — source and rights notices

This directory contains a **converted third-party card set**, not CAW application code.

**Source:** https://github.com/Prior99/cah-fullformat-german  
**Pinned revision:** `92501b245f7002ac3d3875099d859a5579aa2161`  
**Original project contributors named by its README:** Soana (Andra Ruebsteck),
Prior (Frederick Gnodtke), Moerrrlin (Merlin Rübsteck), n00b42 (Maximilian Senftleben),
AnotherStranger (André Büsgen), Vankog (Daniel Mönch), and "Balz" / "Raphael"
(officially recognized fan translations).  
**Original game:** Cards Against Humanity. No affiliation or endorsement.

## Claimed and underlying licenses

Prior99 labels its LaTeX project **GPL-3.0-or-later**, claiming that the
original Cards Against Humanity was GPL. That premise is incorrect: the
original CAH download and its derivative works are published under
**Creative Commons BY-NC-SA 2.0**. A derivative translation may involve
both original and translation rights; these statements are not proof
that Prior99 contributors have granted all permissions needed to
relicense every card.

- https://github.com/Prior99/cah-fullformat-german/blob/92501b245f7002ac3d3875099d859a5579aa2161/README.md
- https://github.com/Prior99/cah-fullformat-german/blob/92501b245f7002ac3d3875099d859a5579aa2161/cah-fullformat-german.tex
- https://s3.amazonaws.com/cah/CAH_MainGame.pdf (official CAH usage terms)
- https://creativecommons.org/licenses/by-nc-sa/2.0/

The licence field in `packs.json` names **both source notices**, not
a legal finding of compatibility or permission for commercial use.
**Third-party rights remain unresolved**, especially for translations
and fan contributions. Review before public hosting, further
redistribution, or commercial deployment. CAW's own MIT license does
not apply to these cards.

## Changes for CAW

The 11 LaTeX categories were combined into one UTF-8 `typ,text` CSV:
107 questions and 610 answers. Page layout, TeX markup and print
metadata were removed; `\\underl` became `___`, `\\newline` became
whitespace, TeX escape sequences were decoded, and whitespace
normalized. The wording was otherwise retained (no editorial review).
37 Pick-1 questions without a placeholder intentionally keep that
form: CAW appends the selected answer. The source contains content
intended for adults, including possibly offensive and outdated material.

This deck is opt-in (`defaultSelected: false`); it must be chosen
explicitly by the host from the pack selection UI. It may be combined
with other packs (subject to CAW's 8-pack limit). Duplicate texts are
removed by CAW's mixed-deck builder.

Conversion checks and source statistics: `docs/cardpacks/PRIOR99.md`.
