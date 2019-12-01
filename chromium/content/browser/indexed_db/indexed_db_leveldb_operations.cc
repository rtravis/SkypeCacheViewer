// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"

#include "base/no_destructor.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

using base::StringPiece;
using blink::IndexedDBKeyPath;
using leveldb::Status;

namespace leveldb_env {

static base::StringPiece MakeStringPiece(const leveldb::Slice& s) {
  return base::StringPiece(s.data(), s.size());
}

}  // namespace leveldb_env


namespace content {
namespace indexed_db {
namespace {

class LDBComparator : public leveldb::Comparator {
 public:
  LDBComparator() = default;
  ~LDBComparator() override = default;
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const override {
    return content::Compare(leveldb_env::MakeStringPiece(a),
                            leveldb_env::MakeStringPiece(b),
                            false /*index_keys*/);
  }
  const char* Name() const override { return "idb_cmp1"; }
  void FindShortestSeparator(std::string* start,
                             const leveldb::Slice& limit) const override {}
  void FindShortSuccessor(std::string* key) const override {}
};

}  // namespace

const leveldb::Comparator* GetDefaultLevelDBComparator() {
  static const base::NoDestructor<LDBComparator> ldb_comparator;
  return ldb_comparator.get();
}

}  // namespace indexed_db
}  // namespace content
