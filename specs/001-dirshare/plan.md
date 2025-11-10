# Implementation Plan: DirShare - Distributed File Synchronization

**Branch**: `001-dirshare` | **Date**: 2025-10-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/001-dirshare/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

DirShare is a distributed file synchronization example demonstrating OpenDDS publish-subscribe patterns for real-time file sharing between multiple participants. The system enables automatic bidirectional synchronization of files in a shared directory across network-connected machines, using DDS topics to propagate file creation, modification, and deletion events. A key design consideration is distinguishing locally-initiated changes from remotely-received changes to prevent notification loops where participants republish changes they received from others. The implementation follows OpenDDS DevGuideExamples conventions with dual discovery support (InfoRepo and RTPS), comprehensive testing, and proper DDS lifecycle management.

## Technical Context

**Language/Version**: C++ (C++11 or later per OpenDDS requirements)
**Primary Dependencies**: OpenDDS (DDS implementation), ACE/TAO (platform abstraction, auto-configured via OpenDDS configure script), Boost (Boost.Test framework for unit testing), Perl (test runner scripts), Robot Framework 6.0+ with Process and OperatingSystem libraries (acceptance testing), Python 3.8+ (Robot Framework runtime)
**Storage**: Local file system (no database required)
**Testing**: Multi-layered test strategy:
  - **Unit Tests**: Boost.Test framework for component-level testing
  - **Integration Tests**: PerlDDS::TestFramework (run_test.pl) for DDS-level integration
  - **Acceptance Tests**: Robot Framework for end-to-end user story validation
