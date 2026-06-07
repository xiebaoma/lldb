#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lldb/env.h"
#include "lldb/slice.h"
#include "lldb/status.h"

namespace lldb {
namespace {

class EnvPosixTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = Env::Default();
    ASSERT_NE(env_, nullptr);
    ASSERT_TRUE(env_->GetTestDirectory(&test_dir_).ok());
    CleanupDir(test_dir_);
    ASSERT_TRUE(env_->CreateDir(test_dir_).ok());
  }

  void TearDown() override { CleanupDir(test_dir_); }

  // Recursively remove all files and subdirs, then the directory itself.
  void CleanupDir(const std::string& dir) {
    std::vector<std::string> children;
    if (env_->GetChildren(dir, &children).ok()) {
      for (const auto& child : children) {
        if (child == "." || child == "..") continue;
        std::string path = dir + "/" + child;
        env_->RemoveFile(path);  // best effort
      }
    }
    env_->RemoveDir(dir);
  }

  std::string TestPath(const std::string& name) {
    return test_dir_ + "/" + name;
  }

  // Write data to a new file, returning the status.
  Status WriteFile(const std::string& path, const Slice& data) {
    WritableFile* file = nullptr;
    Status s = env_->NewWritableFile(path, &file);
    if (!s.ok()) return s;
    s = file->Append(data);
    if (s.ok()) s = file->Close();
    delete file;
    return s;
  }

  // Read entire file content into a string.
  Status ReadFile(const std::string& path, std::string* out) {
    SequentialFile* file = nullptr;
    Status s = env_->NewSequentialFile(path, &file);
    if (!s.ok()) return s;
    out->clear();
    char buf[4096];
    Slice fragment;
    do {
      s = file->Read(sizeof(buf), &fragment, buf);
      if (s.ok()) {
        if (fragment.empty()) break;  // EOF
        out->append(fragment.data(), fragment.size());
      }
    } while (s.ok());
    delete file;
    return s;
  }

  Env* env_;
  std::string test_dir_;
};

// ============================================================
// File write / read
// ============================================================

TEST_F(EnvPosixTest, WriteAndReadFile) {
  std::string path = TestPath("test.txt");
  ASSERT_TRUE(WriteFile(path, Slice("hello world")).ok());

  std::string content;
  ASSERT_TRUE(ReadFile(path, &content).ok());
  EXPECT_EQ(content, "hello world");
}

TEST_F(EnvPosixTest, WriteAndReadEmptyFile) {
  std::string path = TestPath("empty.txt");
  ASSERT_TRUE(WriteFile(path, Slice()).ok());

  std::string content;
  ASSERT_TRUE(ReadFile(path, &content).ok());
  EXPECT_TRUE(content.empty());
}

TEST_F(EnvPosixTest, WriteLargeData) {
  std::string path = TestPath("large.bin");
  std::string data(100000, 'X');
  ASSERT_TRUE(WriteFile(path, Slice(data)).ok());

  std::string content;
  ASSERT_TRUE(ReadFile(path, &content).ok());
  EXPECT_EQ(content, data);
}

// ============================================================
// FileExists
// ============================================================

TEST_F(EnvPosixTest, FileExistsTrue) {
  std::string path = TestPath("exists.txt");
  ASSERT_TRUE(WriteFile(path, Slice("data")).ok());
  EXPECT_TRUE(env_->FileExists(path));
}

TEST_F(EnvPosixTest, FileExistsFalse) {
  EXPECT_FALSE(env_->FileExists(TestPath("nope.txt")));
}

// ============================================================
// GetFileSize
// ============================================================

TEST_F(EnvPosixTest, GetFileSize) {
  std::string path = TestPath("size.txt");
  std::string content = "exactly 24 bytes of data";
  ASSERT_TRUE(WriteFile(path, Slice(content)).ok());

  uint64_t size = 0;
  ASSERT_TRUE(env_->GetFileSize(path, &size).ok());
  EXPECT_EQ(size, content.size());
}

TEST_F(EnvPosixTest, GetFileSizeNonExistent) {
  uint64_t size = 42;
  Status s = env_->GetFileSize(TestPath("nope.txt"), &size);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(size, 0u);
}

// ============================================================
// RenameFile
// ============================================================

TEST_F(EnvPosixTest, RenameFile) {
  std::string src = TestPath("src.txt");
  std::string dst = TestPath("dst.txt");
  ASSERT_TRUE(WriteFile(src, Slice("content")).ok());
  ASSERT_TRUE(env_->RenameFile(src, dst).ok());

  EXPECT_FALSE(env_->FileExists(src));
  EXPECT_TRUE(env_->FileExists(dst));

  std::string content;
  ASSERT_TRUE(ReadFile(dst, &content).ok());
  EXPECT_EQ(content, "content");
}

// ============================================================
// RemoveFile
// ============================================================

TEST_F(EnvPosixTest, RemoveFile) {
  std::string path = TestPath("rm.txt");
  ASSERT_TRUE(WriteFile(path, Slice("data")).ok());
  ASSERT_TRUE(env_->RemoveFile(path).ok());
  EXPECT_FALSE(env_->FileExists(path));
}

// ============================================================
// Directory operations
// ============================================================

TEST_F(EnvPosixTest, CreateAndRemoveDir) {
  std::string dir = TestPath("subdir");
  ASSERT_TRUE(env_->CreateDir(dir).ok());
  // Creating it again should fail.
  EXPECT_FALSE(env_->CreateDir(dir).ok());
  ASSERT_TRUE(env_->RemoveDir(dir).ok());
}

