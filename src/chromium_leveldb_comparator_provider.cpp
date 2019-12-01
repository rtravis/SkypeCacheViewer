/*
 * chromium_leveldb_comparator_provider.cpp
 *
 *  Created on: Nov 23, 2019
 *      Author: Robert
 */

#include <chromium_leveldb_comparator_provider.h>

#include <content/browser/indexed_db/indexed_db_leveldb_operations.h>


namespace leveldb_view {

const leveldb::Comparator *get_chromium_comparator()
{
	return content::indexed_db::GetDefaultLevelDBComparator();
}

} /* namespace leveldb_view */
