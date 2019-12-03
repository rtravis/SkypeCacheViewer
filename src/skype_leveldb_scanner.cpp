/*
 * skype_leveldb_scanner.cpp - scan a Skype LevelDB cache
 *
 *  Created on: Nov 23, 2019
 *      Author: Robert
 *   Copyright: Use of this source code is governed by a BSD 2-Clause license
 *              that can be found in the LICENSE file.
 */
#include "chromium_leveldb_comparator_provider.h"

#include <leveldb/db.h>
#include <leveldb/slice.h>

#include <codecvt>
#include <iostream>
#include <locale>
#include <variant>


#if 0
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
	for (size_t i = 0; i < slice.size() && i < 32; ++i) {
		printf("\\x%02x", (uint8_t) slice.data()[i]);
	}
}
#endif

template <class Function>
static bool scan_leveldb(const char *dbPath, Function scanFunction)
{
	leveldb::DB* db;
	leveldb::Options options;
	options.create_if_missing = false;
	options.comparator = leveldb_view::get_chromium_comparator();

	leveldb::Status status = leveldb::DB::Open(options, dbPath, &db);
	assert(status.ok());
	if (!status.ok()) {
		return false;
	}

	// TODO: use smart pointers
	leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		scanFunction(it->key(), it->value());
	}

	bool ok = it->status().ok();
	assert(ok); // Check for any errors found during the scan
	delete it;
	delete db;
	return ok;
}

struct UnitType : std::monostate
{
};

struct SentinelType // signal the end of parsing of an object or of an array
{
};

class ValueType
{
public:
	using KeyValuePairs = std::vector<std::pair<std::string, ValueType>>;
	using Values = std::vector<ValueType>;
	using ValuePairs = std::vector<std::pair<ValueType, ValueType>>;

	using KeyValuePairsPtr = std::unique_ptr<KeyValuePairs>;
	using ValuesPtr = std::unique_ptr<Values>;
	using ValuePairsPtr = std::unique_ptr<ValuePairs>;

	using Variant = std::variant<UnitType, bool, int, uint64_t, std::string,
			KeyValuePairsPtr, ValuesPtr, ValuePairsPtr, SentinelType>;

	ValueType() = default;
	ValueType(Variant &&v) : vt_(std::move(v)) {}

	ValueType(const ValueType&) = delete;
	ValueType &operator=(const ValueType&) = delete;
	ValueType(ValueType &&) = default;
	ValueType &operator=(ValueType &&) = default;

	Variant vt_;
};

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

static ValueType parseString(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '"'
	assert(*i == '"');
	i++;

	const size_t len = parseVarInt(&i, pend);
	std::string result(i, i + len);
	i += len;
	*p = i;
	return ValueType(result);
}

static ValueType parseUtf16String(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '"'
	assert(*i == 'c');
	i++;

	const size_t len = parseVarInt(&i, pend);

	auto stringData = reinterpret_cast<const char16_t*>(i);
	i += len;
	*p = i;
	return ValueType(std::wstring_convert<
			std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(
			stringData, stringData + len / sizeof(char16_t)));
}

static ValueType parse64BitInt(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'N'
	assert(*i == 'N');
	i++;

	uint64_t result;
	memcpy(&result, i, 8);
	i += 8;
	*p = i;
	return ValueType(result);
}

static ValueType parseInt(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '"'
	assert(*i == 'I');
	i++;

	// TODO: what about signed integers?
	int result = (int) parseVarInt(&i, pend);
	*p = i;
	return ValueType(result);
}

static ValueType parseNull(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'N'
	assert(*i == '_');
	i++;
	*p = i;
	return ValueType();
}

static ValueType parseBool(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == 'F' or 'T'
	assert(*i == 'F' || *i == 'T');
	bool result = *i == 'T';
	i++;
	*p = i;
	return ValueType(result);
}

static ValueType parseArrayClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '$'
	assert(*i == '$');
	i++;
	assert(*i == '\x00');
	i += 2;
	*p = i;
	return ValueType(SentinelType());
}

static ValueType parseArray2ClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '@'
	assert(*i == '@');
	i += 3;
	*p = i;
	return ValueType(SentinelType());
}

static ValueType parseObjectClosingTag(const uint8_t **p, const uint8_t *const pend)
{
	const uint8_t *i = *p;
	// expect *i == '{'
	assert(*i == '{');
	i += 2;
	*p = i;
	return ValueType(SentinelType());
}

// forward declaration
static ValueType parseObject(const uint8_t **p, const uint8_t *const pend);
static ValueType parseArray(const uint8_t **p, const uint8_t *const pend);
static ValueType parseArray2(const uint8_t **p, const uint8_t *const pend);
static ValueType parseKey(const uint8_t **p, const uint8_t *const pend);

