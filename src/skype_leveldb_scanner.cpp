/*
 * skype_leveldb_scanner.cpp - scan a Skype LevelDB cache
 *
 *  Created on: Nov 23, 2019
 *      Author: Robert
 *   Copyright: Use of this source code is governed by a BSD 2-Clause license
 *              that can be found in the LICENSE file.
 */
#include "chromium_leveldb_comparator_provider.h"
#include "string_encoding_utils.h"

#include <leveldb/db.h>
#include <leveldb/slice.h>

#include <codecvt>
#include <iostream>
#include <locale>
#include <memory>
#include <ostream>
#include <sstream>
#include <variant>
#include <vector>


#define PRINT_DEBUG_DETAILS 0

#if PRINT_DEBUG_DETAILS
static void printSlice(const leveldb::Slice &slice)
{
	for (size_t i = 0; i < slice.size(); ++i) {
		if (isalnum(slice.data()[i]) || slice.data()[i] == ' ') {
			printf("%c", slice.data()[i]);
		} else {
			printf("\\x%02x", (uint8_t) slice.data()[i]);
		}
	}

	std::string u;
	for (size_t i = 0; i < slice.size(); ++i) {
		const char *p = &slice.data()[i];
		if (p[0] == 0 && (isalnum(p[1]) || isspace(p[1]))) {
			u += p[1];
			++i;
		}
	}
	printf(" (%s)", u.c_str());
}

static void printSliceSummary(const leveldb::Slice &slice)
{
	printf("%u ", (unsigned) slice.size());
	for (size_t i = 0; i < slice.size() && i < 48; ++i) {
		printf("\\x%02x", (uint8_t) slice.data()[i]);
	}
}
#endif // PRINT_DEBUG_DETAILS

template <class Function>
static bool scan_leveldb(const char *dbPath, Function scanFunction)
{
	leveldb::Options options;
	options.create_if_missing = false;
	options.comparator = leveldb_view::get_chromium_comparator();

	std::unique_ptr<leveldb::DB> db;
	{
		leveldb::DB *tempdb;
		leveldb::Status status = leveldb::DB::Open(options, dbPath, &tempdb);
		if (!status.ok()) {
			return false;
		}
		db.reset(tempdb);
	}

	using leveldb::Iterator;
	using leveldb::ReadOptions;

	std::unique_ptr<Iterator> it {db->NewIterator(ReadOptions())};
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		scanFunction(it->key(), it->value());
	}

	return it->status().ok();
}

namespace parse_result {

struct Unit : std::monostate {};

// ValueSentinel is used to signal the end of parsing of an object or of an array
struct ValueSentinel {};

class Value
{
public:
	using KeyValuePairs = std::vector<std::pair<std::string, Value>>;
	using Values = std::vector<Value>;
	using ValuePairs = std::vector<std::pair<Value, Value>>;

	// we have to use reference semantics because we can't store a Value by
	// value inside a Value.
	using KeyValuePairsPtr = std::unique_ptr<KeyValuePairs>;
	using ValuesPtr = std::unique_ptr<Values>;
	using ValuePairsPtr = std::unique_ptr<ValuePairs>;

	using Variant = std::variant<
						Unit,
						bool,
						int,
						uint64_t,
						std::string,
						KeyValuePairsPtr,
						ValuesPtr,
						ValuePairsPtr,
						ValueSentinel>;

	Value() = default;
	Value(Variant &&v) : vt_(std::move(v)) {}

	Value(const Value&) = delete;
	Value &operator=(const Value&) = delete;
	Value(Value &&) = default;
	Value &operator=(Value &&) = default;

	Variant vt_;
};

struct Visitor
{
	std::ostream &ostr_;
	int indent_ = 0;

	Visitor(std::ostream &ostr) :
			ostr_(ostr), indent_(0)
	{
	}

	void indent()
	{
		for (int i = 0; i < indent_; ++i) {
			ostr_ << "    ";
		}
	}

	void operator()(bool v) const {
		ostr_ << (v ? "True" : "False");
	}

	void operator()(int v) const {
		ostr_ << v;
	}

	void operator()(uint64_t v) const {
		ostr_ << v;
	}

	void operator()(const std::string &v) const {
		ostr_ << v;
	}

	void operator()(Unit) const {
		ostr_ << "Null";
	}

	void operator()(ValueSentinel) const {
		ostr_ << "End";
	}

