# Implementation Plan - Implement Touch Screen Controls for Slideshow Navigation

## Phase 1: File Traversal Refactor (Bidirectional Support) [checkpoint: e91aaaf]

- [x] Task: Design and Implement Bidirectional File Cache [7aa600c]
    - [x] Write unit tests for file cache (index tracking, next/prev bounds handling)
    - [x] Implement a file list caching mechanism to support forward and backward traversal on the SD card
- [x] Task: Conductor - User Manual Verification 'Phase 1: File Traversal Refactor' (Protocol in workflow.md) [e91aaaf]

## Phase 2: Non-Blocking Slideshow Delay Refactor [checkpoint: 27ebfae]

- [x] Task: Implement Non-Blocking Wait Loop [ca61e69]
    - [x] Write unit tests for non-blocking timing loop with interrupt flags
    - [x] Refactor blocking `delay(10000)` in `main.cpp` to check a loop interval every 50ms
- [x] Task: Conductor - User Manual Verification 'Phase 2: Non-Blocking Slideshow Delay Refactor' (Protocol in workflow.md) [27ebfae]

## Phase 3: Touch Driver Integration and Setup [checkpoint: 3a32b3f]

- [x] Task: Touch Screen Hardware Setup [1c34c0f]
    - [x] Define conditional compile paths for XPT2046 and CST820/GT911 touch libraries
    - [x] Configure touch pins and initialize the drivers in setup
- [x] Task: Conductor - User Manual Verification 'Phase 3: Touch Driver Integration and Setup' (Protocol in workflow.md) [3a32b3f]

## Phase 4: Slide Navigation & Interaction Handler

- [ ] Task: Implement Touch Event and Navigation Zone Handlers
    - [ ] Write unit tests for coordinate mapping, zone triggers (Left/Center/Right), and debouncing logic
    - [ ] Implement coordinates check, map touch zone to slideshow navigation actions, and handle screen debounce
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Slide Navigation & Interaction Handler' (Protocol in workflow.md)