static ValueType parseVal(const uint8_t **p, const uint8_t *const pend)
{
	uint8_t tag = **p;
	switch (tag) {
	case '"':
		return parseString(p, pend);
	case '\0': // sometimes added for padding, skip it
		(*p)++;
		[[fallthrough]];
	case 'c':
		return parseUtf16String(p, pend);
	case 'N':
		return parse64BitInt(p, pend);
	case '_':
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

static ValueType parseKey(const uint8_t **p, const uint8_t *const pend)
{
	// returned object should be either string or object terminator sentinel
	return parseVal(p, pend);
}

static ValueType parseObject(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'o');
	(*p)++;

	ValueType result(std::make_unique<ValueType::KeyValuePairs>());
	ValueType k;
	ValueType v;

	ValueType::KeyValuePairs *ps = std::get<ValueType::KeyValuePairsPtr>(result.vt_).get();
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

static ValueType parseArray(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'A');
	(*p)++;

	const size_t len = parseVarInt(p, pend);

	ValueType result(std::make_unique<ValueType::Values>());
	ValueType::Values *vs = std::get<ValueType::ValuesPtr>(result.vt_).get();

	for (size_t i = 0; i < len; ++i) {
		vs->emplace_back(parseVal(p, pend));
	}

	parseVal(p, pend); // discard array terminator
	return result;
}

static ValueType parseArray2(const uint8_t **p, const uint8_t *const pend)
{
	// expect **p == 'o'
	assert(**p == 'a');
	(*p)++;

	const size_t len = parseVarInt(p, pend);

	ValueType result(std::make_unique<ValueType::ValuePairs>());
	ValueType::ValuePairs *ps = std::get<ValueType::ValuePairsPtr>(result.vt_).get();

	ValueType v1;
	for (size_t i = 0; i < len; ++i) {
		v1 = parseVal(p, pend);
		ps->emplace_back(std::move(v1), parseVal(p, pend));
	}

	parseVal(p, pend); // discard array terminator
	return result;
}

using std::cout;

struct Visitor
{
	int indent_ = 0;

	void indent()
	{
		for (int i = 0; i < indent_; ++i) {
			cout << "    ";
		}
	}

	void operator()(bool v) const {
		cout << (v ? "True" : "False");
	}

	void operator()(int v) const {
		cout << v;
	}

	void operator()(uint64_t v) const {
		cout << v;
	}

	void operator()(const std::string &v) const {
		cout << v;
	}

	void operator()(UnitType) const {
		cout << "Null";
	}

	void operator()(SentinelType) const {
		cout << "End";
	}

	void operator()(const ValueType::KeyValuePairsPtr &kvs) {
		cout << '\n';
		indent_++;
		for (auto const &[k, v] : *kvs.get()) {
			indent();
			cout << k << '=';
			std::visit(*this, v.vt_);
			cout << '\n';
		}
		indent_--;
	}

	void operator()(const ValueType::ValuesPtr &vs) {
		cout << '\n';
		indent_++;
		for (auto &v : *vs.get()) {
			indent();
			std::visit(*this, v.vt_);
			cout << '\n';
		}
		indent_--;
	}

	void operator()(const ValueType::ValuePairsPtr &ps) {
		indent_++;
		for (auto &[k, v] : *ps.get()) {
			indent();
			std::visit(*this, v.vt_);
			cout << '\n';
		}
		indent_--;
	}
};

static bool parse_skype_contact_blob(const uint8_t *data, size_t size)
{
	std::cout << "START Contact-----\n";
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
	ValueType v = parseVal(&p, pend);
	std::visit(Visitor(), v.vt_);
	std::cout << "~~~~~~~~~~ Contact\n";
	return true;
}

#if 0
static bool parse_skype_message_blob(const uint8_t *data, size_t size)
{
	std::cout << "START Message-----\n";
	const uint8_t *p = data;
	const uint8_t * const pend = data + size;

	// first field is a Varint, maybe the record ID
	parseVarInt(&p, pend);
	// expect 0xff
	assert(*p == 0xff);
	p++;

	// expect 0x12
	assert(*p == 0x12);
	p++;

	// expect 0xff
	assert(*p == 0xff);
	p++;

	// expect 0x0d
	assert(*p == 0x0d);
	p++;

	// 2. expect object 'o'
	ValueType v = parseVal(&p, pend);
	std::visit(Visitor(), v.vt_);
	std::cout << "~~~~~~~~~~ Message\n";
	return true;
}
#endif

int main(int argc, char *argv[])
{
	using leveldb::Slice;

	auto scanFunction = [](Slice key, Slice value) -> void {

#if 0
		printf("key:  ");
		printSlice(key);
		printf("\n");
		printf("data: ");
		printSlice(value);
		printf("\n\n");
		printSliceSummary(key);
		printf("\n");
#endif

		static const leveldb::Slice contactPrefixKeySlice("\x00\x01\x06\x01\x01", 5);
		if (key.starts_with(contactPrefixKeySlice)) {
			parse_skype_contact_blob(reinterpret_cast<const uint8_t*>(value.data()),
					value.size());
		}
	};

	const char *dbPath = argv[1];
	return scan_leveldb(dbPath, scanFunction) ? 0 : 1;
}
