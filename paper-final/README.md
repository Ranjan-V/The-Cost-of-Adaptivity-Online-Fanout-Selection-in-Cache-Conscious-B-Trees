# Final Paper Draft

Main paper source:

```powershell
paper-final\main.tex
```

Build from this folder after installing TeX Live or MiKTeX:

```powershell
.\build_paper.bat
```

Generated assets:

- `figures/fig_ycsb_throughput.pdf`
- `figures/fig_shifting_phases.pdf`
- `figures/fig_overhead_ablation.pdf`
- `figures/fig_benefit_vs_penalty.pdf`
- `figures/fig_thread_scalability.pdf`
- `figures/fig_dynamic_hotspot.pdf`
- `references.bib`

The same figure PDFs are also copied to `..\results\figures\` by:

```powershell
..\venv\Scripts\python.exe ..\scripts\generate_final_paper_figures.py
```

Before SIGMOD submission:

- Replace placeholder emails and ORCIDs.
- Confirm whether the official submission requires anonymous review mode.
- Replace the `TBD` conference location once the final venue metadata is known.
- Rebuild after rerunning benchmark trials if result CSVs change.
