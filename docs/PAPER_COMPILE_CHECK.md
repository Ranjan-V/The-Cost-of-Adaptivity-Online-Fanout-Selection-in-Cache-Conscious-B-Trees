# Manuscript Compile Check

Status: **COMPILED AND CHECKED** (2026-10-08).

The primary source is `sigmod_submission/main.tex`; the separate review
appendix is `sigmod_submission/appendix.tex`. The main source references the
local ACM class, `references.bib`, generated tables, and PDF figures under the
same directory. Both documents use the same anonymous, venue-neutral ACM
`sigconf` review format. The platform text distinguishes local ASUS
i5-12500H/Windows measurements, Kaggle Xeon/Linux selected and scale campaigns,
and the independent Kaggle EPYC/Linux phase-length campaign.

The checked build used pdfLaTeX and BibTeX directly because Perl/`latexmk` was
not available in the local Windows environment:

```powershell
cd sigmod_submission
pdflatex -interaction=nonstopmode -halt-on-error main.tex
bibtex main
pdflatex -interaction=nonstopmode -halt-on-error main.tex
pdflatex -interaction=nonstopmode -halt-on-error main.tex
pdflatex -interaction=nonstopmode -halt-on-error appendix.tex
pdflatex -interaction=nonstopmode -halt-on-error appendix.tex
```

An environment with `latexmk` can equivalently use:

```bash
cd sigmod_submission
latexmk -pdf -file-line-error -halt-on-error -interaction=nonstopmode main.tex
latexmk -pdf -file-line-error -halt-on-error -interaction=nonstopmode appendix.tex
```

The checked outputs are `sigmod_submission/main.pdf` (14 pages total, with the
12-page research body followed by references beginning on page 13) and
`sigmod_submission/appendix.pdf` (2 pages). The logs contain no unresolved
references, missing citations, overfull boxes, or ACM-class warnings. The
rendered pages were also inspected for clipping, overlap, figure placement,
table legibility, and anonymity. The `paper-source` GitHub Actions job remains
the independent reproducibility build.
