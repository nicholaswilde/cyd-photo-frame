# Project Rules & Guidelines

## Codebase Lookup Preference
- Before performing any internet/web search for display, touchscreen, or hardware solutions for this ESP32 Cheap Yellow Display (CYD) project, **always search the local weather station repository first** at `/home/nicholas/git/nicholaswilde/cyd-weather-station/` for working reference code.

## Build and Test Commands
- Build firmware: `pio run -e cyd_28r` or `pio run -e cyd_35c`
- Run host-native tests: `pio test -e native`

## Git & GitHub Operations
- When running `git` commands, **always prefix them with `rtk`** to save tokens (e.g., `rtk git status`, `rtk git diff`, `rtk git log`).
- When interacting with the remote repository (e.g. retrieving issues, listing pull requests, checking workflow status), **always use the `gh` tool** (and prefix it with `rtk` to save tokens, e.g., `rtk gh issue list` or `rtk gh pr status`).
- Always pipe `gh` commands to `cat` to bypass interactive pagers (e.g., `rtk gh issue list | cat`, `rtk gh issue view 1 | cat`).

## What To Do Next
- When asked "what to do next" (or similar), **always check the remote repository issues first** using `gh`:
  ```bash
  rtk gh issue list | cat
  ```
