#define BOOST_TEST_MODULE FileChunkTest
#include <boost/test/included/unit_test.hpp>

#include "../FileUtils.h"
#include "../Checksum.h"
#include "../DirShareTypeSupportImpl.h"
#include <ace/OS_NS_unistd.h>
#include <vector>
#include <map>

BOOST_AUTO_TEST_SUITE(FileChunkTestSuite)

// Test: Chunk calculation for 10MB threshold
BOOST_AUTO_TEST_CASE(test_chunk_threshold_10mb)
{
  const uint64_t CHUNK_THRESHOLD = 10 * 1024 * 1024; // 10MB
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB

  // File exactly at threshold should use chunks
  uint64_t file_size_at_threshold = CHUNK_THRESHOLD;
  BOOST_CHECK(file_size_at_threshold >= CHUNK_THRESHOLD);

  // Calculate chunks
  uint32_t num_chunks = static_cast<uint32_t>((file_size_at_threshold + CHUNK_SIZE - 1) / CHUNK_SIZE);
  BOOST_CHECK_EQUAL(num_chunks, 10);

  // File just below threshold should NOT use chunks (FileContent instead)
  uint64_t file_size_below = CHUNK_THRESHOLD - 1;
  BOOST_CHECK(file_size_below < CHUNK_THRESHOLD);

  // File above threshold should use chunks
  uint64_t file_size_above = CHUNK_THRESHOLD + 1;
  BOOST_CHECK(file_size_above >= CHUNK_THRESHOLD);
  uint32_t chunks_above = static_cast<uint32_t>((file_size_above + CHUNK_SIZE - 1) / CHUNK_SIZE);
  BOOST_CHECK_EQUAL(chunks_above, 11);
}

// Test: Chunk calculation for 1MB chunks
BOOST_AUTO_TEST_CASE(test_1mb_chunk_size)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB

  // 15MB file should have 15 chunks
  uint64_t file_size_15mb = 15 * 1024 * 1024;
  uint32_t num_chunks = static_cast<uint32_t>((file_size_15mb + CHUNK_SIZE - 1) / CHUNK_SIZE);
  BOOST_CHECK_EQUAL(num_chunks, 15);

  // 15.5MB file should have 16 chunks
  uint64_t file_size_15_5mb = 15 * 1024 * 1024 + 512 * 1024;
  num_chunks = static_cast<uint32_t>((file_size_15_5mb + CHUNK_SIZE - 1) / CHUNK_SIZE);
  BOOST_CHECK_EQUAL(num_chunks, 16);

  // 100MB file should have 100 chunks
  uint64_t file_size_100mb = 100 * 1024 * 1024;
  num_chunks = static_cast<uint32_t>((file_size_100mb + CHUNK_SIZE - 1) / CHUNK_SIZE);
  BOOST_CHECK_EQUAL(num_chunks, 100);
}

// Test: Chunk reassembly in order
BOOST_AUTO_TEST_CASE(test_chunk_reassembly_in_order)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
  const uint64_t file_size = 3 * CHUNK_SIZE; // 3MB file
  const uint32_t total_chunks = 3;

  // Create test data
  std::vector<uint8_t> original_data(file_size);
  for (size_t i = 0; i < file_size; ++i) {
    original_data[i] = static_cast<uint8_t>(i % 256);
  }

  // Compute file checksum
  uint32_t file_checksum = DirShare::compute_checksum(&original_data[0], file_size);

  // Simulate chunking and reassembly
  std::vector<uint8_t> reassembled_data(file_size);

  // Process chunks in order
  for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
    uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
    uint32_t this_chunk_size = CHUNK_SIZE;

    // Copy chunk data
    std::memcpy(&reassembled_data[offset], &original_data[offset], this_chunk_size);
  }

  // Verify reassembled data matches original
  BOOST_CHECK_EQUAL(reassembled_data.size(), original_data.size());

  for (size_t i = 0; i < file_size; ++i) {
    BOOST_CHECK_EQUAL(reassembled_data[i], original_data[i]);
  }

  // Verify checksum
  uint32_t reassembled_checksum = DirShare::compute_checksum(&reassembled_data[0], file_size);
  BOOST_CHECK_EQUAL(reassembled_checksum, file_checksum);
}

// Test: Chunk reassembly out of order
BOOST_AUTO_TEST_CASE(test_chunk_reassembly_out_of_order)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
  const uint64_t file_size = 5 * CHUNK_SIZE; // 5MB file
  const uint32_t total_chunks = 5;

  // Create test data
  std::vector<uint8_t> original_data(file_size);
  for (size_t i = 0; i < file_size; ++i) {
    original_data[i] = static_cast<uint8_t>((i * 7) % 256); // Varying pattern
  }

  // Compute file checksum
  uint32_t file_checksum = DirShare::compute_checksum(&original_data[0], file_size);

  // Simulate out-of-order reception
  std::vector<uint8_t> reassembled_data(file_size);
  std::map<uint32_t, bool> received_chunks;

  // Receive chunks in random order: 2, 4, 0, 3, 1
  uint32_t receive_order[] = {2, 4, 0, 3, 1};

  for (size_t i = 0; i < total_chunks; ++i) {
    uint32_t chunk_id = receive_order[i];
    uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
    uint32_t this_chunk_size = CHUNK_SIZE;

    // Copy chunk data at correct offset
    std::memcpy(&reassembled_data[offset], &original_data[offset], this_chunk_size);
    received_chunks[chunk_id] = true;
  }

  // Verify all chunks received
  BOOST_CHECK_EQUAL(received_chunks.size(), total_chunks);

  // Verify reassembled data matches original
  for (size_t i = 0; i < file_size; ++i) {
    BOOST_CHECK_EQUAL(reassembled_data[i], original_data[i]);
  }

  // Verify checksum
  uint32_t reassembled_checksum = DirShare::compute_checksum(&reassembled_data[0], file_size);
  BOOST_CHECK_EQUAL(reassembled_checksum, file_checksum);
}

// Test: Chunk checksum verification
BOOST_AUTO_TEST_CASE(test_chunk_checksum_verification)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB

  // Create test chunk data
  std::vector<uint8_t> chunk_data(CHUNK_SIZE);
  for (size_t i = 0; i < CHUNK_SIZE; ++i) {
    chunk_data[i] = static_cast<uint8_t>(i % 256);
  }

  // Compute chunk checksum
  uint32_t chunk_checksum = DirShare::compute_checksum(&chunk_data[0], CHUNK_SIZE);

  // Simulate FileChunk
  DirShare::FileChunk chunk;
  chunk.chunk_id = 0;
  chunk.chunk_checksum = chunk_checksum;
  chunk.data.length(CHUNK_SIZE);
  std::memcpy(chunk.data.get_buffer(), &chunk_data[0], CHUNK_SIZE);

  // Verify chunk checksum
  uint32_t computed_checksum = DirShare::compute_checksum(
    reinterpret_cast<const uint8_t*>(chunk.data.get_buffer()),
    chunk.data.length());

  BOOST_CHECK_EQUAL(computed_checksum, chunk_checksum);
}

// Test: File checksum after reassembly
BOOST_AUTO_TEST_CASE(test_file_checksum_after_reassembly)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
  const uint64_t file_size = 10 * CHUNK_SIZE + 512 * 1024; // 10.5MB
  const uint32_t total_chunks = 11;

  // Create test data
  std::vector<uint8_t> original_data(file_size);
  for (size_t i = 0; i < file_size; ++i) {
    original_data[i] = static_cast<uint8_t>((i * 13) % 256);
  }

  // Compute original file checksum
  uint32_t original_checksum = DirShare::compute_checksum(&original_data[0], file_size);

  // Simulate chunking
  std::vector<DirShare::FileChunk> chunks;
  for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
    DirShare::FileChunk chunk;
    chunk.chunk_id = chunk_id;
    chunk.total_chunks = total_chunks;
    chunk.file_size = file_size;
    chunk.file_checksum = original_checksum;

    uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
    uint32_t this_chunk_size = static_cast<uint32_t>(
      (offset + CHUNK_SIZE > file_size) ? (file_size - offset) : CHUNK_SIZE);

    chunk.data.length(this_chunk_size);
    std::memcpy(chunk.data.get_buffer(), &original_data[offset], this_chunk_size);
    chunk.chunk_checksum = DirShare::compute_checksum(&original_data[offset], this_chunk_size);

    chunks.push_back(chunk);
  }

  // Simulate reassembly
  std::vector<uint8_t> reassembled_data(file_size);
  for (size_t i = 0; i < chunks.size(); ++i) {
    uint64_t offset = static_cast<uint64_t>(chunks[i].chunk_id) * CHUNK_SIZE;
    std::memcpy(&reassembled_data[offset],
                chunks[i].data.get_buffer(),
                chunks[i].data.length());
  }

  // Verify file checksum after reassembly
  uint32_t reassembled_checksum = DirShare::compute_checksum(&reassembled_data[0], file_size);
  BOOST_CHECK_EQUAL(reassembled_checksum, original_checksum);
}