	void operator()(const Value::KeyValuePairsPtr &kvs) {
		ostr_ << '\n';
		indent_++;
		for (auto const &[k, v] : *kvs.get()) {
			indent();
			ostr_ << k << '=';
			std::visit(*this, v.vt_);
			ostr_ << '\n';
		}
		indent_--;
	}

	void operator()(const Value::ValuesPtr &vs) {
		ostr_ << '\n';
		indent_++;
		for (auto &v : *vs.get()) {
			indent();
			std::visit(*this, v.vt_);
			ostr_ << '\n';
		}
		indent_--;
	}

	void operator()(const Value::ValuePairsPtr &ps) {
		indent_++;
		for (auto &[k, v] : *ps.get()) {
			indent();
			std::visit(*this, v.vt_);
			ostr_ << '\n';
		}
		indent_--;
	}
};

} // namespace parse_result

namespace parsers {

using namespace parse_result;

#if 0
namespace new_parsers {

template <class State, class Result>
using Parser = std::optional<std::pair<Result, State>>(*)(State);

} // namespace new_parsers
#endif

static size_t parseVarInt(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	if ((*i & 0x80) == 0) {
		(*p)++;
		return *i;
	}

	int count = 0;
	uint8_t buf[8] = {};
	while (*i & 0x80 && i != pend && count < (int) sizeof(buf)) {
		buf[count++] = *i & 0x7fu;
		i++;
	}

	buf[count++] = *i & 0x7fu;
	i++;

	size_t res = 0;
	assert(count > 0);
	for (; count >= 0; count--) {
		res <<= 7;
		res |= buf[count];
	}
	*p = i;
	return res;
}

static Value parseString(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '"'
	assert(*i == '"');
	i++;

	const size_t len = parseVarInt(&i, pend);
	auto result = cp::convert_iso8859_to_utf8(i, len);
	i += len;
	*p = i;
	return Value(result);
}

static Value parseUtf16String(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'c'
	assert(*i == 'c');
	i++;

	const size_t len = parseVarInt(&i, pend);

	auto stringData = reinterpret_cast<const char16_t*>(i);
	i += len;
	*p = i;
	return Value(std::wstring_convert<
			std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(
			stringData, stringData + len / sizeof(char16_t)));
}

static Value parse64BitInt(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'N'
	assert(*i == 'N');
	i++;

	uint64_t result;
	memcpy(&result, i, 8);
	i += 8;
	*p = i;
	return Value(result);
}

static Value parseInt(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '"'
	assert(*i == 'I');
	i++;

	// TODO: what about signed integers?
	int result = (int) parseVarInt(&i, pend);
	*p = i;
	return Value(result);
}

static Value parseNull(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '_'
	// I don't know what the '0' stands for, so consider it the same as null
	assert(*i == '_' || *i == '0');
	*p = i + 1;
	return Value();
}

static Value parseBool(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'F' or 'T'
	assert(*i == 'F' || *i == 'T');
	bool result = *i == 'T';
	i++;
	*p = i;
	return Value(result);
}

static Value parseArrayClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '$'
	assert(*i == '$');
	i++;
	assert(*i == '\x00');
	i += 2;
	*p = i;
	return Value(ValueSentinel());
}

static Value parseArray2ClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '@'
	assert(*i == '@');
	i += 3;
	*p = i;
	return Value(ValueSentinel());
}

static Value parseObjectClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '{'
	assert(*i == '{');
	i += 2;
	*p = i;
	return Value(ValueSentinel());
}

// forward declaration
static Value parseObject(const uint8_t **p, const uint8_t *const pend);
static Value parseArray(const uint8_t **p, const uint8_t *const pend);
static Value parseArray2(const uint8_t **p, const uint8_t *const pend);
static Value parseKey(const uint8_t **p, const uint8_t *const pend);

static Value parseVal(const uint8_t **p, const uint8_t *const pend)
{
	uint8_t tag = **p;
	if (tag == '\0' || tag == '\x01') {
		// sometimes added for padding, skip it
		(*p)++;
		tag = **p;
	}

	switch (tag) {
	case '"':
		return parseString(p, pend);
	case 'c':
		return parseUtf16String(p, pend);
	case 'N':
		return parse64BitInt(p, pend);
	case '_':
	case '0':
		return parseNull(p, pend);
	case 'F':
	case 'T':
		return parseBool(p, pend);
	case 'o':
		return parseObject(p, pend);
	case 'A':
		return parseArray(p, pend);
	case 'a':
		return parseArray2(p, pend);
	case '$':
		return parseArrayClosingTag(p, pend);
	case '@':
		return parseArray2ClosingTag(p, pend);
	case '{':
		return parseObjectClosingTag(p, pend);
	case 'I':
		return parseInt(p, pend);
	default:
		assert(false);
		break;
	}
}

