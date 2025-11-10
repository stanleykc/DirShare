<!--
Sync Impact Report - 2025-10-31

Version Change: 1.0.0 → 1.1.0
Type: MINOR - Added Version Control Hygiene principle

Modified Principles:
- V. Standard Project Structure - Added `.gitignore` requirement
- NEW: VI. Version Control Hygiene - Comprehensive .gitignore requirements

Added Sections:
- Version Control Hygiene principle with detailed .gitignore patterns

Templates Requiring Updates:
✅ plan-template.md - Constitution Check section compatible
✅ spec-template.md - Requirements structure compatible
✅ tasks-template.md - Task organization compatible

Follow-up TODOs: None

Previous Sync Report - 2025-10-30:
Version Change: [INITIAL] → 1.0.0
Type: MINOR - Initial constitution creation for OpenDDS examples
Modified Principles: I-V (IDL-First, Dual Discovery, Complete DDS Lifecycle, Test-Driven Validation, Standard Project Structure)
-->

# OpenDDS Example Constitution

## Core Principles

### I. IDL-First Design

Every OpenDDS example MUST define data structures in IDL before implementation. IDL files are the contract between publishers and subscribers and drive code generation.

**Requirements**:
- IDL file defines all data types using OMG IDL syntax
- Structs intended as topics MUST have `@topic` annotation
- Key fields MUST be marked with `@key` annotation
- TypeSupport implementation generated via build system (MPC or CMake)
- No manual implementation of TypeSupport code

**Rationale**: IDL-first ensures type safety, enables language interoperability, and leverages OpenDDS code generation. This prevents type mismatches between publishers and subscribers and ensures compliance with DDS standards.

### II. Dual Discovery/Transport Support (NON-NEGOTIABLE)

Every example MUST support both InfoRepo and RTPS discovery/transport configurations without code changes.

**Requirements**:
- Default configuration uses InfoRepo discovery + TCP transport
- RTPS configuration uses RTPS discovery + RTPS/UDP transport
- Configuration switched via INI files or command-line arguments
- `run_test.pl` MUST support `--rtps` flag
- Include `rtps.ini` configuration file
- Static builds MUST conditionally include required discovery/transport headers

**Rationale**: Demonstrates OpenDDS flexibility and DDS-standard RTPS interoperability. InfoRepo shows OpenDDS-specific features; RTPS demonstrates standards compliance and peer-to-peer discovery patterns.

### III. Complete DDS Lifecycle

Examples MUST demonstrate the full DDS entity lifecycle including proper initialization, synchronization, data exchange, and cleanup.

**Requirements**:
- Initialize DomainParticipantFactory with `TheParticipantFactoryWithArgs(argc, argv)`
- Create DomainParticipant with explicit domain ID
- Register TypeSupport for all IDL-defined types
- Create Topic with meaningful name
- Publishers: Create Publisher → DataWriter → wait for subscriber match → write samples → wait for acknowledgments
- Subscribers: Create Subscriber → DataReader with Listener → wait for publisher match → process data → wait for publisher disconnect
- Use WaitSet and StatusCondition for synchronization
- Cleanup in reverse order: delete_contained_entities() → delete_participant() → shutdown()
- Handle all DDS return codes with ACE_ERROR macros

**Rationale**: Shows correct DDS patterns, prevents resource leaks, demonstrates synchronization best practices, and teaches proper error handling. Complete lifecycle examples serve as templates for production applications.

### IV. Test-Driven Validation

All components must be validated through automated unit tests using Boost to ensure correct functionality and prevent regressions.

Every example MUST include automated testing via Perl launcher scripts that validate publisher-subscriber communication.



**Requirements**:
- Provide `run_test.pl` using PerlDDS::Run_Test framework
- Test MUST start subscriber before publisher (standard pattern)
- Test MUST validate both InfoRepo and RTPS modes
- Use `--rtps` flag to switch discovery/transport configuration
- Set appropriate timeouts (typically 120 seconds)
- Configure debug levels for troubleshooting (`dcps_debug_level`, `dcps_transport_debug_level`)
- Test script MUST return 0 on success, non-zero on failure

**Rationale**: Automated tests ensure examples remain functional across OpenDDS releases, demonstrate correct startup ordering, and provide diagnostic output patterns for debugging real applications.

### V. Standard Project Structure

Examples MUST follow consistent file organization and build system patterns for discoverability and reusability.

**Requirements**:
- **IDL**: `[ExampleName].idl` - data type definitions
- **MPC**: `[ExampleName].mpc` - MPC build project with three sub-projects:
  - `*idl`: TypeSupport generation (custom_only = 1)
  - `*publisher`: Publisher executable (depends on *idl)
  - `*subscriber`: Subscriber executable (depends on *publisher for ordering)
- **CMake**: `CMakeLists.txt` - Alternative build using `find_package(OpenDDS REQUIRED)`
- **Publisher**: `Publisher.cpp` - Publishing application
- **Subscriber**: `Subscriber.cpp` + `DataReaderListenerImpl.{h,cpp}` - Subscribing application with listener
- **Test**: `run_test.pl` - Perl test launcher
- **Config**: `rtps.ini` - RTPS discovery/transport configuration
- **Docs**: `README.md` - Brief description, build instructions, run instructions
- **Git Ignore**: `.gitignore` - Ignore patterns for generated files and binaries (see requirements below)

