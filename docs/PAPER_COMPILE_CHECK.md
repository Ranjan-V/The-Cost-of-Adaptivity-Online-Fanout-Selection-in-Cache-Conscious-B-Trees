# Manuscript Compile Check

Status: **STATICALLY PREPARED, NOT COMPILED IN THIS TASK**.

The primary source is `sigmod_submission/main.tex`; the separate review
appendix is `sigmod_submission/appendix.tex`. The main source references the
local ACM class, `references.bib`, generated tables, and PDF figures under the
same directory. The Perfect decomposition inputs exist, and the platform text
distinguishes local ASUS i5-12500H/Windows measurements from Kaggle Xeon/Linux
selected-regime measurements.

After all correctness gates pass, run these commands in an environment with
`latexmk` and pdfLaTeX available:

```bash
cd sigmod_submission
latexmk -pdf -file-line-error -halt-on-error -interaction=nonstopmode main.tex
latexmk -pdf -file-line-error -halt-on-error -interaction=nonstopmode appendix.tex
```

Compilation warnings, missing references, missing citations, overfull boxes,
page count, anonymity, and PDF visual layout remain runtime/manual checks.
The `paper-source` GitHub Actions job is prepared to compile both sources, but
successful compilation still does not certify visual correctness.
