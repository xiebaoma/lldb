#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "helper/memenv/memenv.h"
#include "lldb/env.h"
#include "lldb/slice.h"
#include "lldb/status.h"

namespace lldb {
namespace {

class MemEnvTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base_env_ = Env::Default();
    ASSERT_NE(base_env_, nullptr);
    memenv_.reset(NewMemEnv(base_env_));
    ASSERT_NE(memenv_.get(), nullptr);
  }

  void TearDown() override { memenv_.reset(); }

  // Write data to a new file.
  Status WriteFile(const std::string& path, const Slice& data) {
    WritableFile* file = nullptr;
    Status s = memenv_->NewWritableFile(path, &file);
    if (!s.ok()) return s;
    s = file->Append(data);
    if (s.ok()) s = file->Close();
    delete file;
    return s;
  }

  // Read entire file into a string via sequential read.
  Status ReadSequential(const std::string& path, std::string* out) {
    SequentialFile* file = nullptr;
    Status s = memenv_->NewSequentialFile(path, &file);
    if (!s.ok()) return s;
    out->clear();
    char buf[4096];
    Slice fragment;
    do {
      s = file->Read(sizeof(buf), &fragment, buf);
      if (s.ok()) {
        if (fragment.empty()) break;
        out->append(fragment.data(), fragment.size());
      }
    } while (s.ok());
    delete file;
    return s;
  }

  struct EnvDeleter {
    void operator()(Env* p) const { delete p; }
  };
  using EnvPtr = std::unique_ptr<Env, EnvDeleter>;

  Env* base_env_;
  EnvPtr memenv_;
};

// ============================================================
// NewMemEnv / basic env properties
// ============================================================

TEST_F(MemEnvTest, GetTestDirectory) {
  std::string dir;
  ASSERT_TRUE(memenv_->GetTestDirectory(&dir).ok());
  EXPECT_EQ(dir, "/test");
}

TEST_F(MemEnvTest, CreateDirAndRemoveDirAreNoOps) {
  EXPECT_TRUE(memenv_->CreateDir("/foo").ok());
  EXPECT_TRUE(memenv_->RemoveDir("/foo").ok());
}

// ============================================================
// File write / sequential read
// ============================================================

TEST_F(MemEnvTest, WriteAndSequentialRead) {
  ASSERT_TRUE(WriteFile("test.txt", Slice("hello world")).ok());

  std::string content;
  ASSERT_TRUE(ReadSequential("test.txt", &content).ok());
  EXPECT_EQ(content, "hello world");
}

TEST_F(MemEnvTest, WriteAndReadEmptyFile) {
  ASSERT_TRUE(WriteFile("empty.txt", Slice()).ok());

  std::string content;
  ASSERT_TRUE(ReadSequential("empty.txt", &content).ok());
  EXPECT_TRUE(content.empty());
}

TEST_F(MemEnvTest, WriteLargeDataSpanningBlocks) {
  // kBlockSize = 8KB. Write 24KB to cross 3 blocks.
  std::string data(24 * 1024, 'X');
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = static_cast<char>('A' + (i % 26));
  }
  ASSERT_TRUE(WriteFile("large.bin", Slice(data)).ok());

  uint64_t size = 0;
  ASSERT_TRUE(memenv_->GetFileSize("large.bin", &size).ok());
  EXPECT_EQ(size, data.size());

  std::string content;
  ASSERT_TRUE(ReadSequential("large.bin", &content).ok());
  EXPECT_EQ(content, data);
}

TEST_F(MemEnvTest, WriteExactlyOneBlock) {
  std::string data(8 * 1024, 'B');
  ASSERT_TRUE(WriteFile("one_block.bin", Slice(data)).ok());

  std::string content;
  ASSERT_TRUE(ReadSequential("one_block.bin", &content).ok());
  EXPECT_EQ(content, data);
}

// ============================================================
// SequentialFile::Skip
// ============================================================