**Target Platform**: Linux, macOS, Windows (anywhere OpenDDS runs)
**Project Type**: Single project (OpenDDS DevGuideExample)
**Performance Goals**: File propagation within 5 seconds for files up to 10MB, initial sync of 100 files within 30 seconds, support 10+ simultaneous participants
**Constraints**: 5-second latency requirement (SC-002/SC-003), 1GB max file size (Assumption #8), chunked transfer for files >=10MB with 1MB chunks, DDS message size limits require chunking strategy
**Scale/Scope**: Example-level scope (~3000 LOC estimated), demonstrates core DDS patterns for file synchronization, supports 10+ participants (SC-006)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

**Constitution Version**: 1.0.0 (ratified 2025-10-30)

### Principle Compliance

| Principle | Status | Evidence |
|-----------|--------|----------|
| **I. IDL-First Design** | ✅ **PASS** | spec.md defines all data entities (FileEvent, FileMetadata, FileContent, FileChunk, DirectorySnapshot) as Key Entities section; tasks.md T002 creates DirShare.idl with @topic and @key annotations; T016-T019 register TypeSupport |
| **II. Dual Discovery/Transport** | ✅ **PASS** | spec.md FR-008 mandates both InfoRepo and RTPS support; tasks.md T005 creates rtps.ini, T133-T134 test both modes; run_test.pl will support --rtps flag per constitution requirement |
| **III. Complete DDS Lifecycle** | ✅ **PASS** | tasks.md Phase 2 (T015-T027) implements full lifecycle: TheParticipantFactoryWithArgs → create_participant → register TypeSupport → create topics → create Publisher/Subscriber → WaitSet for sync → delete_contained_entities → shutdown |
| **IV. Test-Driven Validation** | ✅ **PASS** | tasks.md includes 63 Boost.Test unit test tasks + Robot Framework acceptance tests; T132 creates run_test.pl using PerlDDS::TestFramework; T133-T134 validate InfoRepo and RTPS modes; constitution requirement for automated testing fully satisfied |
| **V. Standard Project Structure** | ✅ **PASS** | tasks.md T001-T006 create standard structure: DirShare.idl, DirShare.mpc (3 projects: idl, lib, exe), CMakeLists.txt, rtps.ini, README.md; follows DevGuideExamples/DCPS/[ExampleName]/ convention |

### Gate Evaluation

**Result**: ✅ **PASS** - All 5 constitutional principles satisfied. No violations detected.

**Rationale**: DirShare follows established OpenDDS example patterns (similar to Messenger example) and explicitly addresses all constitutional requirements in functional requirements and task breakdown. Robot Framework addition enhances test coverage without violating any principles - it provides acceptance-level validation complementing existing unit and integration tests.

## Project Structure

### Documentation (this feature)

```text
specs/001-dirshare/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
DevGuideExamples/DCPS/DirShare/
├── DirShare.idl                      # IDL data type definitions (FileEvent, FileMetadata, etc.)
├── DirShare.mpc                      # MPC build configuration (idl, lib, exe projects)
├── CMakeLists.txt                    # CMake alternative build support
├── DirShare.cpp                      # Main application (DDS initialization, CLI args)
├── FileMonitor.h                     # File system monitoring interface
├── FileMonitor.cpp                   # Polling-based directory change detection
├── Checksum.h                        # CRC32 checksum utilities
├── Checksum.cpp                      # Checksum implementation
├── FileUtils.h                       # File I/O utilities (read, write, timestamps)
├── FileUtils.cpp                     # File utilities implementation
├── SnapshotListenerImpl.h            # DirectorySnapshot DataReader listener
├── SnapshotListenerImpl.cpp          # Initial sync logic
├── FileEventListenerImpl.h           # FileEvent DataReader listener
├── FileEventListenerImpl.cpp         # File create/modify/delete event handling
├── FileContentListenerImpl.h         # FileContent DataReader listener (small files)
├── FileContentListenerImpl.cpp       # Small file (<10MB) transfer logic
├── FileChunkListenerImpl.h           # FileChunk DataReader listener (large files)
├── FileChunkListenerImpl.cpp         # Chunk reassembly logic (>=10MB files)
├── rtps.ini                          # RTPS discovery/transport configuration
├── run_test.pl                       # Perl test launcher (InfoRepo + RTPS modes)
├── README.md                         # Build instructions, usage examples
├── tests/                            # Unit tests directory
│   ├── tests.mpc                     # Test projects MPC configuration
│   ├── run_tests.pl                  # Test runner script
│   ├── ChecksumBoostTest.cpp         # Boost.Test suite for Checksum utilities
│   ├── FileUtilsBoostTest.cpp        # Boost.Test suite for FileUtils
│   ├── FileMonitorBoostTest.cpp      # Boost.Test suite for FileMonitor
│   ├── DirectorySnapshotBoostTest.cpp # Boost.Test suite for snapshot logic
│   ├── FileContentBoostTest.cpp      # Boost.Test suite for small file transfer
│   ├── FileChunkBoostTest.cpp        # Boost.Test suite for chunking/reassembly
│   ├── FileEventCreateBoostTest.cpp  # Boost.Test suite for CREATE events
│   ├── FileEventModifyBoostTest.cpp  # Boost.Test suite for MODIFY events
│   ├── FileEventDeleteBoostTest.cpp  # Boost.Test suite for DELETE events
│   ├── ConflictResolutionBoostTest.cpp # Boost.Test suite for timestamp conflicts
│   ├── MetadataPreservationBoostTest.cpp # Boost.Test suite for metadata
│   ├── DDSInitBoostTest.cpp          # Boost.Test suite for DDS initialization
│   ├── QoSPolicyBoostTest.cpp        # Boost.Test suite for QoS policies
│   ├── ErrorHandlingBoostTest.cpp    # Boost.Test suite for error conditions
│   ├── SpecialCharactersBoostTest.cpp # Boost.Test suite for special filenames
│   ├── BoostTestRunner.cpp           # Master Boost.Test suite runner
│   └── README.md                     # Test framework documentation
└── robot/                            # Robot Framework acceptance tests
    ├── DirShareAcceptance.robot      # Main acceptance test suite
    ├── UserStories.robot             # User story validation (US1-US6)
    ├── PerformanceTests.robot        # Success criteria validation (SC-001 to SC-010)
    ├── EdgeCaseTests.robot           # Edge case scenarios from spec.md
    ├── keywords/                     # Custom Robot keywords
    │   ├── DirShareKeywords.robot    # DirShare-specific keywords (Start/Stop DirShare, Verify Sync)
    │   ├── FileOperations.robot      # File system operation keywords (Create File, Modify File, etc.)
    │   └── DDSKeywords.robot         # DDS-related keywords (Switch Discovery Mode, Verify DDS Cleanup)
    ├── libraries/                    # Python keyword libraries
    │   ├── DirShareLibrary.py        # Custom Python library for DirShare control
    │   └── ChecksumLibrary.py        # Checksum verification utilities
    ├── resources/                    # Test resources and data
    │   ├── test_files/               # Sample files for testing (various sizes, types)
    │   └── config/                   # Test configuration files
    ├── results/                      # Test execution results (gitignored)
    ├── requirements.txt              # Python dependencies (robotframework, robotframework-process)
    └── README.md                     # Robot Framework test documentation
```

**Structure Decision**: Single project structure following OpenDDS DevGuideExamples convention. All source files reside in `DevGuideExamples/DCPS/DirShare/` at repository root. This matches the standard pattern used by existing OpenDDS examples (Messenger, Hello World, etc.) and satisfies Constitution Principle V (Standard Project Structure).

**Test Strategy**: Three-tiered testing approach:
1. **Unit Tests** (tests/): Boost.Test for isolated component testing
2. **Integration Tests** (run_test.pl): PerlDDS framework for DDS pub-sub validation
3. **Acceptance Tests** (robot/): Robot Framework for end-to-end user story validation

This layered approach provides comprehensive coverage from unit-level (functions/classes) through integration (DDS messaging) to acceptance (full user scenarios).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

**Status**: ✅ **No violations detected**

All constitutional principles are satisfied without requiring exceptions or added complexity. The implementation follows standard OpenDDS example patterns with no deviations requiring justification.

**Note on Robot Framework Addition**: Robot Framework is added as a complementary acceptance testing layer and does not violate Test-Driven Validation (Principle IV). It enhances the existing Perl-based integration tests by providing:
- Human-readable test specifications matching user stories
- Better traceability between requirements and test cases
- Cross-platform acceptance testing without modifying core OpenDDS test patterns