TEST_F(EnvPosixTest, GetChildren) {
  std::string a = TestPath("a.txt");
  std::string b = TestPath("b.txt");
  ASSERT_TRUE(WriteFile(a, Slice("a")).ok());
  ASSERT_TRUE(WriteFile(b, Slice("b")).ok());

  std::vector<std::string> children;
  ASSERT_TRUE(env_->GetChildren(test_dir_, &children).ok());
  // Should contain "a.txt", "b.txt", ".", and "..".
  EXPECT_GE(children.size(), 2u);

  bool found_a = false, found_b = false;
  for (const auto& c : children) {
    if (c == "a.txt") found_a = true;
    if (c == "b.txt") found_b = true;
  }
  EXPECT_TRUE(found_a);
  EXPECT_TRUE(found_b);
}

// ============================================================
// Random access read
// ============================================================

TEST_F(EnvPosixTest, RandomAccessRead) {
  std::string path = TestPath("random.bin");
  std::string data = "0123456789";
  ASSERT_TRUE(WriteFile(path, Slice(data)).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(env_->NewRandomAccessFile(path, &file).ok());

  char buf[4];
  Slice result;
  // Read bytes 4..7.
  ASSERT_TRUE(file->Read(4, 3, &result, buf).ok());
  EXPECT_EQ(result, Slice("456"));

  delete file;
}

TEST_F(EnvPosixTest, RandomAccessReadExactSize) {
  std::string path = TestPath("short.txt");
  ASSERT_TRUE(WriteFile(path, Slice("ab")).ok());

  RandomAccessFile* file = nullptr;
  ASSERT_TRUE(env_->NewRandomAccessFile(path, &file).ok());

  char buf[8];
  Slice result;
  // Read exactly 2 bytes (the file size). Reading past end is
  // implementation-defined (mmap returns error, pread returns partial).
  ASSERT_TRUE(file->Read(0, 2, &result, buf).ok());
  EXPECT_EQ(result, Slice("ab"));

  delete file;
}

// ============================================================
// Appendable file
// ============================================================

TEST_F(EnvPosixTest, AppendableFile) {
  std::string path = TestPath("append.txt");
  {
    WritableFile* file = nullptr;
    ASSERT_TRUE(env_->NewAppendableFile(path, &file).ok());
    ASSERT_TRUE(file->Append(Slice("first ")).ok());
    ASSERT_TRUE(file->Close().ok());
    delete file;
  }
  {
    WritableFile* file = nullptr;
    ASSERT_TRUE(env_->NewAppendableFile(path, &file).ok());
    ASSERT_TRUE(file->Append(Slice("second")).ok());
    ASSERT_TRUE(file->Close().ok());
    delete file;
  }

  std::string content;
  ASSERT_TRUE(ReadFile(path, &content).ok());
  EXPECT_EQ(content, "first second");
}

// ============================================================
// File lock / unlock
// ============================================================

TEST_F(EnvPosixTest, LockAndUnlockFile) {
  std::string path = TestPath("lock.txt");
  FileLock* lock = nullptr;
  ASSERT_TRUE(env_->LockFile(path, &lock).ok());
  ASSERT_NE(lock, nullptr);
  ASSERT_TRUE(env_->UnlockFile(lock).ok());
}

TEST_F(EnvPosixTest, LockFileAlreadyLocked) {
  std::string path = TestPath("lock2.txt");
  FileLock* lock1 = nullptr;
  ASSERT_TRUE(env_->LockFile(path, &lock1).ok());

  FileLock* lock2 = nullptr;
  Status s = env_->LockFile(path, &lock2);
  EXPECT_FALSE(s.ok());
  EXPECT_TRUE(s.IsIOError());

  ASSERT_TRUE(env_->UnlockFile(lock1).ok());
}

// ============================================================
// NowMicros
// ============================================================

TEST_F(EnvPosixTest, NowMicrosIncreasing) {
  uint64_t t1 = env_->NowMicros();
  uint64_t t2 = env_->NowMicros();
  EXPECT_GE(t2, t1);
}

// ============================================================
// SleepForMicroseconds
// ============================================================

TEST_F(EnvPosixTest, SleepForMicroseconds) {
  uint64_t before = env_->NowMicros();
  env_->SleepForMicroseconds(50000);  // 50ms
  uint64_t after = env_->NowMicros();
  // Allow some timing jitter, but should be at least ~30ms.
  EXPECT_GE(after - before, 30000u);
}

// ============================================================
// NewLogger
// ============================================================

TEST_F(EnvPosixTest, NewLogger) {
  std::string path = TestPath("log.txt");
  Logger* logger = nullptr;
  ASSERT_TRUE(env_->NewLogger(path, &logger).ok());
  ASSERT_NE(logger, nullptr);

  Log(logger, "test log message");
  delete logger;

  // Verify the file exists and contains the message.
  EXPECT_TRUE(env_->FileExists(path));
  std::string content;
  ASSERT_TRUE(ReadFile(path, &content).ok());
  EXPECT_NE(content.find("test log message"), std::string::npos);
}

// ============================================================
// SequentialFile Skip
// ============================================================

TEST_F(EnvPosixTest, SequentialFileSkip) {
  std::string path = TestPath("skip.txt");
  ASSERT_TRUE(WriteFile(path, Slice("abcdefghij")).ok());

  SequentialFile* file = nullptr;
  ASSERT_TRUE(env_->NewSequentialFile(path, &file).ok());

  char buf[8];
  Slice result;

  // Read first 3 bytes.
  ASSERT_TRUE(file->Read(3, &result, buf).ok());
  EXPECT_EQ(result, Slice("abc"));

  // Skip 4 bytes (d,e,f,g).
  ASSERT_TRUE(file->Skip(4).ok());

  // Read remaining 3 bytes.
  ASSERT_TRUE(file->Read(3, &result, buf).ok());
  EXPECT_EQ(result, Slice("hij"));

  delete file;
}

}  // namespace
}  // namespace lldb