TEST_F(MemEnvTest, SequentialFileSkip) {
  ASSERT_TRUE(WriteFile("skip.txt", Slice("abcdefghij")).ok());

  SequentialFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewSequentialFile("skip.txt", &file).ok());

  char buf[8];
  Slice result;

  ASSERT_TRUE(file->Read(3, &result, buf).ok());
  EXPECT_EQ(result, Slice("abc"));

  ASSERT_TRUE(file->Skip(4).ok());

  ASSERT_TRUE(file->Read(3, &result, buf).ok());
  EXPECT_EQ(result, Slice("hij"));

  delete file;
}

TEST_F(MemEnvTest, SequentialFileSkipPastEOF) {
  ASSERT_TRUE(WriteFile("short.txt", Slice("abc")).ok());

  SequentialFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewSequentialFile("short.txt", &file).ok());

  // Skip past EOF — should stop at EOF and return OK.
  Status s = file->Skip(100);
  EXPECT_TRUE(s.ok());

  // Further reads should return empty.
  char buf[8];
  Slice result;
  ASSERT_TRUE(file->Read(4, &result, buf).ok());
  EXPECT_TRUE(result.empty());

  delete file;
}

// ============================================================
// Random-access read
// ============================================================

TEST_F(MemEnvTest, RandomAccessRead) {
  std::string data = "0123456789";
  ASSERT_TRUE(WriteFile("random.bin", Slice(data)).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewRandomAccessFile("random.bin", &file).ok());

  char buf[4];
  Slice result;
  ASSERT_TRUE(file->Read(4, 3, &result, buf).ok());
  EXPECT_EQ(result, Slice("456"));

  delete file;
}

TEST_F(MemEnvTest, RandomAccessReadAtOffsetZero) {
  ASSERT_TRUE(WriteFile("head.txt", Slice("hello")).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewRandomAccessFile("head.txt", &file).ok());

  char buf[16];
  Slice result;
  ASSERT_TRUE(file->Read(0, 5, &result, buf).ok());
  EXPECT_EQ(result, Slice("hello"));

  delete file;
}

TEST_F(MemEnvTest, RandomAccessReadNearEOF) {
  ASSERT_TRUE(WriteFile("tail.txt", Slice("abcdef")).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewRandomAccessFile("tail.txt", &file).ok());

  char buf[8];
  Slice result;
  // Read from offset 4 with request size 10 — should get only 2 bytes.
  ASSERT_TRUE(file->Read(4, 10, &result, buf).ok());
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result, Slice("ef"));

  delete file;
}

TEST_F(MemEnvTest, RandomAccessReadCompletelyBeyondSize) {
  ASSERT_TRUE(WriteFile("tiny.txt", Slice("ab")).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewRandomAccessFile("tiny.txt", &file).ok());

  char buf[8];
  Slice result;
  // Offset 3 > size 2 — should return IOError.
  Status s = file->Read(3, 5, &result, buf);
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());

  delete file;
}

// ============================================================
// Appendable file
// ============================================================

TEST_F(MemEnvTest, AppendableFile) {
  {
    WritableFile* file = nullptr;
    ASSERT_TRUE(memenv_->NewAppendableFile("append.txt", &file).ok());
    ASSERT_TRUE(file->Append(Slice("first ")).ok());
    ASSERT_TRUE(file->Close().ok());
    delete file;
  }
  {
    WritableFile* file = nullptr;
    ASSERT_TRUE(memenv_->NewAppendableFile("append.txt", &file).ok());
    ASSERT_TRUE(file->Append(Slice("second")).ok());
    ASSERT_TRUE(file->Close().ok());
    delete file;
  }

  std::string content;
  ASSERT_TRUE(ReadSequential("append.txt", &content).ok());
  EXPECT_EQ(content, "first second");
}

TEST_F(MemEnvTest, NewWritableFileTruncatesExisting) {
  ASSERT_TRUE(WriteFile("overwrite.txt", Slice("old content here")).ok());

  // NewWritableFile should truncate existing content.
  ASSERT_TRUE(WriteFile("overwrite.txt", Slice("new")).ok());

  std::string content;
  ASSERT_TRUE(ReadSequential("overwrite.txt", &content).ok());
  EXPECT_EQ(content, "new");
}

TEST_F(MemEnvTest, NewAppendableFilePreservesExisting) {
  ASSERT_TRUE(WriteFile("keep.txt", Slice("original ")).ok());

  WritableFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewAppendableFile("keep.txt", &file).ok());
  ASSERT_TRUE(file->Append(Slice("extra")).ok());
  ASSERT_TRUE(file->Close().ok());
  delete file;

  std::string content;
  ASSERT_TRUE(ReadSequential("keep.txt", &content).ok());
  EXPECT_EQ(content, "original extra");
}

// ============================================================
// FileExists
// ============================================================

TEST_F(MemEnvTest, FileExistsTrue) {
  ASSERT_TRUE(WriteFile("exists.txt", Slice("data")).ok());
  EXPECT_TRUE(memenv_->FileExists("exists.txt"));
}

TEST_F(MemEnvTest, FileExistsFalse) {
  EXPECT_FALSE(memenv_->FileExists("nope.txt"));
}

// ============================================================
// GetFileSize
// ============================================================

TEST_F(MemEnvTest, GetFileSize) {
  ASSERT_TRUE(WriteFile("size.txt", Slice("exactly 24 bytes of data")).ok());

  uint64_t size = 0;
  ASSERT_TRUE(memenv_->GetFileSize("size.txt", &size).ok());
  EXPECT_EQ(size, 24u);
}

TEST_F(MemEnvTest, GetFileSizeEmptyFile) {
  ASSERT_TRUE(WriteFile("empty_size.txt", Slice()).ok());

  uint64_t size = 1;
  ASSERT_TRUE(memenv_->GetFileSize("empty_size.txt", &size).ok());
  EXPECT_EQ(size, 0u);
}

TEST_F(MemEnvTest, GetFileSizeNonExistent) {
  uint64_t size = 42;
  Status s = memenv_->GetFileSize("nope.txt", &size);
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
}

// ============================================================
// RenameFile
// ============================================================

TEST_F(MemEnvTest, RenameFile) {
  ASSERT_TRUE(WriteFile("src.txt", Slice("content")).ok());
  ASSERT_TRUE(memenv_->RenameFile("src.txt", "dst.txt").ok());

  EXPECT_FALSE(memenv_->FileExists("src.txt"));
  EXPECT_TRUE(memenv_->FileExists("dst.txt"));

  std::string content;
  ASSERT_TRUE(ReadSequential("dst.txt", &content).ok());
  EXPECT_EQ(content, "content");
}

TEST_F(MemEnvTest, RenameFileOverwritesTarget) {
  ASSERT_TRUE(WriteFile("src.txt", Slice("new content")).ok());
  ASSERT_TRUE(WriteFile("dst.txt", Slice("old content")).ok());

  ASSERT_TRUE(memenv_->RenameFile("src.txt", "dst.txt").ok());

  EXPECT_FALSE(memenv_->FileExists("src.txt"));
  EXPECT_TRUE(memenv_->FileExists("dst.txt"));

  std::string content;
  ASSERT_TRUE(ReadSequential("dst.txt", &content).ok());
  EXPECT_EQ(content, "new content");
}

TEST_F(MemEnvTest, RenameFileNonExistent) {
  Status s = memenv_->RenameFile("nonexistent.txt", "target.txt");
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
}

// ============================================================
// RemoveFile
// ============================================================

TEST_F(MemEnvTest, RemoveFile) {
  ASSERT_TRUE(WriteFile("rm.txt", Slice("data")).ok());
  ASSERT_TRUE(memenv_->RemoveFile("rm.txt").ok());
  EXPECT_FALSE(memenv_->FileExists("rm.txt"));
}

TEST_F(MemEnvTest, RemoveNonExistentFile) {
  Status s = memenv_->RemoveFile("nope.txt");
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
}

// ============================================================
// GetChildren
// ============================================================

