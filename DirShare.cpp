#include "DirShareTypeSupportImpl.h"
#include "FileMonitor.h"
#include "FileChangeTracker.h"
#include "FileUtils.h"
#include "Checksum.h"
#include "SnapshotListenerImpl.h"
#include "FileContentListenerImpl.h"
#include "FileChunkListenerImpl.h"
#include "FileEventListenerImpl.h"

#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DCPS/StaticIncludes.h>

#include <ace/Log_Msg.h>
#include <ace/OS_NS_unistd.h>
#include <ace/Get_Opt.h>
#include <ace/UUID.h>
#include <ace/Time_Value.h>

#if OPENDDS_DO_MANUAL_STATIC_INCLUDES
#  include <dds/DCPS/RTPS/RtpsDiscovery.h>
#  include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#endif

#include <string>
#include <iostream>

// Configuration constants
const int DEFAULT_DOMAIN_ID = 42;
const int POLL_INTERVAL_SEC = 2; // 2 second polling interval

// Global shared directory path
std::string g_shared_directory;

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  int return_code = 0;

  try {
    // Initialize DDS DomainParticipantFactory (this processes -DCPS* options)
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    // Parse remaining command-line arguments (after DDS options are processed)
    ACE_Get_Opt get_opts(argc, argv, ACE_TEXT("h"));
    int option;
    while ((option = get_opts()) != EOF) {
      switch (option) {
      case 'h':
      default:
        ACE_ERROR_RETURN((LM_ERROR,
                         ACE_TEXT("Usage: %C [DDS options] <shared_directory>\n")
                         ACE_TEXT("Options:\n")
                         ACE_TEXT("  -h                  Show this help message\n")
                         ACE_TEXT("  -DCPSConfigFile <file> Specify DDS configuration file (e.g., rtps.ini)\n")
                         ACE_TEXT("  -DCPSInfoRepo <ior>    Specify DCPSInfoRepo IOR (InfoRepo mode)\n")
                         ACE_TEXT("\n")
                         ACE_TEXT("Example (InfoRepo mode):\n")
                         ACE_TEXT("  %C -DCPSInfoRepo file://repo.ior /path/to/shared_dir\n")
                         ACE_TEXT("\n")
                         ACE_TEXT("Example (RTPS mode):\n")
                         ACE_TEXT("  %C -DCPSConfigFile rtps.ini /path/to/shared_dir\n"),
                         argv[0], argv[0], argv[0]),
                        1);
      }
    }

    // Get shared directory path from remaining arguments
    if (get_opts.opt_ind() >= argc) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Missing required <shared_directory> argument\n")
                       ACE_TEXT("Usage: %C [DDS options] <shared_directory>\n"),
                       argv[0]),
                      1);
    }

    g_shared_directory = ACE_TEXT_ALWAYS_CHAR(argv[get_opts.opt_ind()]);

    // Validate shared directory exists and is a directory
    if (!DirShare::is_directory(g_shared_directory)) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Specified path is not a directory: %C\n"),
                       g_shared_directory.c_str()),
                      1);
    }

    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) DirShare starting...\n")
               ACE_TEXT("  Monitoring directory: %C\n")
               ACE_TEXT("  Poll interval: %d seconds\n"),
               g_shared_directory.c_str(),
               POLL_INTERVAL_SEC));

    // Create DomainParticipant
    DDS::DomainParticipant_var participant =
      dpf->create_participant(DEFAULT_DOMAIN_ID,
                             PARTICIPANT_QOS_DEFAULT,
                             0,
                             OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!participant) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_participant failed!\n")),
                      1);
    }

    // Register TypeSupport for FileEvent
    DirShare::FileEventTypeSupport_var ts_event =
      new DirShare::FileEventTypeSupportImpl;

    if (ts_event->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: register_type FileEvent failed!\n")),
                      1);
    }

    // Register TypeSupport for FileContent
    DirShare::FileContentTypeSupport_var ts_content =
      new DirShare::FileContentTypeSupportImpl;

    if (ts_content->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: register_type FileContent failed!\n")),
                      1);
    }

    // Register TypeSupport for FileChunk
    DirShare::FileChunkTypeSupport_var ts_chunk =
      new DirShare::FileChunkTypeSupportImpl;

    if (ts_chunk->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: register_type FileChunk failed!\n")),
                      1);
    }

    // Register TypeSupport for DirectorySnapshot
    DirShare::DirectorySnapshotTypeSupport_var ts_snapshot =
      new DirShare::DirectorySnapshotTypeSupportImpl;

    if (ts_snapshot->register_type(participant, "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: register_type DirectorySnapshot failed!\n")),
                      1);
    }

    // Get type names
    CORBA::String_var type_name_event = ts_event->get_type_name();
    CORBA::String_var type_name_content = ts_content->get_type_name();
    CORBA::String_var type_name_chunk = ts_chunk->get_type_name();
    CORBA::String_var type_name_snapshot = ts_snapshot->get_type_name();

    // Set QoS for RELIABLE and TRANSIENT_LOCAL for FileEvents topic
    DDS::TopicQos topic_qos_events;
    participant->get_default_topic_qos(topic_qos_events);
    topic_qos_events.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    topic_qos_events.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
    topic_qos_events.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
    topic_qos_events.history.depth = 100;

    // Create FileEvents Topic
    DDS::Topic_var topic_events =
      participant->create_topic("DirShare_FileEvents",
                               type_name_event,
                               topic_qos_events,
                               0,
                               OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!topic_events) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_topic FileEvents failed!\n")),
                      1);
    }

    // Set QoS for RELIABLE and VOLATILE for FileContent topic
    DDS::TopicQos topic_qos_content;
    participant->get_default_topic_qos(topic_qos_content);
    topic_qos_content.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    topic_qos_content.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    topic_qos_content.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
    topic_qos_content.history.depth = 1;

    // Create FileContent Topic
    DDS::Topic_var topic_content =
      participant->create_topic("DirShare_FileContent",
                               type_name_content,
                               topic_qos_content,
                               0,
                               OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!topic_content) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_topic FileContent failed!\n")),
                      1);
    }

    // Set QoS for RELIABLE and VOLATILE with KEEP_ALL for FileChunks topic
    DDS::TopicQos topic_qos_chunks;
    participant->get_default_topic_qos(topic_qos_chunks);
    topic_qos_chunks.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    topic_qos_chunks.durability.kind = DDS::VOLATILE_DURABILITY_QOS;
    topic_qos_chunks.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    topic_qos_chunks.resource_limits.max_samples = 1000;
    topic_qos_chunks.resource_limits.max_instances = 100;
    topic_qos_chunks.resource_limits.max_samples_per_instance = 1000;

    // Create FileChunks Topic
    DDS::Topic_var topic_chunks =
      participant->create_topic("DirShare_FileChunks",
                               type_name_chunk,
                               topic_qos_chunks,
                               0,
                               OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!topic_chunks) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_topic FileChunks failed!\n")),
                      1);
    }

    // Set QoS for RELIABLE and TRANSIENT_LOCAL for DirectorySnapshot topic
    DDS::TopicQos topic_qos_snapshot;
    participant->get_default_topic_qos(topic_qos_snapshot);
    topic_qos_snapshot.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    topic_qos_snapshot.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
    topic_qos_snapshot.history.kind = DDS::KEEP_LAST_HISTORY_QOS;
    topic_qos_snapshot.history.depth = 1;

    // Create DirectorySnapshot Topic
    DDS::Topic_var topic_snapshot =
      participant->create_topic("DirShare_DirectorySnapshot",
                               type_name_snapshot,
                               topic_qos_snapshot,
                               0,
                               OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!topic_snapshot) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_topic DirectorySnapshot failed!\n")),
                      1);
    }

    // Create Publisher
    DDS::Publisher_var publisher =
      participant->create_publisher(PUBLISHER_QOS_DEFAULT,
                                   0,
                                   OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!publisher) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_publisher failed!\n")),
                      1);
    }

    // Create Subscriber
    DDS::Subscriber_var subscriber =
      participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                                    0,
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!subscriber) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_subscriber failed!\n")),
                      1);
    }

    // Create WaitSet for synchronization
    DDS::WaitSet_var ws = new DDS::WaitSet;

    // Attach StatusCondition for Publisher to detect publication matched
    DDS::StatusCondition_var condition = publisher->get_statuscondition();
    condition->set_enabled_statuses(DDS::PUBLICATION_MATCHED_STATUS);
    ws->attach_condition(condition);

    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) DDS infrastructure initialized successfully\n")
               ACE_TEXT("  Domain ID: %d\n")
               ACE_TEXT("  Topics created: FileEvents, FileContent, FileChunks, DirectorySnapshot\n"),
               DEFAULT_DOMAIN_ID));

    // Create DataWriters for publishing
    DDS::DataWriter_var event_writer =
      publisher->create_datawriter(topic_events,
                                   DATAWRITER_QOS_DEFAULT,
                                   0,
                                   OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!event_writer) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datawriter FileEvent failed!\n")),
                      1);
    }

    DDS::DataWriter_var snapshot_writer =
      publisher->create_datawriter(topic_snapshot,
                                   DATAWRITER_QOS_DEFAULT,
                                   0,
                                   OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!snapshot_writer) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datawriter DirectorySnapshot failed!\n")),
                      1);
    }

    DDS::DataWriter_var content_writer =
      publisher->create_datawriter(topic_content,
                                   DATAWRITER_QOS_DEFAULT,
                                   0,
                                   OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!content_writer) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datawriter FileContent failed!\n")),
                      1);
    }

    DDS::DataWriter_var chunk_writer =
      publisher->create_datawriter(topic_chunks,
                                   DATAWRITER_QOS_DEFAULT,
                                   0,
                                   OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!chunk_writer) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datawriter FileChunk failed!\n")),
                      1);
    }

    // Narrow to typed writers
    DirShare::FileEventDataWriter_var typed_event_writer =
      DirShare::FileEventDataWriter::_narrow(event_writer);
    DirShare::DirectorySnapshotDataWriter_var typed_snapshot_writer =
      DirShare::DirectorySnapshotDataWriter::_narrow(snapshot_writer);
    DirShare::FileContentDataWriter_var typed_content_writer =
      DirShare::FileContentDataWriter::_narrow(content_writer);
    DirShare::FileChunkDataWriter_var typed_chunk_writer =
      DirShare::FileChunkDataWriter::_narrow(chunk_writer);

    // Create FileChangeTracker for notification loop prevention (SC-011)
    DirShare::FileChangeTracker change_tracker;

    // Create listeners for receiving data
    DDS::DataReaderListener_var event_listener =
      new DirShare::FileEventListenerImpl(g_shared_directory, content_writer, chunk_writer, change_tracker);
    DDS::DataReaderListener_var snapshot_listener =
      new DirShare::SnapshotListenerImpl(g_shared_directory, content_writer, chunk_writer);
    DDS::DataReaderListener_var content_listener =
      new DirShare::FileContentListenerImpl(g_shared_directory);
    DDS::DataReaderListener_var chunk_listener =
      new DirShare::FileChunkListenerImpl(g_shared_directory);

    // Create DataReaders with listeners
    DDS::DataReader_var event_reader =
      subscriber->create_datareader(topic_events,
                                    DATAREADER_QOS_DEFAULT,
                                    event_listener,
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!event_reader) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datareader FileEvent failed!\n")),
                      1);
    }

    DDS::DataReader_var snapshot_reader =
      subscriber->create_datareader(topic_snapshot,
                                    DATAREADER_QOS_DEFAULT,
                                    snapshot_listener,
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!snapshot_reader) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datareader DirectorySnapshot failed!\n")),
                      1);
    }

    DDS::DataReader_var content_reader =
      subscriber->create_datareader(topic_content,
                                    DATAREADER_QOS_DEFAULT,
                                    content_listener,
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!content_reader) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datareader FileContent failed!\n")),
                      1);
    }

    DDS::DataReader_var chunk_reader =
      subscriber->create_datareader(topic_chunks,
                                    DATAREADER_QOS_DEFAULT,
                                    chunk_listener,
                                    OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!chunk_reader) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: create_datareader FileChunk failed!\n")),
                      1);
    }

    // Wait for discovery - wait for publication/subscription matching
    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) Waiting for participant discovery...\n")));

    DDS::Duration_t timeout = {30, 0}; // 30 second timeout
    DDS::ConditionSeq conditions;
    DDS::ReturnCode_t ret = ws->wait(conditions, timeout);

    if (ret == DDS::RETCODE_TIMEOUT) {
      ACE_DEBUG((LM_INFO,
                 ACE_TEXT("(%P|%t) No other participants discovered yet, continuing...\n")));
    } else if (ret != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: WaitSet wait failed: %d\n"),
                       ret),
                      1);
    }

    // Create FileMonitor for directory scanning
    DirShare::FileMonitor monitor(g_shared_directory, change_tracker);

    // Generate and publish initial directory snapshot
    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) Publishing initial directory snapshot...\n")));

    DirShare::DirectorySnapshot snapshot;

    // Generate unique participant ID using UUID
    ACE_Utils::UUID uuid;
    ACE_Utils::UUID_GENERATOR::instance()->generate_UUID(uuid);
    snapshot.participant_id = uuid.to_string()->c_str();

    // Get all files in directory
    std::vector<DirShare::FileMetadata> file_list = monitor.get_all_files();
    snapshot.files.length(static_cast<CORBA::ULong>(file_list.size()));
    for (size_t i = 0; i < file_list.size(); ++i) {
      snapshot.files[static_cast<CORBA::ULong>(i)] = file_list[i];
    }

    // Set timestamp
    ACE_Time_Value now = ACE_OS::gettimeofday();
    snapshot.snapshot_time_sec = static_cast<CORBA::ULongLong>(now.sec());
    snapshot.snapshot_time_nsec = static_cast<CORBA::ULong>(now.usec() * 1000);
    snapshot.file_count = static_cast<CORBA::ULong>(file_list.size());

    // Publish snapshot
    ret = typed_snapshot_writer->write(snapshot, DDS::HANDLE_NIL);
    if (ret != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: write DirectorySnapshot failed: %d\n"),
                       ret),
                      1);
    }

    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) Initial snapshot published: %u files\n"),
               snapshot.file_count));

    // Publish initial file contents
    for (size_t i = 0; i < file_list.size(); ++i) {
      const DirShare::FileMetadata& metadata = file_list[i];
      std::string filename = metadata.filename.in();
      std::string full_path = g_shared_directory + "/" + filename;

      // Determine if file should be sent as chunks or content
      const uint64_t CHUNK_THRESHOLD = 10 * 1024 * 1024; // 10MB

      if (metadata.size < CHUNK_THRESHOLD) {
        // Send as FileContent (small file)
        DirShare::FileContent content;
        content.filename = metadata.filename;
        content.size = metadata.size;
        content.checksum = metadata.checksum;
        content.timestamp_sec = metadata.timestamp_sec;
        content.timestamp_nsec = metadata.timestamp_nsec;

        // Read file data
        std::vector<uint8_t> data;
        if (!DirShare::read_file(full_path, data)) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                     full_path.c_str()));
          continue;
        }

        content.data.length(static_cast<CORBA::ULong>(data.size()));
        std::memcpy(content.data.get_buffer(), &data[0], data.size());

        ret = typed_content_writer->write(content, DDS::HANDLE_NIL);
        if (ret != DDS::RETCODE_OK) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("ERROR: %N:%l: write FileContent failed: %d\n"),
                     ret));
        } else {
          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) Published FileContent: %C (%Q bytes)\n"),
                     filename.c_str(),
                     metadata.size));
        }
      } else {
        // Send as FileChunks (large file)
        const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
        uint32_t total_chunks = static_cast<uint32_t>((metadata.size + CHUNK_SIZE - 1) / CHUNK_SIZE);

        ACE_DEBUG((LM_INFO,
                   ACE_TEXT("(%P|%t) Publishing FileChunks for: %C (%Q bytes, %u chunks)\n"),
                   filename.c_str(),
                   metadata.size,
                   total_chunks));

        // Read entire file
        std::vector<uint8_t> file_data;
        if (!DirShare::read_file(full_path, file_data)) {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                     full_path.c_str()));
          continue;
        }

        // Send chunks
        for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
          DirShare::FileChunk chunk;
          chunk.filename = metadata.filename;
          chunk.chunk_id = chunk_id;
          chunk.total_chunks = total_chunks;
          chunk.file_size = metadata.size;
          chunk.file_checksum = metadata.checksum;
          chunk.timestamp_sec = metadata.timestamp_sec;
          chunk.timestamp_nsec = metadata.timestamp_nsec;

          // Calculate chunk data
          uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
          uint32_t this_chunk_size = static_cast<uint32_t>(
            (offset + CHUNK_SIZE > metadata.size) ?
            (metadata.size - offset) : CHUNK_SIZE);

          chunk.data.length(this_chunk_size);
          std::memcpy(chunk.data.get_buffer(), &file_data[offset], this_chunk_size);

          // Calculate chunk checksum
          chunk.chunk_checksum = DirShare::compute_checksum(
            &file_data[offset], this_chunk_size);

          ret = typed_chunk_writer->write(chunk, DDS::HANDLE_NIL);
          if (ret != DDS::RETCODE_OK) {
            ACE_ERROR((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: write FileChunk failed: %d\n"),
                       ret));
            break;
          }

          // Small delay to avoid overwhelming UDP send buffer
          ACE_Time_Value delay(0, 10000); // 10ms
          ACE_OS::sleep(delay);
        }

        ACE_DEBUG((LM_INFO,
                   ACE_TEXT("(%P|%t) Completed publishing chunks for: %C\n"),
                   filename.c_str()));
      }
    }

    ACE_DEBUG((LM_INFO,
               ACE_TEXT("(%P|%t) DirShare running. Monitoring: %C\n")
               ACE_TEXT("  Press Ctrl+C to exit.\n"),
               g_shared_directory.c_str()));

    // Main monitoring loop
    while (true) {
      ACE_OS::sleep(POLL_INTERVAL_SEC);

      // Phase 4: Detect file changes and publish FileEvents
      std::vector<std::string> created_files;
      std::vector<std::string> modified_files;
      std::vector<std::string> deleted_files;

      if (monitor.scan_for_changes(created_files, modified_files, deleted_files)) {
        // Handle created files (Phase 4)
        for (size_t i = 0; i < created_files.size(); ++i) {
          const std::string& filename = created_files[i];
          std::string full_path = g_shared_directory + "/" + filename;

          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) File CREATE detected: %C\n"),
                     filename.c_str()));

          // Get file metadata
          DirShare::FileMetadata metadata;
          if (!monitor.get_file_metadata(filename, metadata)) {
            ACE_ERROR((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Failed to get metadata for: %C\n"),
                       filename.c_str()));
            continue;
          }

          // Create and publish FileEvent(CREATE)
          DirShare::FileEvent event;
          event.filename = metadata.filename;
          event.operation = DirShare::CREATE;
          ACE_Time_Value now = ACE_OS::gettimeofday();
          event.timestamp_sec = static_cast<CORBA::ULongLong>(now.sec());
          event.timestamp_nsec = static_cast<CORBA::ULong>(now.usec() * 1000);
          event.metadata = metadata;

          ret = typed_event_writer->write(event, DDS::HANDLE_NIL);
          if (ret != DDS::RETCODE_OK) {
            ACE_ERROR((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Failed to publish FileEvent(CREATE): %d\n"),
                       ret));
            continue;
          }

          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) Published FileEvent(CREATE) for: %C\n"),
                     filename.c_str()));

          // Publish file content
          const uint64_t CHUNK_THRESHOLD = 10 * 1024 * 1024; // 10MB

          if (metadata.size < CHUNK_THRESHOLD) {
            // Send as FileContent (small file)
            DirShare::FileContent content;
            content.filename = metadata.filename;
            content.size = metadata.size;
            content.checksum = metadata.checksum;
            content.timestamp_sec = metadata.timestamp_sec;
            content.timestamp_nsec = metadata.timestamp_nsec;

            // Read file data
            std::vector<uint8_t> data;
            if (!DirShare::read_file(full_path, data)) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                         full_path.c_str()));
              continue;
            }

            content.data.length(static_cast<CORBA::ULong>(data.size()));
            std::memcpy(content.data.get_buffer(), &data[0], data.size());

            ret = typed_content_writer->write(content, DDS::HANDLE_NIL);
            if (ret != DDS::RETCODE_OK) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: write FileContent failed: %d\n"),
                         ret));
            } else {
              ACE_DEBUG((LM_INFO,
                         ACE_TEXT("(%P|%t) Published FileContent for: %C (%Q bytes)\n"),
                         filename.c_str(),
                         metadata.size));
            }
          } else {
            // Send as FileChunks (large file)
            const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
            uint32_t total_chunks = static_cast<uint32_t>((metadata.size + CHUNK_SIZE - 1) / CHUNK_SIZE);

            ACE_DEBUG((LM_INFO,
                       ACE_TEXT("(%P|%t) Publishing FileChunks for: %C (%Q bytes, %u chunks)\n"),
                       filename.c_str(),
                       metadata.size,
                       total_chunks));

            // Read entire file
            std::vector<uint8_t> file_data;
            if (!DirShare::read_file(full_path, file_data)) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                         full_path.c_str()));
              continue;
            }

            // Send chunks
            for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
              DirShare::FileChunk chunk;
              chunk.filename = metadata.filename;
              chunk.chunk_id = chunk_id;
              chunk.total_chunks = total_chunks;
              chunk.file_size = metadata.size;
              chunk.file_checksum = metadata.checksum;
              chunk.timestamp_sec = metadata.timestamp_sec;
              chunk.timestamp_nsec = metadata.timestamp_nsec;

              // Calculate chunk data
              uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
              uint32_t this_chunk_size = static_cast<uint32_t>(
                (offset + CHUNK_SIZE > metadata.size) ?
                (metadata.size - offset) : CHUNK_SIZE);

              chunk.data.length(this_chunk_size);
              std::memcpy(chunk.data.get_buffer(), &file_data[offset], this_chunk_size);

              // Calculate chunk checksum
              chunk.chunk_checksum = DirShare::compute_checksum(
                &file_data[offset], this_chunk_size);

              ret = typed_chunk_writer->write(chunk, DDS::HANDLE_NIL);
              if (ret != DDS::RETCODE_OK) {
                ACE_ERROR((LM_ERROR,
                           ACE_TEXT("ERROR: %N:%l: write FileChunk failed: %d\n"),
                           ret));
                break;
              }

              // Small delay to avoid overwhelming UDP send buffer
              ACE_Time_Value delay(0, 10000); // 10ms
              ACE_OS::sleep(delay);
            }

            ACE_DEBUG((LM_INFO,
                       ACE_TEXT("(%P|%t) Completed publishing chunks for: %C\n"),
                       filename.c_str()));
          }
        }

        // Handle modified files (Phase 5)
        for (size_t i = 0; i < modified_files.size(); ++i) {
          const std::string& filename = modified_files[i];
          std::string full_path = g_shared_directory + "/" + filename;

          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) File MODIFY detected: %C\n"),
                     filename.c_str()));

          // Get file metadata
          DirShare::FileMetadata metadata;
          if (!monitor.get_file_metadata(filename, metadata)) {
            ACE_ERROR((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Failed to get metadata for: %C\n"),
                       filename.c_str()));
            continue;
          }

          // Create and publish FileEvent(MODIFY)
          DirShare::FileEvent event;
          event.filename = metadata.filename;
          event.operation = DirShare::MODIFY;
          ACE_Time_Value now = ACE_OS::gettimeofday();
          event.timestamp_sec = static_cast<CORBA::ULongLong>(now.sec());
          event.timestamp_nsec = static_cast<CORBA::ULong>(now.usec() * 1000);
          event.metadata = metadata;

          ret = typed_event_writer->write(event, DDS::HANDLE_NIL);
          if (ret != DDS::RETCODE_OK) {
            ACE_ERROR((LM_ERROR,
                       ACE_TEXT("ERROR: %N:%l: Failed to publish FileEvent(MODIFY): %d\n"),
                       ret));
            continue;
          }

          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) Published FileEvent(MODIFY) for: %C\n"),
                     filename.c_str()));

          // Publish updated file content
          const uint64_t CHUNK_THRESHOLD = 10 * 1024 * 1024; // 10MB

          if (metadata.size < CHUNK_THRESHOLD) {
            // Send as FileContent (small file)
            DirShare::FileContent content;
            content.filename = metadata.filename;
            content.size = metadata.size;
            content.checksum = metadata.checksum;
            content.timestamp_sec = metadata.timestamp_sec;
            content.timestamp_nsec = metadata.timestamp_nsec;

            // Read file data
            std::vector<uint8_t> data;
            if (!DirShare::read_file(full_path, data)) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                         full_path.c_str()));
              continue;
            }

            content.data.length(static_cast<CORBA::ULong>(data.size()));
            std::memcpy(content.data.get_buffer(), &data[0], data.size());

            ret = typed_content_writer->write(content, DDS::HANDLE_NIL);
            if (ret != DDS::RETCODE_OK) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: write FileContent failed: %d\n"),
                         ret));
            } else {
              ACE_DEBUG((LM_INFO,
                         ACE_TEXT("(%P|%t) Published FileContent for MODIFY: %C (%Q bytes)\n"),
                         filename.c_str(),
                         metadata.size));
            }
          } else {
            // Send as FileChunks (large file)
            const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
            uint32_t total_chunks = static_cast<uint32_t>((metadata.size + CHUNK_SIZE - 1) / CHUNK_SIZE);

            ACE_DEBUG((LM_INFO,
                       ACE_TEXT("(%P|%t) Publishing FileChunks for MODIFY: %C (%Q bytes, %u chunks)\n"),
                       filename.c_str(),
                       metadata.size,
                       total_chunks));

            // Read entire file
            std::vector<uint8_t> file_data;
            if (!DirShare::read_file(full_path, file_data)) {
              ACE_ERROR((LM_ERROR,
                         ACE_TEXT("ERROR: %N:%l: Failed to read file: %C\n"),
                         full_path.c_str()));
              continue;
            }

            // Send chunks
            for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
              DirShare::FileChunk chunk;
              chunk.filename = metadata.filename;
              chunk.chunk_id = chunk_id;
              chunk.total_chunks = total_chunks;
              chunk.file_size = metadata.size;
              chunk.file_checksum = metadata.checksum;
              chunk.timestamp_sec = metadata.timestamp_sec;
              chunk.timestamp_nsec = metadata.timestamp_nsec;

              // Calculate chunk data
              uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
              uint32_t this_chunk_size = static_cast<uint32_t>(
                (offset + CHUNK_SIZE > metadata.size) ?
                (metadata.size - offset) : CHUNK_SIZE);

              chunk.data.length(this_chunk_size);
              std::memcpy(chunk.data.get_buffer(), &file_data[offset], this_chunk_size);

              // Calculate chunk checksum
              chunk.chunk_checksum = DirShare::compute_checksum(
                &file_data[offset], this_chunk_size);

              ret = typed_chunk_writer->write(chunk, DDS::HANDLE_NIL);
              if (ret != DDS::RETCODE_OK) {
                ACE_ERROR((LM_ERROR,
                           ACE_TEXT("ERROR: %N:%l: write FileChunk failed: %d\n"),
                           ret));
                break;
              }

              // Small delay to avoid overwhelming UDP send buffer
              ACE_Time_Value delay(0, 10000); // 10ms
              ACE_OS::sleep(delay);
            }

            ACE_DEBUG((LM_INFO,
                       ACE_TEXT("(%P|%t) Completed publishing chunks for MODIFY: %C\n"),
                       filename.c_str()));
          }
        }

        // Handle deleted files (Phase 6 - placeholder)
        for (size_t i = 0; i < deleted_files.size(); ++i) {
          ACE_DEBUG((LM_INFO,
                     ACE_TEXT("(%P|%t) File DELETE detected: %C (Phase 6 - not yet implemented)\n"),
                     deleted_files[i].c_str()));
        }
      }
    }

    // Cleanup (will be reached via signal handler or when loop exits)
    ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Shutting down DirShare...\n")));

    ws->detach_condition(condition);

    participant->delete_contained_entities();
    dpf->delete_participant(participant);

    TheServiceParticipant->shutdown();

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    return_code = 1;
  }

  return return_code;
}
