# Feature Specification: Multi-Participant Robustness Testing

**Feature Branch**: `002-robustness-testing`
**Created**: 2025-11-12
**Status**: Draft
**Input**: User description: "I want to create a set of robot framework tests that test the robustness of the application to dirshare shutdown and startup between multiple participants. For example 3 participants: Participant A, Participant B, and Participant C. the application should be robust enough to deal with files being added, while other participants are shut down and ensuring that they all have a synchronized directory at some point"

## Clarifications

### Session 2025-11-12

- Q: When a test fails due to timing (e.g., synchronization takes 12 seconds instead of 10), how should the test suite respond? → A: No retries - single execution determines pass/fail, accept 5% flakiness as environmental variation
- Q: Which edge cases must have explicit test coverage in this test suite? → A: Only test scenarios already covered by the 4 user stories; edge cases are documentation only
- Q: What is the maximum timeout for "eventually" in scenarios where sender restarts mid-transfer? → A: 60 seconds (allows for full discovery + large file retransmission)

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Participant Recovery After Shutdown (Priority: P1)

A DirShare participant is shut down while other participants continue making file changes. When the shutdown participant restarts, it must synchronize all missed changes and reach eventual consistency with other participants.

**Why this priority**: This is the core robustness requirement. If a participant cannot recover from being offline, the system is not production-ready. This represents the most common real-world scenario where network issues, crashes, or intentional shutdowns occur.

**Independent Test**: Can be fully tested with 2 participants by shutting down one, creating files on the other, restarting the first, and verifying synchronization. Delivers immediate value by validating the fundamental recovery mechanism.

**Acceptance Scenarios**:

1. **Given** three participants (A, B, C) are running and synchronized, **When** participant B is shut down, and participants A and C create new files, **Then** when B restarts, it receives all files created during its downtime within 10 seconds
2. **Given** participant A is shut down, **When** participant B creates a 5MB file and participant C modifies an existing file, **Then** when A restarts, it receives both the new file and the modified file with correct content and timestamps
3. **Given** two participants are shut down simultaneously, **When** the remaining participant creates multiple files, **Then** within 30 seconds after the last shutdown participant restarts, all three participants have identical directory contents

---

### User Story 2 - Concurrent Operations During Partial Network (Priority: P2)

Multiple participants perform file operations while some participants are offline. The system must handle file creation, modification, and deletion events in any order and reach eventual consistency without data loss.

**Why this priority**: This tests the resilience of the DDS event ordering and DirectorySnapshot mechanism. It's critical for multi-participant scenarios but builds on the basic recovery mechanism tested in P1.

**Independent Test**: Can be tested by orchestrating specific sequences of create/modify/delete operations across participants with controlled shutdown/startup timing, then verifying all participants converge to the same state.

**Acceptance Scenarios**:

1. **Given** participant A is offline, **When** participant B creates "file1.txt", participant C creates "file2.txt", and then B is shut down, **Then** when A and B restart, all three participants have both files with identical content
2. **Given** all participants are synchronized with "test.txt", **When** participant A is shut down, participant B modifies "test.txt", and participant C deletes "test.txt", **Then** when A restarts, all participants agree that "test.txt" is deleted (last operation wins)
3. **Given** participant B is offline, **When** participant A creates 10 files sequentially, then shuts down before B restarts, **Then** when B restarts, it receives all 10 files in the correct order with correct checksums

---

### User Story 3 - Rolling Participant Restarts (Priority: P3)

Participants are restarted in sequence (rolling restart pattern) while file operations continue. The system must maintain synchronization throughout the restart cycle without requiring all participants to be online simultaneously.

**Why this priority**: This simulates deployment scenarios and validates that the system can maintain operational continuity during maintenance windows. Less critical than basic recovery but important for production operations.

**Independent Test**: Can be tested by implementing a restart sequence script that cycles through participants while continuously creating test files, then validating final consistency.

**Acceptance Scenarios**:

1. **Given** three participants are synchronized, **When** participant A is restarted (shutdown then startup), immediately followed by B restart, then C restart, with file creation happening during each restart, **Then** after all restarts complete, all participants have identical directories
2. **Given** participants are in a rolling restart cycle, **When** 20 files are created across the restart window (distributed across different participants), **Then** within 30 seconds after the last restart completes, all participants have all 20 files
3. **Given** participant A restarts while B and C are actively creating files, **When** A comes online during this file creation activity, **Then** A receives both the historical snapshot and the real-time file events without duplicates or data loss

---

### User Story 4 - Large File Transfer During Participant Restart (Priority: P4)

A large file (>= 10MB using chunked transfer) is being synchronized when a participant restarts. The system must handle incomplete transfers gracefully and complete synchronization after restart.

**Why this priority**: This tests the robustness of the chunked transfer mechanism under failure conditions. Important for real-world usage but lower priority than basic recovery scenarios.

**Independent Test**: Can be tested by initiating a large file transfer, forcing a participant restart mid-transfer, then verifying the file completes successfully after restart.

**Acceptance Scenarios**:

1. **Given** participant A creates a 15MB file, and participant B begins receiving chunks, **When** participant B is shut down after receiving 50% of chunks, **Then** when B restarts, it receives the complete file with correct checksum within 15 seconds
2. **Given** participant A is sending a 20MB file to participants B and C, **When** participant C is shut down and restarted during chunk transfer, **Then** C receives the complete file without requiring retransmission of all chunks
3. **Given** a 25MB file transfer is in progress, **When** the sending participant is shut down mid-transfer, **Then** receiving participants receive the complete file with correct checksum within 60 seconds of the sender restarting

