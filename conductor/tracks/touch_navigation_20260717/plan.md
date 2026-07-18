# Implementation Plan - Implement Touch Screen Controls for Slideshow Navigation

## Phase 1: File Traversal Refactor (Bidirectional Support)

- [ ] Task: Design and Implement Bidirectional File Cache
    - [ ] Write unit tests for file cache (index tracking, next/prev bounds handling)
    - [ ] Implement a file list caching mechanism to support forward and backward traversal on the SD card
- [ ] Task: Conductor - User Manual Verification 'Phase 1: File Traversal Refactor' (Protocol in workflow.md)

## Phase 2: Non-Blocking Slideshow Delay Refactor

- [ ] Task: Implement Non-Blocking Wait Loop
    - [ ] Write unit tests for non-blocking timing loop with interrupt flags
    - [ ] Refactor blocking `delay(10000)` in `main.cpp` to check a loop interval every 50ms
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Non-Blocking Slideshow Delay Refactor' (Protocol in workflow.md)

## Phase 3: Touch Driver Integration and Setup

- [ ] Task: Touch Screen Hardware Setup
    - [ ] Define conditional compile paths for XPT2046 and CST820/GT911 touch libraries
    - [ ] Configure touch pins and initialize the drivers in setup
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Touch Driver Integration and Setup' (Protocol in workflow.md)

## Phase 4: Slide Navigation & Interaction Handler

- [ ] Task: Implement Touch Event and Navigation Zone Handlers
    - [ ] Write unit tests for coordinate mapping, zone triggers (Left/Center/Right), and debouncing logic
    - [ ] Implement coordinates check, map touch zone to slideshow navigation actions, and handle screen debounce
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Slide Navigation & Interaction Handler' (Protocol in workflow.md)
