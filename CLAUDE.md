# Project Rules & Guidelines

## Codebase Lookup Preference
- Before performing any internet/web search for display, touchscreen, or hardware solutions for this ESP32 Cheap Yellow Display (CYD) project, **always search the local weather station repository first** at `/home/nicholas/git/nicholaswilde/cyd-weather-station/` for working reference code.

## Build and Test Commands
- Build firmware: `pio run -e cyd_28r` or `pio run -e cyd_35c`
- Run host-native tests: `pio test -e native`

## GitHub Operations
- When interacting with the remote repository (e.g. retrieving issues, listing pull requests, checking workflow status), **always use the `gh` tool** (and prefix it with `rtk` to save tokens, e.g., `rtk gh issue list` or `rtk gh pr status`).