TEST_F(MemEnvTest, GetChildren) {
  ASSERT_TRUE(WriteFile("/mydir/file_a.txt", Slice("a")).ok());
  ASSERT_TRUE(WriteFile("/mydir/file_b.txt", Slice("b")).ok());
  ASSERT_TRUE(WriteFile("/mydir/sub/file_c.txt", Slice("c")).ok());

  std::vector<std::string> children;
  ASSERT_TRUE(memenv_->GetChildren("/mydir", &children).ok());

  EXPECT_EQ(children.size(), 3u);

  bool found_a = false, found_b = false, found_sub = false;
  for (const auto& c : children) {
    if (c == "file_a.txt") found_a = true;
    if (c == "file_b.txt") found_b = true;
    if (c == "sub/file_c.txt") found_sub = true;
  }
  EXPECT_TRUE(found_a);
  EXPECT_TRUE(found_b);
  EXPECT_TRUE(found_sub);
}

TEST_F(MemEnvTest, GetChildrenEmpty) {
  std::vector<std::string> children;
  children.push_back("stale");
  ASSERT_TRUE(memenv_->GetChildren("/nonexistent", &children).ok());
  EXPECT_TRUE(children.empty());
}

TEST_F(MemEnvTest, GetChildrenExcludesExactDirMatch) {
  // A file named exactly "/dir" should not be a child of "/dir".
  ASSERT_TRUE(WriteFile("/dir", Slice("not a dir")).ok());

  std::vector<std::string> children;
  ASSERT_TRUE(memenv_->GetChildren("/dir", &children).ok());
  EXPECT_TRUE(children.empty());
}

// ============================================================
// LockFile / UnlockFile
// ============================================================

TEST_F(MemEnvTest, LockAndUnlockFile) {
  FileLock* lock = nullptr;
  ASSERT_TRUE(memenv_->LockFile("lock.txt", &lock).ok());
  ASSERT_NE(lock, nullptr);

  ASSERT_TRUE(memenv_->UnlockFile(lock).ok());
}

TEST_F(MemEnvTest, LockFileAlwaysSucceeds) {
  // InMemoryEnv does not implement real locking — two locks on the same
  // file both succeed.
  FileLock* lock1 = nullptr;
  ASSERT_TRUE(memenv_->LockFile("nolock.txt", &lock1).ok());

  FileLock* lock2 = nullptr;
  ASSERT_TRUE(memenv_->LockFile("nolock.txt", &lock2).ok());

  ASSERT_TRUE(memenv_->UnlockFile(lock1).ok());
  ASSERT_TRUE(memenv_->UnlockFile(lock2).ok());
}

// ============================================================
// NewLogger
// ============================================================

TEST_F(MemEnvTest, NewLoggerReturnsNonNull) {
  Logger* logger = nullptr;
  ASSERT_TRUE(memenv_->NewLogger("log.txt", &logger).ok());
  ASSERT_NE(logger, nullptr);
  delete logger;
}

// ============================================================
// Error cases — file not found
// ============================================================

TEST_F(MemEnvTest, NewSequentialFileNotFound) {
  SequentialFile* file = reinterpret_cast<SequentialFile*>(0x1);
  Status s = memenv_->NewSequentialFile("missing.txt", &file);
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
  EXPECT_EQ(file, nullptr);
}

TEST_F(MemEnvTest, NewRandomAccessFileNotFound) {
  RandomAccessFile* file = reinterpret_cast<RandomAccessFile*>(0x1);
  Status s = memenv_->NewRandomAccessFile("missing.txt", &file);
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());
  EXPECT_EQ(file, nullptr);
}

// ============================================================
// SequentialFile::Read edge cases
// ============================================================

TEST_F(MemEnvTest, SequentialReadLargerThanFileContents) {
  ASSERT_TRUE(WriteFile("small.txt", Slice("abc")).ok());

  SequentialFile* file = nullptr;
  ASSERT_TRUE(memenv_->NewSequentialFile("small.txt", &file).ok());

  char buf[16];
  Slice result;
  ASSERT_TRUE(file->Read(sizeof(buf), &result, buf).ok());
  EXPECT_EQ(result.size(), 3u);
  EXPECT_EQ(result, Slice("abc"));

  // Next read should return empty (EOF).
  ASSERT_TRUE(file->Read(sizeof(buf), &result, buf).ok());
  EXPECT_TRUE(result.empty());

  delete file;
}

}  // namespace
}  // namespace lldb
