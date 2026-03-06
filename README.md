# ur

Your agent sandbox — secured, efficient, local hosted, for you only 

## Overview

## Requirements

## Build from Source

## Configuration

## Usage

```shell
# init database and workspace
ur init
# clean up database or workspace
ur clean [--database|workspace|all(default)]
# run one-time request
ur run <user_prompt> [--system-prompt=message|file] [--tools=message|file] [--model=provider/name] [--allow-all]
# tui chat with agent with context manager
ur chat [--continue=<id>] [--model=provider/name] [--allow-all]
# view session history
ur history [<id>]
# view user profile created by agent
ur persona
```

## Database Schema
```sql
TABLE session ()
TABLE message ()
TABLE persona ()
```

## Workspace Structure
```shell
# Linux
root=~/.local/share/ur/
# macOS
root=~/Library/Application Support/ur/
# Windows
root=%APPDATA%\ur\

# ur init creates the following
$root/workspace
$root/databse
$root/tools
$root/log
$root/keys
``` 


## Providers

### llama.cpp (default)

## Architecture

```
.
├── docs/
│   ├── devlog/         Daily development notes
│   └── plans/          Feature implementation plans
├── src/ur/             Agent implementation
│   ├── agent/          
│   ├── llm/
│   ├── memory/
│   └── tools/
└── tests/unit/         Unit tests, one file per source module
```

### Sandbox tiers

### Custom tool plugins

## Development

### Test organisation

## Roadmap

**Phase 1**:  

**Phase 2**:

**Phase 3**:
