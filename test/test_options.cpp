#include <gtest/gtest.h>

#include <cstddef>

#include "lldb/options.h"

namespace lldb {
namespace {

// ============================================================
// Options defaults
// ============================================================

TEST(OptionsTest, CreateIfMissingDefault) {
  Options opts;
  EXPECT_FALSE(opts.create_if_missing);
}

TEST(OptionsTest, ErrorIfExistsDefault) {
  Options opts;
  EXPECT_FALSE(opts.error_if_exists);
}

TEST(OptionsTest, ParanoidChecksDefault) {
  Options opts;
  EXPECT_FALSE(opts.paranoid_checks);
}

TEST(OptionsTest, EnvIsSet) {
  Options opts;
  EXPECT_NE(opts.env, nullptr);
}

TEST(OptionsTest, ComparatorIsSet) {
  Options opts;
  EXPECT_NE(opts.comparator, nullptr);
}

TEST(OptionsTest, InfoLogDefault) {
  Options opts;
  EXPECT_EQ(opts.info_log, nullptr);
}

TEST(OptionsTest, WriteBufferSizeDefault) {
  Options opts;
  EXPECT_EQ(opts.write_buffer_size, 4u * 1024 * 1024);
}

TEST(OptionsTest, MaxOpenFilesDefault) {
  Options opts;
  EXPECT_EQ(opts.max_open_files, 1000);
}

TEST(OptionsTest, BlockCacheDefault) {
  Options opts;
  EXPECT_EQ(opts.block_cache, nullptr);
}

TEST(OptionsTest, BlockSizeDefault) {
  Options opts;
  EXPECT_EQ(opts.block_size, 4u * 1024);
}

TEST(OptionsTest, BlockRestartIntervalDefault) {
  Options opts;
  EXPECT_EQ(opts.block_restart_interval, 16);
}

TEST(OptionsTest, MaxFileSizeDefault) {
  Options opts;
  EXPECT_EQ(opts.max_file_size, 2u * 1024 * 1024);
}

TEST(OptionsTest, CompressionDefault) {
  Options opts;
  EXPECT_EQ(opts.compression, kSnappyCompression);
}

TEST(OptionsTest, ZstdCompressionLevelDefault) {
  Options opts;
  EXPECT_EQ(opts.zstd_compression_level, 1);
}

TEST(OptionsTest, ReuseLogsDefault) {
  Options opts;
  EXPECT_FALSE(opts.reuse_logs);
}

TEST(OptionsTest, FilterPolicyDefault) {
  Options opts;
  EXPECT_EQ(opts.filter_policy, nullptr);
}

// ============================================================
// ReadOptions defaults
// ============================================================

TEST(ReadOptionsTest, VerifyChecksumsDefault) {
  ReadOptions opts;
  EXPECT_FALSE(opts.verify_checksums);
}

TEST(ReadOptionsTest, FillCacheDefault) {
  ReadOptions opts;
  EXPECT_TRUE(opts.fill_cache);
}

TEST(ReadOptionsTest, SnapshotDefault) {
  ReadOptions opts;
  EXPECT_EQ(opts.snapshot, nullptr);
}

// ============================================================
// WriteOptions defaults
// ============================================================

TEST(WriteOptionsTest, SyncDefault) {
  WriteOptions opts;
  EXPECT_FALSE(opts.sync);
}

// ============================================================
// CompressionType enum values
// ============================================================

TEST(CompressionTypeTest, EnumValues) {
  EXPECT_EQ(kNoCompression, 0x0);
  EXPECT_EQ(kSnappyCompression, 0x1);
  EXPECT_EQ(kZstdCompression, 0x2);
}

}  // namespace
}  // namespace lldb
