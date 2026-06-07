// xiebaoma

#ifndef STORAGE_LLDB_TABLE_MERGER_H_
#define STORAGE_LLDB_TABLE_MERGER_H_

namespace lldb {

class Comparator;
class Iterator;

Iterator* NewMergingIterator(const Comparator* comparator, Iterator** children,
                             int n);

}  // namespace lldb

#endif  // STORAGE_LLDB_TABLE_MERGER_H_
