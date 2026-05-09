# GRANITE – Documentation (docs/)

> [!NOTE]
> **Welcome to the GRANITE knowledge base.** This directory houses the authoritative technical documentation, theoretical stubs, and architecture guides for the engine.
> For the root project overview, return to the [Main README](../README.md).

## Quick Navigation

| Topic | Reference File |
|-------|----------------|
| **Installation** | [getting_started/Installation.md](getting_started/Installation.md) |
| **Running Benchmarks** | [user_guide/BENCHMARKS.md](user_guide/BENCHMARKS.md) |
| **Output & Diagnostics** | [user_guide/diagnostic_handbook.md](user_guide/diagnostic_handbook.md) |
| **Developer Architecture** | [developer_guide/DEVELOPER_GUIDE.md](developer_guide/DEVELOPER_GUIDE.md) |
| **Physics Background** | [theory/SCIENCE.md](theory/SCIENCE.md) |
| **Author's Personal Note** | [design/PERSONAL_NOTE.md](design/PERSONAL_NOTE.md) |

## Directory Structure

```text
docs/
├── README.md                     # You are here
├── index.rst                     # Sphinx documentation root
├── conf.py                       # Sphinx configuration
│
├── getting_started/              # Setup and platform guides (WSL2 / Linux)
│   ├── INSTALL.md                
│   └── WINDOWS_README.md         
│
├── user_guide/                   # Runtime operation and I/O analysis
│   ├── BENCHMARKS.md             
│   ├── diagnostic_handbook.md    
│   ├── FAQ.md
│   └── DEPLOYMENT_AND_PERFORMANCE.md  
│
├── developer_guide/              # Engine internals, code logic, and design patterns
│   ├── DEVELOPER_GUIDE.md        
│   ├── COMPARISON.md             
│   └── v0.6.5_master_dictionary.md   
│
├── theory/                       # Mathematical formulations (CCZ4, GRMHD, AMR)
│   ├── index.rst                 # Theory Hub root
│   ├── SCIENCE.md                
│   ├── amr.rst
│   ├── ccz4.rst                  
│   ├── grmhd.rst
│   ├── gw_extraction.rst
│   ├── initial_data.rst
│   ├── radiation.rst
│   └── citation.bib              
│    
├── design/                       # Philosophy, narrative, and historical context
│   ├── VISION.md
│   ├── PERSONAL_NOTE.md          
│   ├── GENESIS_AND_ARCHITECTURE.md
│   └── DEVELOPMENT_JOURNAL.md    
│
└── paper/                        
    ├── granite_preprint_v067.tex
    ├── granite_preprint_v067.pdf
    └── figures/
```

> [!IMPORTANT]
> **Documentation Builds:** To build the HTML documentation, run:
> ```bash
> pip install -e .[docs]          # install Sphinx + furo (one-time)
> python3 scripts/run_granite.py docs          # build HTML
> python3 scripts/run_granite.py docs --open  # build and open in browser
> ```
> Output is written to `docs/_build/html/index.html`.
