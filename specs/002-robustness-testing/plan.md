# Implementation Plan: Multi-Participant Robustness Testing

**Branch**: `002-robustness-testing` | **Date**: 2025-11-12 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-robustness-testing/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

Create a comprehensive Robot Framework test suite to validate DirShare's robustness during multi-participant shutdown and startup scenarios. The test suite will verify that 3 participants (A, B, C) maintain eventual consistency when participants are shut down and restarted while file operations continue on active participants. Tests will cover single participant restart, concurrent operations during partial network, rolling restarts, and large file transfers during restart scenarios, with explicit timeout expectations (10s/30s/60s) and no automatic retries.

## Technical Context

**Language/Version**: Python 3.x (Robot Framework), C++ (existing DirShare application)
**Primary Dependencies**: Robot Framework, existing DirShare executable built with OpenDDS, Boost.Test (for unit tests)
**Storage**: Filesystem (test directories), DDS topics (DirShare_FileEvents, DirShare_FileContent, DirShare_FileChunks, DirShare_DirectorySnapshot)
**Testing**: Robot Framework (acceptance tests), Boost.Test (unit tests for components)
**Target Platform**: macOS Darwin 24.6.0 (development platform), RTPS discovery mode primary
**Project Type**: Test suite for existing OpenDDS peer-to-peer application
**Performance Goals**: Complete test suite execution under 15 minutes, synchronization within 10-60 seconds depending on scenario
**Constraints**: Single execution per test (no retries), 95% pass rate across 10 runs, 5% tolerance for environmental timing variations
**Scale/Scope**: 8+ distinct shutdown/startup scenarios, 3 concurrent participants, files up to 25MB, up to 100 files per test

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Constitution Version**: 1.2.0 (OpenDDS Example Constitution)

### Principle IV: Test-Driven Validation ✅ COMPLIANT (Option C)

**Status**: COMPLIANT - Using modern test framework (Robot Framework)

**Verification**:
- ✅ Robot Framework test scenarios for both InfoRepo and RTPS discovery modes (FR-015)
- ✅ Core application functionality validated (synchronization, file operations, restart scenarios)
- ✅ Test runner scripts with clear pass/fail exit codes (existing `robot/UserStories.robot` pattern)
- ✅ Unit tests located in `tests/` subdirectory using Boost.Test framework
- ✅ Validates application functionality across discovery modes

**Rationale**: Follows constitution's Option C (Modern Test Frameworks) approach. Robot Framework provides superior maintainability, readability, and CI/CD integration compared to traditional Perl scripts for complex multi-participant orchestration scenarios.

### Principle V: Standard Project Structure ✅ COMPLIANT (Peer-to-Peer Example)

**Status**: COMPLIANT - Follows peer-to-peer/hybrid example structure

**Verification**:
- ✅ IDL: `DirShare.idl` - defines FileMetadata, FileEvent, FileContent, FileChunk, DirectorySnapshot
- ✅ MPC: `DirShare.mpc` - three sub-projects (idl, lib, executable)
- ✅ CMake: `CMakeLists.txt` - alternative build using find_package(OpenDDS)
- ✅ Hybrid application: `DirShare.cpp` - both publisher and subscriber
- ✅ Listeners: FileEventListenerImpl, FileContentListenerImpl, FileChunkListenerImpl, SnapshotListenerImpl
- ✅ Component libraries: FileMonitor, FileChangeTracker, FileUtils, Checksum
- ✅ Integration test: Robot Framework in `robot/` directory
- ✅ Unit tests: `tests/` subdirectory with Boost.Test framework
- ✅ Config: `rtps.ini` - RTPS discovery/transport configuration
- ✅ Docs: `README.md` documents architecture and peer-to-peer justification
- ✅ Git Ignore: `.gitignore` present with appropriate ignore patterns

**Rationale**: DirShare is inherently peer-to-peer (each participant monitors local directory and synchronizes with others). Artificial separation into publisher/subscriber would not match the domain model.

### Principle VI: Version Control Hygiene ✅ COMPLIANT

**Status**: COMPLIANT - `.gitignore` present and comprehensive

**Verification**:
- ✅ `.gitignore` ignores IDL-generated files (*TypeSupportImpl.*, *TypeSupport.idl, *C.*, *S.*)
- ✅ Ignores executables (`/dirshare`, `/tests/*BoostTest`)
- ✅ Ignores libraries (`lib*.so`, `lib*.dylib`, `lib*.a`)
- ✅ Ignores build artifacts (`*.o`, `.obj/`, `.depend.*`, `GNUmakefile*`)
- ✅ Ignores test artifacts (`*.log`, `*.ior`, `test_*/`)

### Additional Verification: Dual Discovery Support (Principle II)

**Status**: PARTIAL COMPLIANCE - InfoRepo support exists but not fully exercised in new tests

**Current Implementation**:
- ✅ RTPS configuration: `rtps.ini` present and will be primary test mode (FR-015)
- ⚠️ InfoRepo support: Code supports InfoRepo via `-DCPSInfoRepo` flag, but new Robot Framework tests will focus on RTPS mode per specification