static Value parseKey(const uint8_t **p, const uint8_t *const pend)
{
	// returned object should be either string or object terminator sentinel
	return parseVal(p, pend);
}

static Value parseObject(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'o');
	(*p)++;

	Value result(std::make_unique<Value::KeyValuePairs>());
	Value k;
	Value v;

	Value::KeyValuePairs *ps = std::get<Value::KeyValuePairsPtr>(result.vt_).get();
	while (true) {
		k = parseKey(p, pend);
		if (!std::holds_alternative<std::string>(k.vt_)) {
			break;
		}
		v = parseVal(p, pend);
		ps->emplace_back(std::move(std::get<std::string>(k.vt_)), std::move(v));
	}
	return result;
}

static Value parseArray(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'A');
	(*p)++;

	const size_t len = parseVarInt(p, pend);

	Value result(std::make_unique<Value::Values>());
	Value::Values *vs = std::get<Value::ValuesPtr>(result.vt_).get();

	for (size_t i = 0; i < len; ++i) {
		vs->emplace_back(parseVal(p, pend));
	}

	parseVal(p, pend); // discard array terminator
	return result;
}

static Value parseArray2(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'a');
	(*p)++;

	const size_t len = parseVarInt(p, pend);

	Value result(std::make_unique<Value::ValuePairs>());
	Value::ValuePairs *ps = std::get<Value::ValuePairsPtr>(result.vt_).get();

	Value v1;
	for (size_t i = 0; i < len; ++i) {
		v1 = parseVal(p, pend);
		ps->emplace_back(std::move(v1), parseVal(p, pend));
	}

	parseVal(p, pend); // discard array terminator
	return result;
}

} // namespace parsers

static parsers::Value parse_skype_contact_blob(const uint8_t *data, size_t size)
{
	using namespace parsers;

	const uint8_t *p = data;
	const uint8_t * const pend = data + size;

	// first field is a Varint, maybe the record ID
	parseVarInt(&p, pend);
	// expect 0xff
	assert(*p == 0xff);
	p++;
	parseVarInt(&p, pend);
	// expect 0xff
	assert(*p == 0xff);
	p++;

	// expect 0x0d
	assert(*p == 0x0d);
	p++;

	// 2. expect object 'o'
	return parseVal(&p, pend);
}

static std::string skypeTimestampToString(uint64_t ts)
{
	time_t t = (ts - 4782822804267467000) / 4096000;
	struct tm parts = {};
	gmtime_r(&t, &parts);
	char buffer[80];
	snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d-%02d-%02dZ",
			 parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
			 parts.tm_hour, parts.tm_min, parts.tm_sec);
	return buffer;
}

static std::string &toCsvFieldValue(std::string &val)
{
	auto pos = val.find_first_of(",\"\n\r");
	if (pos == std::string::npos) {
		return val;
	}

	val.insert(0, 1, '"');
	pos++;
	if (val[pos] == '"') {
		val.insert(pos, 1, '"');
		pos += 2;
	}
	while (true) {
		pos = val.find('"', pos);
		if (pos != std::string::npos) {
			val.insert(pos, 1, '"');
			pos += 2;
		} else {
			break;
		}
	}
	val += '"';
	return val;
}

static std::string show_skype_message(const parsers::Value &v,
									  const bool useCsvFormat)
{
	using namespace parsers;

	if (!std::holds_alternative<Value::KeyValuePairsPtr>(v.vt_)) {
		return std::string();
	}

	auto pairs = std::get<Value::KeyValuePairsPtr>(v.vt_).get();
	std::ostringstream os;

	bool messageOk = false;
	std::string currentValue;
	for (auto const &[k, val] : *pairs) {
		if (k == "messagetype") {
			auto const &mtype = std::get<std::string>(val.vt_);
			messageOk = (mtype == "RichText" || mtype == "Text");
			continue;
		} else if (k == "cuid" || k == "conversationId" || k == "creator") {
			currentValue = std::get<std::string>(val.vt_);
		} else if (k == "createdTime" || k == "composeTime") {
			currentValue = skypeTimestampToString(std::get<uint64_t>(val.vt_));
		} else if (k == "content") {
			currentValue = std::get<std::string>(val.vt_);
		} else {
			continue;
		}

		if (!useCsvFormat) {
			if (k != "content") {
				os << k << '=' << currentValue << '\n';
			} else {
				os << '\n' << std::get<std::string>(val.vt_) << '\n';
			}
		} else {
			os << toCsvFieldValue(currentValue);
			if (k != "content") {
				os << ',';
			}
		}
	}

	if (messageOk) {
		return os.str();
	} else {
		return std::string();
	}
}

