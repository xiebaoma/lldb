// xiebaoma

#ifndef STORAGE_LLDB_HELPERS_MEMENV_MEMENV_H_
#define STORAGE_LLDB_HELPERS_MEMENV_MEMENV_H_

namespace lldb {

class Env;

// Returns a new environment that stores its data in memory and delegates
// all non-file-storage tasks to base_env. The caller must delete the result
// when it is no longer needed.
// *base_env must remain live while the result is in use.
Env* NewMemEnv(Env* base_env);

}  // namespace lldb

#endif  // STORAGE_LLDB_HELPERS_MEMENV_MEMENV_H_
