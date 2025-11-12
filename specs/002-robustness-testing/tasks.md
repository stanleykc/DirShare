# Tasks: Multi-Participant Robustness Testing

**Feature**: 002-robustness-testing
**Input**: Design documents from `/specs/002-robustness-testing/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/test-scenarios.md

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story. This is a test-only feature - no changes to DirShare application code.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Test Infrastructure)

**Purpose**: Project initialization and Robot Framework test structure

- [X] T001 Update robot/requirements.txt to include psutil for process management
- [X] T002 Create robot/resources/config.py with timeout and file size constants
- [X] T003 [P] Create robot/keywords/ directory for reusable Robot Framework keywords
- [X] T004 [P] Create robot/resources/process_manager.py Python library skeleton

**Checkpoint**: Basic test infrastructure directories and dependencies ready

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core test utilities that ALL user stories depend on

**⚠️ CRITICAL**: No user story test implementation can begin until this phase is complete

- [X] T005 [P] Implement ProcessManager.start_participant() in robot/resources/process_manager.py
- [X] T006 [P] Implement ProcessManager.shutdown_participant() in robot/resources/process_manager.py
- [X] T007 [P] Implement ProcessManager.kill_participant() in robot/resources/process_manager.py
- [X] T008 [P] Implement ProcessManager.restart_participant() in robot/resources/process_manager.py
- [X] T009 [P] Create SyncVerifier class in robot/resources/sync_verifier.py with _build_file_map() and _checksum() methods
- [X] T010 [P] Implement SyncVerifier.directories_are_synchronized() in robot/resources/sync_verifier.py
- [X] T011 [P] Create TestFileGenerator class in robot/resources/test_files.py with create_test_file() method
- [X] T012 [P] Implement TestFileGenerator.create_large_file() in robot/resources/test_files.py for 5MB-25MB files
- [X] T013 Create ParticipantControl.robot keyword file in robot/keywords/ with Start Three Participants keyword
- [X] T014 Add Shutdown Participant and Restart Participant keywords to robot/keywords/ParticipantControl.robot
- [X] T015 Create FileOperations.robot keyword file in robot/keywords/ with Create File and Modify File keywords
- [X] T016 Add Delete File and Create Large File keywords to robot/keywords/FileOperations.robot
- [X] T017 Create SyncVerification.robot keyword file in robot/keywords/ with Wait For Synchronization keyword
- [X] T018 Add Verify File Checksums Match and Directories Are Synchronized keywords to robot/keywords/SyncVerification.robot
- [X] T019 Create robot/RobustnessTests.robot skeleton with Settings, Test Setup, and Test Teardown sections

**Checkpoint**: Foundation ready - all 12 test scenarios can now be implemented in parallel

---

## Phase 3: User Story 1 - Participant Recovery After Shutdown (Priority: P1) 🎯 MVP

**Goal**: Validate that a participant can recover from being offline and synchronize all missed file operations

**Independent Test**: Start 3 participants, shutdown one, create files on others, restart, verify synchronization within 10s

### Implementation for User Story 1

- [X] T020 [P] [US1] Implement US1.1: Single Participant Restart With File Creation test in robot/RobustnessTests.robot
- [X] T021 [P] [US1] Implement US1.2: Mixed File Operations During Downtime test in robot/RobustnessTests.robot
- [X] T022 [P] [US1] Implement US1.3: Multiple Participants Restart Sequentially test in robot/RobustnessTests.robot

**Checkpoint**: User Story 1 tests complete - validates basic recovery mechanism (3 test scenarios passing)

---

## Phase 4: User Story 2 - Concurrent Operations During Partial Network (Priority: P2)

**Goal**: Validate that file operations are correctly ordered and resolved when participants are offline

**Independent Test**: Orchestrate specific create/modify/delete sequences with controlled shutdown timing, verify convergence

### Implementation for User Story 2

- [ ] T023 [P] [US2] Implement US2.1: Concurrent File Creation While Participant Offline test in robot/RobustnessTests.robot
- [ ] T024 [P] [US2] Implement US2.2: Conflicting Modify and Delete Operations test in robot/RobustnessTests.robot
- [ ] T025 [P] [US2] Implement US2.3: Sequential File Creation With Intermediate Shutdown test in robot/RobustnessTests.robot

**Checkpoint**: User Story 2 tests complete - validates DDS event ordering and conflict resolution (6 total scenarios passing)

---

## Phase 5: User Story 3 - Rolling Participant Restarts (Priority: P3)

**Goal**: Validate that the system maintains synchronization during rolling restart patterns

**Independent Test**: Cycle through participant restarts while continuously creating files, verify final consistency

### Implementation for User Story 3

- [ ] T026 [P] [US3] Implement US3.1: Sequential Restart With Continuous File Creation test in robot/RobustnessTests.robot
- [ ] T027 [P] [US3] Implement US3.2: High-Volume Rolling Restart test with background file creation thread in robot/RobustnessTests.robot
- [ ] T028 [P] [US3] Implement US3.3: Restart During Active File Reception test in robot/RobustnessTests.robot

**Checkpoint**: User Story 3 tests complete - validates operational continuity during maintenance (9 total scenarios passing)

---

## Phase 6: User Story 4 - Large File Transfer During Participant Restart (Priority: P4)

**Goal**: Validate that chunked file transfers (>=10MB) complete successfully after participant restarts

**Independent Test**: Start large file transfer, force restart at specific completion percentage, verify file completes with correct checksum

### Implementation for User Story 4

- [ ] T029 [P] [US4] Implement US4.1: Receiver Restart During Chunked Transfer test with 50% shutdown trigger in robot/RobustnessTests.robot
- [ ] T030 [P] [US4] Implement US4.2: Concurrent Receiver Restart test in robot/RobustnessTests.robot
- [ ] T031 [P] [US4] Implement US4.3: Sender Restart During Chunked Transfer test in robot/RobustnessTests.robot

**Checkpoint**: All 12 test scenarios implemented and passing - robustness test suite complete

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Test suite reliability, documentation, and validation

- [ ] T032 [P] Add detailed logging to all keywords in robot/keywords/ files for debugging failed tests
- [ ] T033 [P] Implement test directory cleanup in Test Teardown section of robot/RobustnessTests.robot
- [ ] T034 [P] Add participant log capture (stdout/stderr) to robot/resources/process_manager.py
- [ ] T035 Update robot/RobustnessTests.robot to include tags (US1, US2, US3, US4, priority-P1, priority-P2, priority-P3, priority-P4)
- [ ] T036 Add timing statistics logging (actual sync time vs. timeout) to robot/keywords/SyncVerification.robot
- [ ] T037 [P] Update specs/002-robustness-testing/quickstart.md with actual test execution commands
- [ ] T038 [P] Update CLAUDE.md with Robot Framework robustness testing section
- [ ] T039 Run complete test suite 3 times consecutively to validate basic reliability
- [ ] T040 Validate test suite completes under 15 minutes (SC-006)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user story tests
- **User Stories (Phases 3-6)**: All depend on Foundational phase completion
  - User story tests can proceed in parallel (if staffed)
  - Or sequentially in priority order (P1 → P2 → P3 → P4)
- **Polish (Phase 7)**: Depends on all user story tests being implemented

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Can start after Foundational (Phase 2) - Independent from US1
- **User Story 3 (P3)**: Can start after Foundational (Phase 2) - Independent from US1/US2
- **User Story 4 (P4)**: Can start after Foundational (Phase 2) - Independent from US1/US2/US3

### Within Each User Story

- All 3 test scenarios per story can be implemented in parallel (marked [P])
- Each scenario is self-contained with own preconditions and expected outcomes
- Test execution is sequential but implementation can be parallel

### Parallel Opportunities

- **Phase 1**: T003 and T004 can run in parallel
- **Phase 2**: T005-T012 (Python libraries) can run in parallel, T013-T018 (Robot keywords) can run in parallel after
- **Phase 3-6**: All test scenarios within same user story marked [P] can be implemented in parallel
- **Phase 3-6**: Different user stories can be implemented in parallel by different developers
- **Phase 7**: T032, T033, T034, T037, T038 can run in parallel

---

## Parallel Example: User Story 1 (Phase 3)

```bash
# All 3 US1 test scenarios can be implemented concurrently:
Task T020: "Implement US1.1: Single Participant Restart With File Creation"
Task T021: "Implement US1.2: Mixed File Operations During Downtime"
Task T022: "Implement US1.3: Multiple Participants Restart Sequentially"
```

Each test is self-contained with distinct file creation patterns and timing.

---

## Parallel Example: Foundational Python Libraries (Phase 2)

```bash
# All Python utility classes can be implemented concurrently:
Task T005-T008: "ProcessManager methods (start, shutdown, kill, restart)"
Task T009-T010: "SyncVerifier methods (build_file_map, directories_are_synchronized)"
Task T011-T012: "TestFileGenerator methods (create_test_file, create_large_file)"
```

These are independent utility modules with no interdependencies.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T004)
2. Complete Phase 2: Foundational (T005-T019) - CRITICAL: All test utilities ready
3. Complete Phase 3: User Story 1 (T020-T022)
4. **STOP and VALIDATE**: Run US1 tests with `robot --include US1 RobustnessTests.robot`
5. Verify 3/3 tests passing with <10s synchronization times

### Incremental Delivery

1. Complete Setup (Phase 1) + Foundational (Phase 2) → Test infrastructure ready
2. Add User Story 1 (Phase 3) → Validate basic recovery → 3 scenarios passing (MVP!)
3. Add User Story 2 (Phase 4) → Validate conflict resolution → 6 scenarios passing
4. Add User Story 3 (Phase 5) → Validate rolling restarts → 9 scenarios passing
5. Add User Story 4 (Phase 6) → Validate large files → 12 scenarios passing (complete!)
6. Each story adds coverage without breaking previous scenarios

### Parallel Team Strategy

With multiple developers after Foundational phase (T019) completes:

1. Team completes Setup + Foundational together (T001-T019)
2. Once Foundational is done, parallelize user story implementation:
   - **Developer A**: Phase 3 - User Story 1 tests (T020-T022)
   - **Developer B**: Phase 4 - User Story 2 tests (T023-T025)
   - **Developer C**: Phase 5 - User Story 3 tests (T026-T028)
   - **Developer D**: Phase 6 - User Story 4 tests (T029-T031)
3. Each developer validates their story independently
4. Final integration: Run complete suite (all 12 scenarios)

---

## Validation Checklist

After complete implementation (all phases):

- [ ] All 12 test scenarios implemented in robot/RobustnessTests.robot
- [ ] Test suite runs with `robot --outputdir results robot/RobustnessTests.robot`
- [ ] At least 11/12 tests pass (95% pass rate per SC-004)
- [ ] Complete suite execution under 15 minutes (SC-006)
- [ ] Each test enforces correct timeout (10s/30s/60s per FR-017)
- [ ] Test report shows actual synchronization times
- [ ] Test directories cleaned up after execution
- [ ] Participant logs captured for debugging
- [ ] quickstart.md accurately reflects test execution steps

---

## Notes

- **No DirShare application changes**: This feature only adds test coverage, existing application remains unchanged
- **[P] tasks**: Different files or independent modules, can be implemented concurrently
- **[Story] labels**: Map tasks to specific user stories for traceability and independent testing
- **Timeout compliance**: US1 scenarios use 10s, US2/US3 use 10-30s, US4 uses 15-60s timeouts
- **Test independence**: Each scenario creates fresh directories, no shared state between tests
- **Checksum validation**: All tests verify CRC32 checksums match across participants (per data-model.md)
- **No retries**: Tests execute once per run, 5% failure tolerance for environmental variations (FR-016)
- **Existing patterns**: Follow robot/UserStories.robot structure for consistency

---

## Summary Statistics

- **Total Tasks**: 40 tasks
- **Setup**: 4 tasks (Phase 1)
- **Foundational**: 15 tasks (Phase 2) - BLOCKS all test implementation
- **User Story 1**: 3 tasks (Phase 3)
- **User Story 2**: 3 tasks (Phase 4)
- **User Story 3**: 3 tasks (Phase 5)
- **User Story 4**: 3 tasks (Phase 6)
- **Polish**: 9 tasks (Phase 7)
- **Test Scenarios**: 12 scenarios (3 per user story)
- **Parallel Opportunities**: 28 tasks marked [P] can run concurrently within phases
- **Estimated MVP Time**: Complete Setup + Foundational + US1 = ~22 tasks for first 3 passing scenarios