**Rationale**: Consistency enables developers to quickly understand any example, copy patterns to new projects, and switch between build systems. Standard structure supports documentation generation and automated testing.

### VI. Version Control Hygiene

Examples MUST maintain clean version control by excluding generated files, build artifacts, and binary outputs from git tracking.

**Requirements**:
- Every example directory MUST include a `.gitignore` file
- `.gitignore` MUST ignore all IDL compiler-generated files:
  - `*TypeSupportImpl.cpp`, `*TypeSupportImpl.h`
  - `*TypeSupport.idl`
  - MUST explicitly ignore file names for generated files that match this pattern.`*C.{h,cpp,inl}`, `*S.{h,cpp,inl}`
  - `*TypeSupportC.{h,cpp,inl}`, `*TypeSupportS.{h,cpp,inl}`
- `.gitignore` MUST ignore all executable and library outputs:
  - Application executables (by name, e.g., `/publisher`, `/subscriber`, `/dirshare`)
  - Shared libraries (`lib*.so`, `lib*.so.*`, `lib*.dylib`, `lib*.a`)
  - Test executables (e.g., `/tests/*BoostTest`, `/tests/*Test`)
- `.gitignore` MUST ignore build artifacts:
  - Object files (`*.o`)
  - Build directories (`.obj/`, `.shobj/`)
  - Dependency files (`.depend.*`)
  - Generated makefiles (`GNUmakefile*`)
- `.gitignore` MUST ignore temporary and test-generated files:
  - Log files (`*.log`)
  - IOR files (`*.ior`)
  - Core dumps (`core`, `core.*`)
  - Test directories (`test_*/`)
- When adding new executables or libraries, `.gitignore` MUST be updated BEFORE committing code
- Never commit binary executables, shared libraries, or IDL-generated code to the repository

**Rationale**: Generated files and binaries should not be version-controlled as they are build artifacts that vary by platform, configuration, and OpenDDS version. Tracking them causes merge conflicts, bloats repository size, and creates confusion about source-of-truth. Clean version control ensures examples remain portable and maintainable across different development environments.

## Build System Requirements

### MPC (Primary)

- Use `dcpsexe` base project for executables
- Include `dcps_tcp` and `dcps_rtps_udp` transport features
- Set `requires += no_opendds_safety_profile` if using dynamic features
- Use `TypeSupport_Files` section for IDL compilation
- Maintain dependency order: idl → publisher → subscriber

### CMake (Alternative)

- Use `find_package(OpenDDS REQUIRED)` for OpenDDS integration
- Include `opendds_testing` module for test integration
- Create IDL library with `opendds_target_sources([target] PUBLIC [idl_file])`
- Link executables against: `OpenDDS::Dcps`, `OpenDDS::InfoRepoDiscovery`, `OpenDDS::Tcp`, `OpenDDS::Rtps`, `OpenDDS::Rtps_Udp`
- Use `opendds_add_test()` for test registration

### Environment

- Examples assume `setenv.sh` (or `setenv.cmd`) has been sourced
- `DDS_ROOT` and `ACE_ROOT` environment variables MUST be set
- Build from example directory after OpenDDS core is built

## Testing Requirements

### Test Execution

- All tests MUST pass in both InfoRepo and RTPS modes
- Tests run in CI/CD environments
- Provide meaningful error messages using ACE_ERROR macros
- Log debug output to aid troubleshooting

### Test Script Pattern

```perl
my $test = new PerlDDS::TestFramework();
$test->{dcps_debug_level} = 4;
$test->{dcps_transport_debug_level} = 2;
$test->setup_discovery() unless $rtps;  # Only for InfoRepo mode
$test->process("publisher", "publisher", $opts);
$test->process("subscriber", "subscriber", $opts);
$test->start_process("subscriber");     # Start subscriber first
$test->start_process("publisher");
exit $test->finish(120);                # 120 second timeout
```

## Governance

This constitution defines the standards for OpenDDS examples in the `DevGuideExamples/` directory. Examples demonstrate correct DDS usage patterns and serve as templates for production applications.

**Compliance**:
- All new examples MUST adhere to these principles before merging
- Existing examples SHOULD be updated to comply during maintenance
- Deviations require documented justification in example README
- Examples failing automated tests MUST be fixed or removed

**Amendment Process**:
- Constitution changes require pull request with rationale
- Changes that affect build systems, test patterns, or standard structure require MINOR version bump
- Clarifications and documentation improvements require PATCH version bump
- Breaking changes to example structure or requirements require MAJOR version bump

**Versioning**:
- Constitution follows semantic versioning (MAJOR.MINOR.PATCH)
- Version tracked in this file and referenced in `/speckit.plan` Constitution Check sections
- Plan templates verify examples against current constitution version

**Review**:
- Example pull requests MUST include Constitution Check validation
- Reviewers verify: IDL-first design, dual discovery support, complete lifecycle, test coverage, standard structure
- Complexity not justified by teaching value MUST be simplified

**Version**: 1.1.0 | **Ratified**: 2025-10-30 | **Last Amended**: 2025-10-31
