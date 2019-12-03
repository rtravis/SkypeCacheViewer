/*
 * chromium_leveldb_comparator_provider.h - Chromium compatible LevelDB comparator
 *
 *  Created on: Nov 23, 2019
 *      Author: Robert
 *   Copyright: Use of this source code is governed by a BSD 2-Clause license
 *              that can be found in the LICENSE file.
 */
#ifndef SRC_CHROMIUM_LEVELDB_COMPARATOR_PROVIDER_H_
#define SRC_CHROMIUM_LEVELDB_COMPARATOR_PROVIDER_H_

namespace leveldb {
class Comparator;
}

namespace leveldb_view {

const leveldb::Comparator *get_chromium_comparator();

} /* namespace leveldb_view */

#endif /* SRC_CHROMIUM_LEVELDB_COMPARATOR_PROVIDER_H_ */
