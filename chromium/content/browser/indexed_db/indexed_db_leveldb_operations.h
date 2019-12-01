// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include <leveldb/comparator.h>
#include <leveldb/status.h>

// Contains common operations for LevelDBTransactions and/or LevelDBDatabases.

namespace content {
namespace indexed_db {

CONTENT_EXPORT const leveldb::Comparator* GetDefaultLevelDBComparator();

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_OPERATIONS_H_
