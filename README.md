# C++ Shogi Utilities

A collection of C++ utilities for managing and analyzing shogi game records (KIF files). 
## Table of Contents


## Overview

**organize_kif** - Automated KIF file organization based on pattern matching

## Prerequisites

- C++17 compatible compiler (g++, clang++)
- Standard C++ libraries
- `iconv` command-line utility (for Japanese text encoding conversion)
- JSON library (nlohmann/json - included as `json.hpp`)
- Linux


## Utilities

### 1. compare kif

Remove duplication.


### 2. organize_kif

Automated KIF file organization system that moves files from input directory to categorized output directories based on filename patterns.

#### Features

- Pattern-based file classification
- Automatic directory creation

#### Usage

```bash
./organize_kif
```

#### Configuration

Uses `setting.json` for pattern matching:

```json
[
  {
    "name": "24",
    "player": "your_user_name",
    "pattern": "^\\d+_(\\d{4})_.*\\.kif$",
    "output_path": "/path/to/output/24_games"
  },
  {
    "name": "wars",
    "player": "your_user_name", 
    "pattern": "^.*?(\\d{8})_.*?\\.kif$",
    "output_path": "/path/to/output/wars_games"
  }
]
```

