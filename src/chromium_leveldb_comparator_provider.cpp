/*
 * chromium_leveldb_comparator_provider.cpp - Chromium compatible LevelDB comparator
 *
 *  Created on: Nov 23, 2019
 *      Author: Robert
 *   Copyright: Use of this source code is governed by a BSD 2-Clause license
 *              that can be found in the LICENSE file.
 */
#include "chromium_leveldb_comparator_provider.h"

#include <content/browser/indexed_db/indexed_db_leveldb_operations.h>


namespace leveldb_view {

const leveldb::Comparator *get_chromium_comparator()
{
	return content::indexed_db::GetDefaultLevelDBComparator();
}

} /* namespace leveldb_view */