static parsers::Value parse_skype_message_blob(const uint8_t *data,
											   const size_t size)
{
	using namespace parsers;

	const uint8_t *p = data;
	const uint8_t * const pend = data + size;

	// first field is a Varint, maybe the record ID
	parseVarInt(&p, pend);
	if (*p != 0xff) {
		// unexpected record type
		return parsers::Value();
	}

	// expect 0xff
	assert(*p == 0xff);
	p++;

	// expect 0x12
	assert(*p == 0x12 || *p == 0x13 || *p == 0x14);
	p++;

	// expect 0xff
	assert(*p == 0xff);
	p++;

	// expect 0x0d
	assert(*p == 0x0d);
	p++;

	// 2. expect object 'o'
	return parseVal(&p, pend);
}

static std::string show_skype_message_blob(const uint8_t *data,
										   const size_t size, bool useCsvFormat)
{
	return show_skype_message(parse_skype_message_blob(data, size),
							  useCsvFormat);
}

int showUsage(const char *execPath)
{
	std::unique_ptr<char> pathCopy{strdup(execPath)};
	const char *baseName = basename(pathCopy.get());
	printf("USAGE: %s [options] <LEVELDB_PATH>\n\n"
			"OPTIONS:\n"
			"\t-h   - show this help\n"
			"\t-m   - display messages instead of contacts\n"
			"\t-csv - display messages in CSV format\n\n"
			"EXAMPLE:\n"
			"\t%s ~/.config/skypeforlinux/IndexedDB/file__0.indexeddb.leveldb\n",
			baseName, baseName);
	return 1;
}

static const leveldb::Slice contactPrefixKeySlice("\x00\x01\x06\x01\x01", 5);
static const leveldb::Slice msgPrefixKeySlice1("\x00\x01\x02\x01\x01\x24\x00", 7);
static const leveldb::Slice msgPrefixKeySlice2("\x00\x01\x01\x01\x04\x02\x01", 7);
static const leveldb::Slice msgPrefixKeySlice3("\x00\x01\x04\x01\x01", 5);

int main(int argc, char *argv[])
{
	using leveldb::Slice;

	// parse the command line arguments
	bool showHelp = (argc < 2);
	bool showMessages = false;
	bool useCsvFormat = false;
	const char *dbPath = nullptr;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-m") == 0) {
			showMessages = true;
		} else if (strcmp(argv[i], "-csv") == 0) {
			useCsvFormat = true;
		} else if (strcmp(argv[i], "-h") == 0) {
			showHelp = true;
		} else {
			dbPath = argv[i];
		}
	}

	if (showHelp || !dbPath) {
		return showUsage(argv[0]);
	}

	auto scanFunction = [showMessages, useCsvFormat](Slice key, Slice value) {

#if PRINT_DEBUG_DETAILS
		printf("key:  ");
		printSlice(key);
		printf("\n");
		printf("data: ");
		printSlice(value);
		printf("\nsummary: ");
		printSliceSummary(key);
		printf("\n\n");
#endif

		if (showMessages) {
			if (key.starts_with(msgPrefixKeySlice1) ||
				key.starts_with(msgPrefixKeySlice2) ||
				key.starts_with(msgPrefixKeySlice3)) {
				auto formatedMsg = show_skype_message_blob(
						reinterpret_cast<const uint8_t *>(value.data()),
						value.size(), useCsvFormat);
				if (!formatedMsg.empty()) {
					std::cout << formatedMsg << '\n';
					if (!useCsvFormat) {
						std::cout << '\n';
					}
				}
			}
			return;
		}

		if (key.starts_with(contactPrefixKeySlice)) {
			auto v = parse_skype_contact_blob(
					reinterpret_cast<const uint8_t *>(value.data()),
					value.size());

			auto &ostr = std::cout;
			using parse_result::Visitor;
			ostr << "BEGIN Contact -----\n";
			std::visit(Visitor(ostr), v.vt_);
			ostr << "END Contact -----\n";
		}
	};

	return scan_leveldb(dbPath, scanFunction) ? 0 : 1;
}