---

### Edge Cases

**Note**: The following edge cases are documented for awareness but do not require explicit test coverage beyond what is already validated in the 4 prioritized user stories above. These represent potential failure modes that may be addressed in future test iterations or require specialized infrastructure.

- What happens when a participant is offline for an extended period (hours) while thousands of files are created/modified?
- How does the system handle rapid shutdown/startup cycles (participant flapping)?
- What happens when two participants are shut down and the remaining participant creates files, then shuts down before the others restart (all offline simultaneously)?
- How does the system handle participants starting in different orders after a full cluster shutdown?
- What happens when a participant restarts with a corrupt or incomplete directory state?
- How does the system handle clock skew between participants after restart (timestamp conflicts)?
- What happens when a participant restarts with a full disk and cannot receive files?
- How does the system handle network partitions where some participants can communicate but not others?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Test suite MUST validate synchronization across at least 3 concurrent DirShare participants
- **FR-002**: Test suite MUST verify that a participant can be shut down gracefully and restarted while other participants continue operations
- **FR-003**: Test suite MUST verify that files created while a participant is offline are synchronized when that participant restarts
- **FR-004**: Test suite MUST verify that file modifications made during a participant's downtime are correctly received after restart
- **FR-005**: Test suite MUST verify that file deletions that occur during downtime are propagated to restarted participants
- **FR-006**: Test suite MUST verify eventual consistency - all participants reach identical directory states after all are online
- **FR-007**: Test suite MUST test scenarios where multiple participants are shut down simultaneously and restarted at different times
- **FR-008**: Test suite MUST verify synchronization of both small files (<10MB) and large files (>=10MB) during restart scenarios
- **FR-009**: Test suite MUST validate that restarted participants receive the DirectorySnapshot correctly and apply subsequent FileEvents
- **FR-010**: Test suite MUST verify that checksums match for all synchronized files after restart scenarios
- **FR-011**: Test suite MUST test timing scenarios where files are created immediately before, during, and immediately after participant shutdown/startup
- **FR-012**: Test suite MUST validate behavior when the last active participant shuts down (all participants offline scenario)
- **FR-013**: Test suite MUST verify that participants can be restarted in any order and still achieve synchronization
- **FR-014**: Test suite MUST test robustness under rapid shutdown/startup cycles of individual participants
- **FR-015**: Test suite MUST validate synchronization using both RTPS discovery mode (primary testing mode)
- **FR-016**: Test suite MUST execute each test case exactly once per run without automatic retries, allowing up to 5% failure rate to account for environmental variations
- **FR-017**: Test suite MUST enforce timeout expectations: 10 seconds for single participant restart, 30 seconds for multiple/rolling restarts, 60 seconds for sender restart during large file transfer

### Key Entities

- **Test Participant**: Represents a single DirShare process instance with its own monitored directory, identified by a unique label (A, B, C, etc.)
- **Synchronization State**: The collective state of all participants' directories at a given point in time, used to verify eventual consistency
- **Shutdown Event**: A controlled termination of a participant process, allowing testing of recovery scenarios
- **Restart Event**: The reinitialization of a previously shutdown participant, triggering DirectorySnapshot reception and event replay
- **File Operation Sequence**: An ordered series of file create/modify/delete operations performed across participants during test execution

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Test suite successfully validates that a restarted participant synchronizes all missed file operations within 10 seconds for directories containing up to 100 files
- **SC-002**: Test suite verifies eventual consistency is achieved in 100% of test scenarios, with all participants having identical directory contents and checksums
- **SC-003**: Test suite validates that large file transfers (15-25MB) complete successfully after participant restarts, with 100% checksum match rate
- **SC-004**: Test suite runs reliably with at least 95% pass rate across 10 consecutive executions, with each test executed once without retries (5% failure tolerance accounts for environmental timing variations)
- **SC-005**: Test suite covers at least 8 distinct shutdown/startup scenarios including single participant restart, multiple participant restart, rolling restarts, and full cluster restart
- **SC-006**: Test execution time remains under 15 minutes for the complete robustness test suite
- **SC-007**: Test suite identifies and reports synchronization failures within 30 seconds of expected consistency achievement
- **SC-008**: Test suite validates correct behavior for all acceptance scenarios defined in the 4 prioritized user stories, covering timing conflicts, missing participants, and interrupted transfers

## Assumptions

- DirShare participants are running in RTPS discovery mode (standard configuration)
- Test environment has sufficient network bandwidth for large file transfers (at least 10 Mbps)
- Participant shutdown is graceful (SIGTERM) unless testing crash scenarios specifically
- Test directories start empty before each test to ensure clean state
- Filesystem operations complete within expected timeframes (no disk I/O bottlenecks)
- Clock synchronization between test participants is within 5 seconds (reasonable for NTP-synchronized systems)
- Test framework (Robot Framework) has capability to manage multiple concurrent processes
- OpenDDS environment is correctly configured and sourced before test execution

## Out of Scope

- Testing with more than 5 concurrent participants (scalability testing is separate concern)
- Performance benchmarking or latency measurements (focused on correctness, not performance)
- Testing InfoRepo discovery mode (RTPS is the recommended and primary mode)
- Network simulation or bandwidth throttling (assumes reliable network)
- Disk space exhaustion scenarios (assumes adequate storage)
- Security testing or authentication mechanisms (DirShare operates in trusted environment)
- Testing with non-standard QoS settings or custom DDS configurations
- Cross-platform testing (tests will run on the development platform only)
- Automated recovery from corrupted state (assumes clean participant state)
