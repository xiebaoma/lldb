// xiebaoma

#include "lldb/options.h"

#include "lldb/comparator.h"
#include "lldb/env.h"

namespace lldb {

Options::Options() : comparator(BytewiseComparator()), env(Env::Default()) {}

}  // namespace lldb
