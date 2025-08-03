# C++ Shogi Utilities

A collection of C++ utilities for managing and analyzing shogi game records (KIF files) with database integration.

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Compilation](#compilation)
- [Utilities](#utilities)
  - [1. file_manager](#1-file_manager)
  - [2. organize_kif](#2-organize_kif)
  - [3. update_sql](#3-update_sql)
- [Configuration](#configuration)
- [Database Schema](#database-schema)
- [Usage Examples](#usage-examples)
- [Troubleshooting](#troubleshooting)

## Overview

This project provides three main utilities for processing shogi game records:

1. **file_manager** - A file organization system with level-based categorization
2. **organize_kif** - Automated KIF file organization based on pattern matching
3. **update_sql** - KIF file analysis and SQL database integration with evaluation processing

## Prerequisites

- C++17 compatible compiler (g++, clang++)
- Standard C++ libraries
- `iconv` command-line utility (for Japanese text encoding conversion)
- JSON library (nlohmann/json - included as `json.hpp`)
- File system support

### System Dependencies

```bash
# On Ubuntu/Debian
sudo apt install build-essential iconv

# On macOS
brew install gcc iconv

# On Fedora/RHEL
sudo dnf install gcc-c++ glibc-common
```

## Compilation

Compile all utilities using the provided commands:

```bash
# Compile file_manager
g++ -std=c++17 -o file_manager file_manager.cpp

# Compile organize_kif
g++ -std=c++17 -o organize_kif organize_kif.cpp

# Compile update_sql
g++ -std=c++17 -o update_sql update_sql.cpp
```

## Utilities

### 1. file_manager

A file management utility that organizes files with date-based tracking and level categorization.

#### Features

- Date-based file entry tracking
- Level-based categorization (1-10)
- Random file selection by level
- Persistent storage using `entries.txt`

#### Usage

```bash
# Create a new file entry
./file_manager create YYYY-MM-DD LEVEL /path/to/file

# Update a file's level
./file_manager update /path/to/file NEW_LEVEL

# Open a file in system explorer
./file_manager open /path/to/file

# Check and automatically open files based on time intervals
./file_manager check
```

#### Commands

- **create** - Add a new file entry with date, level, and path
- **update** - Change the level of an existing file entry
- **open** - Open a specific file in the system file explorer
- **check** - Check files and automatically open those ready for review based on exponential time intervals (2^level days)

#### Example Usage

```bash
# Create a new entry for today with level 5
./file_manager create 2025-08-03 5 /path/to/my/file.txt

# Update an existing file to level 7
./file_manager update /path/to/my/file.txt 7

# Open a file in the system explorer
./file_manager open /path/to/my/file.txt

# Check all files and open those ready for review
./file_manager check
```

### 2. organize_kif

Automated KIF file organization system that moves files from input directory to categorized output directories based on filename patterns.

#### Features

- Pattern-based file classification
- Automatic directory creation
- JSON configuration support
- Batch file processing

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
    "player": "",
    "pattern": "^\\d+_(\\d{4})_.*\\.kif$",
    "output_path": "/path/to/output/24_games"
  },
  {
    "name": "wars",
    "player": "", 
    "pattern": "^.*?(\\d{8})_.*?\\.kif$",
    "output_path": "/path/to/output/wars_games"
  }
]
```

#### Process Flow

1. Scans `Evaluation/input` directory for KIF files
2. Matches filenames against patterns in `setting.json`
3. Creates target directories if they don't exist
4. Moves files to appropriate categorized folders
5. Reports success/failure for each file

### 3. update_sql

Advanced KIF file analysis tool that extracts game information, processes evaluation data, and generates SQL database entries.

#### Features

- **Dual Format Support**: Handles both legacy (`**解析 0 候補1 時間...評価値`) and new (`*#評価値=数値`) evaluation formats
- **Encoding Conversion**: Automatic Shift_JIS to UTF-8 conversion using `iconv`
- **Game Mode Detection**: Extracts game modes (10分, 3分, スプリント, 10秒) for wars format
- **Rating Extraction**: Processes player ratings for 24-hour format games
- **Strategic Analysis**: Extracts opening strategies (戦法) and castle formations (囲い)
- **Recursive Processing**: Processes entire directory trees
- **SQL Integration**: Updates database files with processed game data
- **Detailed Logging**: Comprehensive processing logs with error reporting

#### Usage

```bash
# Process a single KIF file
./update_sql /path/to/single/file.kif

# Process entire directory recursively
./update_sql /path/to/directory

# Process the evaluation directory
./update_sql /home/takanori/C-_utils/Evaluation
```

#### Supported Game Formats

##### 1. Wars Format (`wars`)
- **File Pattern**: `^.*?(\\d{8})_.*?\\.kif$`
- **Player**: Love_Kapibara
- **Game Modes**: 10m (10分), 3m (3分), sp (スプリント), 10s (10秒)
- **Extracted Data**: mode, player position, win/loss, evaluation loss, strategies, castles

##### 2. 24-Hour Format (`24`)
- **File Pattern**: `^\\d+_(\\d{4})_.*\\.kif$`
- **Player**: komasan88
- **Game Modes**: hy (早指し), hy2, etc.
- **Extracted Data**: mode, player position, win/loss, evaluation loss, player ratings

#### Evaluation Processing

The system processes evaluation values from KIF files and calculates:

- **Evaluation Extraction**: Captures numerical evaluation values from analysis lines
- **Value Sanitization**: Limits values to range [-3300, 3300] for database constraints
- **Difference Calculation**: Computes evaluation changes between moves
- **Loss Averaging**: Calculates average evaluation loss for both players
- **Mate Detection**: Handles checkmate notations (詰)

#### Output

Generates SQL INSERT statements and updates database files:

```sql
-- Wars format output
INSERT INTO shogi_eval_loss_for_wars (
    game_mode, teban, result, value1, value2,
    s_senpou, g_senpou, s_kakoi, g_kakoi, filename
) VALUES
('sp', true, true, -150, 300, '石田流', '角換わり', '美濃囲い', '居玉', 'game.kif');

-- 24-hour format output  
INSERT INTO shogi_eval_loss_for_24 (
    game_mode, teban, result, value1, value2,
    sente_rating, gote_rating, filename
) VALUES
('hy', false, false, -200, 150, 1500, 1600, 'game.kif');
```

## Configuration

### setting.json

Central configuration file for pattern matching and output routing:

```json
[
  {
    "name": "24",                    // Configuration identifier
    "player": "",           // Target player name
    "pattern": "^\\d+_(\\d{4})_.*\\.kif$",  // Regex pattern for filename matching
    "output_path": "/path/to/output", // Directory for organized files
    "sql_file_path": "/path/to/sql"   // Target SQL file for database updates
  }
]
```

#### Pattern Examples

- **24-hour games**: `^\\d+_(\\d{4})_.*\\.kif$` matches `12345_1500_player1_vs_player2.kif`
- **Wars games**: `^.*?(\\d{8})_.*?\\.kif$` matches `player1-player2-20250803_123456.kif`

## Database Schema

### Wars Format Table (`shogi_eval_loss_for_wars`)

```sql
CREATE TABLE IF NOT EXISTS shogi_eval_loss_for_wars (
    id INT PRIMARY KEY AUTO_INCREMENT,
    game_mode VARCHAR(10),        -- 'sp', '10m', '3m', '10s'
    teban BOOLEAN,                -- true = sente (first player), false = gote (second player)
    result BOOLEAN,               -- true = win, false = lose, NULL = draw
    value1 INT CHECK (value1 BETWEEN -3300 AND 3300),  -- sente evaluation loss
    value2 INT CHECK (value2 BETWEEN -3300 AND 3300),  -- gote evaluation loss
    s_senpou VARCHAR(50),         -- sente strategy (先手の戦法)
    g_senpou VARCHAR(50),         -- gote strategy (後手の戦法)
    s_kakoi VARCHAR(50),          -- sente castle (先手の囲い)
    g_kakoi VARCHAR(50),          -- gote castle (後手の囲い)
    filename VARCHAR(255),        -- source KIF filename
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### 24-Hour Format Table (`shogi_eval_loss_for_24`)

```sql
CREATE TABLE IF NOT EXISTS shogi_eval_loss_for_24 (
    id INT PRIMARY KEY AUTO_INCREMENT,
    game_mode VARCHAR(10),        -- 'hy', 'hy2', etc.
    teban BOOLEAN,                -- true = sente, false = gote
    result BOOLEAN,               -- true = win, false = lose, NULL = draw
    value1 INT CHECK (value1 BETWEEN -3300 AND 3300),  -- sente evaluation loss
    value2 INT CHECK (value2 BETWEEN -3300 AND 3300),  -- gote evaluation loss
    sente_rating INT,             -- sente player rating
    gote_rating INT,              -- gote player rating
    filename VARCHAR(255),        -- source KIF filename
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Usage Examples

### Complete Workflow Example

```bash
# 1. Organize new KIF files from input directory
./organize_kif

# 2. Process organized files and update database
./update_sql /home/takanori/C-_utils/Evaluation

# 3. Manage file references with level system
./file_manager
```



## Troubleshooting

### Common Issues


2. **Encoding Issues**
   ```bash
   # Test iconv installaton
   iconv --version
   # Test manual conversion
   iconv -f shift_jis -t utf-8 input.kif
   ```

### Logging and Debugging

Enable detailed logging by examining program output:

```bash
# Redirect output to log file
./update_sql /path/to/directory 2>&1 | tee processing.log

# Monitor progress in real-time
./update_sql /path/to/directory | grep -E "(Processing|Successfully|Error)"
```

## License

This project is licensed under the terms specified in the LICENSE file.

