/*
 * string_encoding_utils.h - convert ISO-8859-1 to UTF-8
 *
 *  Created on: Jun 7, 2021
 *      Author: Robert
 *   Copyright: Use of this source code is governed by a BSD 2-Clause license
 *              that can be found in the LICENSE file.
 */
#ifndef SRC_STRING_ENCODING_UTILS_H_
#define SRC_STRING_ENCODING_UTILS_H_

#include <cstdint>
#include <string>

namespace cp {

std::string convert_iso8859_to_utf8(const uint8_t *data, size_t length);

} /* namespace cp */

#endif /* SRC_STRING_ENCODING_UTILS_H_ */