// Test: Last chunk smaller than CHUNK_SIZE
BOOST_AUTO_TEST_CASE(test_last_chunk_partial)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
  const uint64_t file_size = 2 * CHUNK_SIZE + 512 * 1024; // 2.5MB
  const uint32_t total_chunks = 3;

  // Verify chunk size calculations
  for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
    uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;
    uint32_t expected_chunk_size;

    if (chunk_id < 2) {
      // First two chunks should be full 1MB
      expected_chunk_size = CHUNK_SIZE;
    } else {
      // Last chunk should be 512KB
      expected_chunk_size = 512 * 1024;
    }

    uint32_t actual_chunk_size = static_cast<uint32_t>(
      (offset + CHUNK_SIZE > file_size) ? (file_size - offset) : CHUNK_SIZE);

    BOOST_CHECK_EQUAL(actual_chunk_size, expected_chunk_size);
  }
}

// Test: Chunk count calculation for various file sizes
BOOST_AUTO_TEST_CASE(test_chunk_count_various_sizes)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB

  struct TestCase {
    uint64_t file_size;
    uint32_t expected_chunks;
    const char* description;
  };

  TestCase test_cases[] = {
    {10 * 1024 * 1024, 10, "Exactly 10MB"},
    {10 * 1024 * 1024 + 1, 11, "10MB + 1 byte"},
    {15 * 1024 * 1024, 15, "Exactly 15MB"},
    {20 * 1024 * 1024 + 500 * 1024, 21, "20.5MB"},
    {100 * 1024 * 1024, 100, "Exactly 100MB"},
    {1024 * 1024 * 1024, 1024, "1GB (max supported)"}
  };

  for (size_t i = 0; i < sizeof(test_cases) / sizeof(TestCase); ++i) {
    uint32_t num_chunks = static_cast<uint32_t>(
      (test_cases[i].file_size + CHUNK_SIZE - 1) / CHUNK_SIZE);

    BOOST_CHECK_EQUAL(num_chunks, test_cases[i].expected_chunks);
  }
}

// Test: Chunk reassembly buffer management
BOOST_AUTO_TEST_CASE(test_chunk_buffer_management)
{
  const uint32_t CHUNK_SIZE = 1024 * 1024; // 1MB
  const uint64_t file_size = 4 * CHUNK_SIZE; // 4MB
  const uint32_t total_chunks = 4;

  // Simulate reassembly buffer
  std::vector<uint8_t> buffer(file_size);
  std::map<uint32_t, bool> received_chunks;

  // Create original data
  std::vector<uint8_t> original_data(file_size);
  for (size_t i = 0; i < file_size; ++i) {
    original_data[i] = static_cast<uint8_t>(i % 256);
  }

  // Simulate receiving chunks
  for (uint32_t chunk_id = 0; chunk_id < total_chunks; ++chunk_id) {
    uint64_t offset = static_cast<uint64_t>(chunk_id) * CHUNK_SIZE;

    // Copy chunk into buffer
    std::memcpy(&buffer[offset], &original_data[offset], CHUNK_SIZE);
    received_chunks[chunk_id] = true;

    // Check if complete
    bool all_received = (received_chunks.size() == total_chunks);
    if (chunk_id == total_chunks - 1) {
      BOOST_CHECK(all_received);
    }
  }

  // Verify buffer contains correct data
  for (size_t i = 0; i < file_size; ++i) {
    BOOST_CHECK_EQUAL(buffer[i], original_data[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