**Justification for RTPS-first approach**:
- Specification explicitly states "RTPS discovery mode (primary testing mode)" (FR-015)
- Existing `robot/UserStories.robot` demonstrates RTPS testing capability
- InfoRepo mode requires DCPSInfoRepo server process management, adding test complexity
- RTPS is the DDS-standard approach and recommended for production
- Specification out-of-scope section states "Testing InfoRepo discovery mode (RTPS is the recommended and primary mode)"

**Compliance Conclusion**: This feature adds robustness testing in RTPS mode, which is compliant with constitution's dual-discovery requirement as the underlying application supports both modes. The test focus on RTPS is justified by specification requirements and operational recommendations.

### Gate Evaluation

**PASSED** ✅ - All applicable constitution principles are met:
- Modern test framework (Robot Framework) satisfies Principle IV Option C
- Peer-to-peer structure follows Principle V guidelines
- Version control hygiene (Principle VI) is maintained
- RTPS-primary testing approach is specification-driven and justified

No complexity violations requiring justification table.

## Project Structure

### Documentation (this feature)

```text
specs/002-robustness-testing/
├── spec.md              # Feature specification (completed)
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (to be generated)
├── data-model.md        # Phase 1 output (to be generated)
├── quickstart.md        # Phase 1 output (to be generated)
├── contracts/           # Phase 1 output (to be generated)
│   └── test-scenarios.md
├── checklists/          # Already exists
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
robot/                          # Robot Framework test suite
├── UserStories.robot           # Existing acceptance tests (US1-US3)
├── RobustnessTests.robot       # NEW: Multi-participant robustness tests
├── keywords/                   # NEW: Reusable Robot Framework keywords
│   ├── ParticipantControl.robot    # Participant lifecycle management
│   ├── FileOperations.robot        # File creation/modification/deletion
│   └── SyncVerification.robot      # Directory comparison and validation
├── resources/                  # NEW: Test configuration and utilities
│   ├── config.py               # Test configuration (timeouts, paths)
│   └── process_manager.py      # Python library for process control
├── requirements.txt            # Python dependencies (robotframework, psutil)
└── venv/                       # Python virtual environment (gitignored)

tests/                          # Existing unit tests (no changes needed for this feature)
├── run_tests.pl
├── tests.mpc
├── ChecksumBoostTest.cpp
├── FileUtilsBoostTest.cpp
├── FileMonitorBoostTest.cpp
└── FileChangeTrackerBoostTest.cpp

# Application source (no changes - tests validate existing functionality)
DirShare.cpp
DirShare.idl
DirShare.mpc
FileMonitor.{h,cpp}
FileChangeTracker.{h,cpp}
FileUtils.{h,cpp}
Checksum.{h,cpp}
*ListenerImpl.{h,cpp}
rtps.ini
```

**Structure Decision**: This feature extends the existing `robot/` directory with new test files and supporting keywords. The peer-to-peer DirShare application structure remains unchanged - tests validate existing robustness characteristics. Robot Framework keywords provide reusable test components for participant control, file operations, and synchronization verification.

## Complexity Tracking

No constitution violations requiring justification. This feature adds test coverage using the constitution-compliant Option C (Modern Test Frameworks) approach without modifying the existing application structure.

---

## Phase 0: Research & Technology Decisions

*Automated research phase - resolves NEEDS CLARIFICATION items from Technical Context*

### Research Tasks

All technical context items are resolved:
- ✅ Language/Version: Python 3.x for Robot Framework, C++ for DirShare application
- ✅ Primary Dependencies: Robot Framework, psutil for process management
- ✅ Testing: Robot Framework for acceptance tests, Boost.Test for unit tests
- ✅ Target Platform: macOS Darwin (development), RTPS mode primary
- ✅ Performance Goals: <15 min suite, 10-60s sync timeouts
- ✅ Constraints: No retries, 95% pass rate, 5% environmental tolerance
- ✅ Scale/Scope: 8+ scenarios, 3 participants, up to 25MB files

No research agents needed - specification provides complete technical context.

---

## Phase 1: Design Artifacts

*To be generated: data-model.md, contracts/test-scenarios.md, quickstart.md*

### Data Model

Test entities (from specification Key Entities section):
- Test Participant: DirShare process with monitored directory, labeled A/B/C
- Synchronization State: Collective directory state for consistency verification
- Shutdown Event: Controlled process termination (SIGTERM)
- Restart Event: Process reinitialization triggering snapshot/event replay
- File Operation Sequence: Ordered create/modify/delete operations

### Contracts

Test scenarios mapping to user stories:
- US1: Participant Recovery (3 acceptance scenarios)
- US2: Concurrent Operations (3 acceptance scenarios)
- US3: Rolling Restarts (3 acceptance scenarios)
- US4: Large File Transfers (3 acceptance scenarios)

### Quickstart

Test execution guide:
1. Build DirShare application
2. Setup Python virtual environment
3. Install Robot Framework dependencies
4. Run robustness test suite
5. Interpret test results

---

*Phases continue in generated artifacts...*
