
set(SRCS
	base/third_party/icu/icu_utf.cc
	base/third_party/double_conversion/double-conversion/double-to-string.cc
	base/third_party/double_conversion/double-conversion/bignum-dtoa.cc
	base/third_party/double_conversion/double-conversion/strtod.cc
	base/third_party/double_conversion/double-conversion/fixed-dtoa.cc
	base/third_party/double_conversion/double-conversion/string-to-double.cc
	base/third_party/double_conversion/double-conversion/bignum.cc
	base/third_party/double_conversion/double-conversion/fast-dtoa.cc
	base/third_party/double_conversion/double-conversion/cached-powers.cc
	base/value_iterators.cc
	base/values.cc
	base/json/json_writer.cc
	base/json/string_escape.cc
	base/strings/string_util.cc
	base/strings/string_util_constants.cc
	base/strings/utf_string_conversions.cc
	base/strings/utf_string_conversion_utils.cc
	base/strings/stringprintf.cc
	base/strings/string_piece.cc
	base/strings/sys_string_conversions_posix.cc
	base/strings/string16.cc
	base/strings/string_number_conversions.cc
	third_party/blink/common/indexeddb/indexeddb_key_path.cc
	third_party/blink/common/indexeddb/indexeddb_key.cc
	content/browser/indexed_db/indexed_db_leveldb_coding.cc
	content/browser/indexed_db/indexed_db_leveldb_operations.cc
	)

	add_library(leveldb_chromium_comparator ${SRCS})
	target_include_directories(leveldb_chromium_comparator PUBLIC .)
