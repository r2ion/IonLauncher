#include "particledocument.h"


#include <algorithm>
#include <bit>
#include <charconv>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <utility>

namespace ParticleTools
{
namespace DmxBinary
{
constexpr std::string_view Header = "<!-- dmx encoding binary 5 format pcf 2 -->\n";
constexpr int32_t NullElementIndex = -1;
constexpr int32_t ExternalElementIndex = -2;
constexpr size_t ElementDictionarySize = 24;
constexpr size_t AttributeHeaderSize = 5;

using ByteVector = std::vector<uint8_t>;
using FloatTuple = std::vector<float>;
using ColorValue = std::array<uint8_t, 4>;

class Reader
{
public:
	explicit Reader(const ByteVector& bytes) : m_Bytes(bytes) {}

	size_t Remaining() const
	{
		return m_Bytes.size() - m_Position;
	}

	bool Finished() const
	{
		return m_Position == m_Bytes.size();
	}

	bool ReadByte(uint8_t& value)
	{
		if (Remaining() < 1)
			return false;
		value = m_Bytes[m_Position++];
		return true;
	}

	bool ReadInt32(int32_t& value)
	{
		uint32_t bits = 0;
		if (!ReadUInt32(bits))
			return false;
		value = std::bit_cast<int32_t>(bits);
		return true;
	}

	bool ReadUInt32(uint32_t& value)
	{
		if (Remaining() < 4)
			return false;
		value = static_cast<uint32_t>(m_Bytes[m_Position]) |
			(static_cast<uint32_t>(m_Bytes[m_Position + 1]) << 8) |
			(static_cast<uint32_t>(m_Bytes[m_Position + 2]) << 16) |
			(static_cast<uint32_t>(m_Bytes[m_Position + 3]) << 24);
		m_Position += 4;
		return true;
	}

	bool ReadFloat(float& value)
	{
		uint32_t bits = 0;
		if (!ReadUInt32(bits))
			return false;
		value = std::bit_cast<float>(bits);
		return true;
	}

	bool ReadBytes(size_t count, uint8_t* destination)
	{
		if (count > Remaining())
			return false;
		if (count != 0)
			std::memcpy(destination, m_Bytes.data() + m_Position, count);
		m_Position += count;
		return true;
	}

	bool ReadBlock(size_t count, ByteVector& value)
	{
		if (count > Remaining())
			return false;
		value.assign(m_Bytes.begin() + m_Position, m_Bytes.begin() + m_Position + count);
		m_Position += count;
		return true;
	}

	bool ReadCString(std::string& value)
	{
		const auto begin = m_Bytes.begin() + m_Position;
		const auto end = std::find(begin, m_Bytes.end(), uint8_t{0});
		if (end == m_Bytes.end())
			return false;
		value.assign(reinterpret_cast<const char*>(m_Bytes.data() + m_Position), static_cast<size_t>(end - begin));
		m_Position += static_cast<size_t>(end - begin) + 1;
		return true;
	}

private:
	const ByteVector& m_Bytes;
	size_t m_Position = 0;
};

class Writer
{
public:
	void WriteByte(uint8_t value)
	{
		m_Bytes.push_back(value);
	}

	void WriteInt32(int32_t value)
	{
		WriteUInt32(std::bit_cast<uint32_t>(value));
	}

	void WriteUInt32(uint32_t value)
	{
		m_Bytes.push_back(static_cast<uint8_t>(value));
		m_Bytes.push_back(static_cast<uint8_t>(value >> 8));
		m_Bytes.push_back(static_cast<uint8_t>(value >> 16));
		m_Bytes.push_back(static_cast<uint8_t>(value >> 24));
	}

	void WriteFloat(float value)
	{
		WriteUInt32(std::bit_cast<uint32_t>(value));
	}

	void WriteBytes(const uint8_t* data, size_t count)
	{
		if (count != 0)
			m_Bytes.insert(m_Bytes.end(), data, data + count);
	}

	void WriteCString(std::string_view value)
	{
		m_Bytes.insert(m_Bytes.end(), value.begin(), value.end());
		m_Bytes.push_back(0);
	}

	const ByteVector& Bytes() const
	{
		return m_Bytes;
	}

private:
	ByteVector m_Bytes;
};

class TextParser
{
public:
	explicit TextParser(std::string_view text) : m_Text(text) {}

	void SkipWhitespace()
	{
		while (m_Position < m_Text.size() &&
			(m_Text[m_Position] == ' ' || m_Text[m_Position] == '\t' || m_Text[m_Position] == '\r' || m_Text[m_Position] == '\n'))
			++m_Position;
	}

	bool Consume(char character)
	{
		SkipWhitespace();
		if (m_Position >= m_Text.size() || m_Text[m_Position] != character)
			return false;
		++m_Position;
		return true;
	}

	bool Peek(char character)
	{
		SkipWhitespace();
		return m_Position < m_Text.size() && m_Text[m_Position] == character;
	}

	bool Finished()
	{
		SkipWhitespace();
		return m_Position == m_Text.size();
	}

	bool ReadToken(std::string_view& token)
	{
		SkipWhitespace();
		const size_t begin = m_Position;
		while (m_Position < m_Text.size())
		{
			const char character = m_Text[m_Position];
			if (character == ',' || character == '[' || character == ']' || character == ' ' || character == '\t' || character == '\r' || character == '\n')
				break;
			++m_Position;
		}
		if (m_Position == begin)
			return false;
		token = m_Text.substr(begin, m_Position - begin);
		return true;
	}

	bool ReadString(std::string& value)
	{
		SkipWhitespace();
		if (m_Position >= m_Text.size() || m_Text[m_Position++] != '"')
			return false;
		value.clear();
		while (m_Position < m_Text.size())
		{
			const unsigned char character = static_cast<unsigned char>(m_Text[m_Position++]);
			if (character == '"')
				return true;
			if (character != '\\')
			{
				if (character < 0x20)
					return false;
				value.push_back(static_cast<char>(character));
				continue;
			}
			if (m_Position >= m_Text.size())
				return false;
			const char escaped = m_Text[m_Position++];
			switch (escaped)
			{
			case '"': value.push_back('"'); break;
			case '\\': value.push_back('\\'); break;
			case '/': value.push_back('/'); break;
			case 'b': value.push_back('\b'); break;
			case 'f': value.push_back('\f'); break;
			case 'n': value.push_back('\n'); break;
			case 'r': value.push_back('\r'); break;
			case 't': value.push_back('\t'); break;
			case 'u':
			{
				uint32_t codePoint = 0;
				if (!ReadHexCodeUnit(codePoint))
					return false;
				if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
				{
					if (m_Position + 2 > m_Text.size() || m_Text[m_Position] != '\\' || m_Text[m_Position + 1] != 'u')
						return false;
					m_Position += 2;
					uint32_t low = 0;
					if (!ReadHexCodeUnit(low) || low < 0xDC00 || low > 0xDFFF)
						return false;
					codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
				}
				else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
					return false;
				AppendUtf8(codePoint, value);
				break;
			}
			default:
				return false;
			}
		}
		return false;
	}

private:
	static int HexValue(char character)
	{
		if (character >= '0' && character <= '9')
			return character - '0';
		if (character >= 'a' && character <= 'f')
			return character - 'a' + 10;
		if (character >= 'A' && character <= 'F')
			return character - 'A' + 10;
		return -1;
	}

	bool ReadHexCodeUnit(uint32_t& value)
	{
		if (m_Position + 4 > m_Text.size())
			return false;
		value = 0;
		for (size_t index = 0; index < 4; ++index)
		{
			const int digit = HexValue(m_Text[m_Position++]);
			if (digit < 0)
				return false;
			value = (value << 4) | static_cast<uint32_t>(digit);
		}
		return true;
	}

	static void AppendUtf8(uint32_t codePoint, std::string& value)
	{
		if (codePoint <= 0x7F)
			value.push_back(static_cast<char>(codePoint));
		else if (codePoint <= 0x7FF)
		{
			value.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
			value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
		else if (codePoint <= 0xFFFF)
		{
			value.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
			value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
			value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
		else
		{
			value.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
			value.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
			value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
			value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
	}

	std::string_view m_Text;
	size_t m_Position = 0;
};

bool ContainsNull(std::string_view value)
{
	return value.find('\0') != std::string_view::npos;
}

std::string FormatInt(int32_t value)
{
	char buffer[32];
	const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
	return std::string(buffer, result.ptr);
}

std::string FormatFloat(float value)
{
	char buffer[64];
	const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value, std::chars_format::general,
		std::numeric_limits<float>::max_digits10);
	if (result.ec == std::errc{})
		return std::string(buffer, result.ptr);
	return "0";
}

bool ParseIntToken(std::string_view token, int32_t& value)
{
	if (token.empty())
		return false;
	const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
	return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

bool ParseFloatToken(std::string_view token, float& value)
{
	if (token.empty())
		return false;
	const auto result = std::from_chars(token.data(), token.data() + token.size(), value, std::chars_format::general);
	return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

bool ParseBoolToken(std::string_view token, bool& value)
{
	if (token == "true" || token == "1")
	{
		value = true;
		return true;
	}
	if (token == "false" || token == "0")
	{
		value = false;
		return true;
	}
	return false;
}

std::string EscapeString(std::string_view value)
{
	static constexpr char Hex[] = "0123456789abcdef";
	std::string result;
	result.reserve(value.size() + 2);
	result.push_back('"');
	for (const unsigned char character : value)
	{
		switch (character)
		{
		case '"': result += "\\\""; break;
		case '\\': result += "\\\\"; break;
		case '\b': result += "\\b"; break;
		case '\f': result += "\\f"; break;
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default:
			if (character < 0x20)
			{
				result += "\\u00";
				result.push_back(Hex[character >> 4]);
				result.push_back(Hex[character & 15]);
			}
			else
				result.push_back(static_cast<char>(character));
			break;
		}
	}
	result.push_back('"');
	return result;
}

std::string FormatHex(const ByteVector& value)
{
	static constexpr char Hex[] = "0123456789abcdef";
	std::string result;
	result.reserve(value.size() * 2);
	for (const uint8_t byte : value)
	{
		result.push_back(Hex[byte >> 4]);
		result.push_back(Hex[byte & 15]);
	}
	return result;
}

bool ParseHex(std::string_view text, ByteVector& value)
{
	if ((text.size() & 1) != 0)
		return false;
	value.clear();
	value.reserve(text.size() / 2);
	auto hexValue = [](char character) -> int
	{
		if (character >= '0' && character <= '9') return character - '0';
		if (character >= 'a' && character <= 'f') return character - 'a' + 10;
		if (character >= 'A' && character <= 'F') return character - 'A' + 10;
		return -1;
	};
	for (size_t index = 0; index < text.size(); index += 2)
	{
		const int high = hexValue(text[index]);
		const int low = hexValue(text[index + 1]);
		if (high < 0 || low < 0)
			return false;
		value.push_back(static_cast<uint8_t>((high << 4) | low));
	}
	return true;
}

bool ParseScalarInt(std::string_view text, int32_t& value)
{
	TextParser parser(text);
	std::string_view token;
	return parser.ReadToken(token) && ParseIntToken(token, value) && parser.Finished();
}

bool ParseScalarFloat(std::string_view text, float& value)
{
	TextParser parser(text);
	std::string_view token;
	return parser.ReadToken(token) && ParseFloatToken(token, value) && parser.Finished();
}

bool ParseScalarBool(std::string_view text, bool& value)
{
	TextParser parser(text);
	std::string_view token;
	return parser.ReadToken(token) && ParseBoolToken(token, value) && parser.Finished();
}

bool ParseTuple(std::string_view text, size_t size, FloatTuple& values, bool bracketed)
{
	TextParser parser(text);
	if (bracketed && !parser.Consume('['))
		return false;
	values.clear();
	values.reserve(size);
	for (size_t index = 0; index < size; ++index)
	{
		if (index != 0)
			parser.Consume(',');
		std::string_view token;
		float value = 0.0f;
		if (!parser.ReadToken(token) || !ParseFloatToken(token, value))
			return false;
		values.push_back(value);
	}
	if (bracketed && !parser.Consume(']'))
		return false;
	return parser.Finished();
}

std::string FormatTuple(const FloatTuple& values, bool bracketed)
{
	std::string result = bracketed ? "[" : "";
	for (size_t index = 0; index < values.size(); ++index)
	{
		if (index != 0)
			result += bracketed ? ", " : " ";
		result += FormatFloat(values[index]);
	}
	if (bracketed)
		result += ']';
	return result;
}

bool ParseColor(std::string_view text, ColorValue& value, bool bracketed)
{
	TextParser parser(text);
	if (bracketed && !parser.Consume('['))
		return false;
	for (size_t index = 0; index < value.size(); ++index)
	{
		if (index != 0)
			parser.Consume(',');
		std::string_view token;
		int32_t component = 0;
		if (!parser.ReadToken(token) || !ParseIntToken(token, component) || component < 0 || component > 255)
			return false;
		value[index] = static_cast<uint8_t>(component);
	}
	if (bracketed && !parser.Consume(']'))
		return false;
	return parser.Finished();
}

std::string FormatColor(const ColorValue& value, bool bracketed)
{
	std::string result = bracketed ? "[" : "";
	for (size_t index = 0; index < value.size(); ++index)
	{
		if (index != 0)
			result += bracketed ? ", " : " ";
		result += FormatInt(value[index]);
	}
	if (bracketed)
		result += ']';
	return result;
}

template <typename Value, typename ParseValue>
bool ParseArray(std::string_view text, std::vector<Value>& values, ParseValue parseValue)
{
	TextParser parser(text);
	if (!parser.Consume('['))
		return false;
	values.clear();
	if (parser.Consume(']'))
		return parser.Finished();
	while (true)
	{
		Value value{};
		if (!parseValue(parser, value))
			return false;
		values.push_back(std::move(value));
		if (parser.Consume(']'))
			return parser.Finished();
		if (!parser.Consume(','))
			return false;
	}
}

bool ParseIntArray(std::string_view text, std::vector<int32_t>& values)
{
	return ParseArray<int32_t>(text, values, [](TextParser& parser, int32_t& value)
	{
		std::string_view token;
		return parser.ReadToken(token) && ParseIntToken(token, value);
	});
}

bool ParseFloatArray(std::string_view text, std::vector<float>& values)
{
	return ParseArray<float>(text, values, [](TextParser& parser, float& value)
	{
		std::string_view token;
		return parser.ReadToken(token) && ParseFloatToken(token, value);
	});
}

bool ParseBoolArray(std::string_view text, std::vector<bool>& values)
{
	return ParseArray<bool>(text, values, [](TextParser& parser, bool& value)
	{
		std::string_view token;
		return parser.ReadToken(token) && ParseBoolToken(token, value);
	});
}

bool ParseStringArray(std::string_view text, std::vector<std::string>& values)
{
	return ParseArray<std::string>(text, values, [](TextParser& parser, std::string& value)
	{
		return parser.ReadString(value) && !ContainsNull(value);
	});
}

bool ParseBlobArray(std::string_view text, std::vector<ByteVector>& values)
{
	return ParseArray<ByteVector>(text, values, [](TextParser& parser, ByteVector& value)
	{
		std::string encoded;
		return parser.ReadString(encoded) && ParseHex(encoded, value);
	});
}

bool ParseTupleArray(std::string_view text, size_t tupleSize, std::vector<FloatTuple>& values)
{
	return ParseArray<FloatTuple>(text, values, [tupleSize](TextParser& parser, FloatTuple& value)
	{
		if (!parser.Consume('['))
			return false;
		value.clear();
		value.reserve(tupleSize);
		for (size_t index = 0; index < tupleSize; ++index)
		{
			if (index != 0 && !parser.Consume(','))
				return false;
			std::string_view token;
			float component = 0.0f;
			if (!parser.ReadToken(token) || !ParseFloatToken(token, component))
				return false;
			value.push_back(component);
		}
		return parser.Consume(']');
	});
}

bool ParseColorArray(std::string_view text, std::vector<ColorValue>& values)
{
	return ParseArray<ColorValue>(text, values, [](TextParser& parser, ColorValue& value)
	{
		if (!parser.Consume('['))
			return false;
		for (size_t index = 0; index < value.size(); ++index)
		{
			if (index != 0 && !parser.Consume(','))
				return false;
			std::string_view token;
			int32_t component = 0;
			if (!parser.ReadToken(token) || !ParseIntToken(token, component) || component < 0 || component > 255)
				return false;
			value[index] = static_cast<uint8_t>(component);
		}
		return parser.Consume(']');
	});
}

template <typename Value, typename FormatValue>
std::string FormatArray(const std::vector<Value>& values, FormatValue formatValue)
{
	std::string result = "[";
	for (size_t index = 0; index < values.size(); ++index)
	{
		if (index != 0)
			result += ", ";
		result += formatValue(values[index]);
	}
	result += ']';
	return result;
}

std::string FormatIntArray(const std::vector<int32_t>& values)
{
	return FormatArray<int32_t>(values, [](int32_t value) { return FormatInt(value); });
}

std::string FormatFloatArray(const std::vector<float>& values)
{
	return FormatArray<float>(values, [](float value) { return FormatFloat(value); });
}

std::string FormatBoolArray(const std::vector<bool>& values)
{
	std::string result = "[";
	for (size_t index = 0; index < values.size(); ++index)
	{
		if (index != 0)
			result += ", ";
		result += values[index] ? "true" : "false";
	}
	result += ']';
	return result;
}

std::string FormatStringArray(const std::vector<std::string>& values)
{
	return FormatArray<std::string>(values, [](const std::string& value) { return EscapeString(value); });
}

std::string FormatBlobArray(const std::vector<ByteVector>& values)
{
	return FormatArray<ByteVector>(values, [](const ByteVector& value) { return EscapeString(FormatHex(value)); });
}

std::string FormatTupleArray(const std::vector<FloatTuple>& values)
{
	return FormatArray<FloatTuple>(values, [](const FloatTuple& value) { return FormatTuple(value, true); });
}

std::string FormatColorArray(const std::vector<ColorValue>& values)
{
	return FormatArray<ColorValue>(values, [](const ColorValue& value) { return FormatColor(value, true); });
}

size_t TupleSize(DmAttributeType_t type)
{
	const DmAttributeType_t valueType = ::IsArrayType(type) ? ArrayTypeToValueType(type) : type;
	return static_cast<size_t>(NumComponents(valueType));
}

bool IsValidType(DmAttributeType_t type)
{
	const uint8_t value = static_cast<uint8_t>(type);
	return value >= static_cast<uint8_t>(AT_ELEMENT) && value <= static_cast<uint8_t>(AT_VMATRIX_ARRAY);
}


bool ReadCount(Reader& reader, size_t minimumItemSize, int32_t& count, std::string& error, std::string_view context)
{
	if (!reader.ReadInt32(count))
	{
		error = "Truncated " + std::string(context) + " count.";
		return false;
	}
	if (count < 0)
	{
		error = "Negative " + std::string(context) + " count.";
		return false;
	}
	if (minimumItemSize != 0 && static_cast<size_t>(count) > reader.Remaining() / minimumItemSize)
	{
		error = std::string(context) + " count exceeds the remaining file size.";
		return false;
	}
	return true;
}

bool ReadSymbolIndex(Reader& reader, const std::vector<std::string>& symbols, std::string& value, std::string& error, std::string_view context)
{
	int32_t index = 0;
	if (!reader.ReadInt32(index))
	{
		error = "Truncated " + std::string(context) + " symbol index.";
		return false;
	}
	if (index < 0 || static_cast<size_t>(index) >= symbols.size())
	{
		error = "Invalid " + std::string(context) + " symbol index " + FormatInt(index) + ".";
		return false;
	}
	value = symbols[index];
	return true;
}

bool ReadBlob(Reader& reader, ByteVector& value, std::string& error, std::string_view context)
{
	int32_t size = 0;
	if (!reader.ReadInt32(size))
	{
		error = "Truncated " + std::string(context) + " byte length.";
		return false;
	}
	if (size < 0 || static_cast<size_t>(size) > reader.Remaining())
	{
		error = "Invalid " + std::string(context) + " byte length.";
		return false;
	}
	if (!reader.ReadBlock(static_cast<size_t>(size), value))
	{
		error = "Truncated " + std::string(context) + ".";
		return false;
	}
	return true;
}

bool ReadTuple(Reader& reader, size_t size, FloatTuple& values)
{
	if (size > reader.Remaining() / sizeof(float))
		return false;
	values.resize(size);
	for (float& value : values)
	{
		if (!reader.ReadFloat(value))
			return false;
	}
	return true;
}

bool ReadElementReference(Reader& reader, const std::vector<DmxElement>& elements, DmObjectId_t& id, std::string& error)
{
	int32_t index = 0;
	if (!reader.ReadInt32(index))
	{
		error = "Truncated element reference.";
		return false;
	}
	if (index == NullElementIndex)
	{
		id = {};
		return true;
	}
	if (index == ExternalElementIndex)
	{
		std::string text;
		if (!reader.ReadCString(text))
		{
			error = "Truncated external element GUID.";
			return false;
		}
		if (!ParseObjectId(text, id) || !IsUniqueIdValid(id))
		{
			error = "Invalid external element GUID '" + text + "'.";
			return false;
		}
		return true;
	}
	if (index < 0 || static_cast<size_t>(index) >= elements.size())
	{
		error = "Element reference index " + FormatInt(index) + " is outside the element dictionary.";
		return false;
	}
	id = elements[index].m_Id;
	return true;
}

bool ReadAttributeValue(Reader& reader, const std::vector<std::string>& symbols, const std::vector<DmxElement>& elements,
	DmxAttribute& attribute, std::string& error)
{
	int32_t integer = 0;
	float real = 0.0f;
	uint8_t byte = 0;
	FloatTuple tuple;
	ColorValue color{};
	ByteVector blob;

	switch (attribute.m_Type)
	{
	case AT_ELEMENT:
	{
		DmObjectId_t id;
		if (!ReadElementReference(reader, elements, id, error)) return false;
		if (IsUniqueIdValid(id)) attribute.m_ElementIds.push_back(id);
		return true;
	}
	case AT_INT:
	case AT_TIME:
		if (!reader.ReadInt32(integer)) { error = "Truncated integer attribute '" + attribute.m_Name + "'."; return false; }
		attribute.m_Value = FormatInt(integer);
		return true;
	case AT_FLOAT:
		if (!reader.ReadFloat(real)) { error = "Truncated float attribute '" + attribute.m_Name + "'."; return false; }
		attribute.m_Value = FormatFloat(real);
		return true;
	case AT_BOOL:
		if (!reader.ReadByte(byte)) { error = "Truncated bool attribute '" + attribute.m_Name + "'."; return false; }
		if (byte > 1) { error = "Bool attribute '" + attribute.m_Name + "' is not 0 or 1."; return false; }
		attribute.m_Value = byte ? "true" : "false";
		return true;
	case AT_STRING:
		return ReadSymbolIndex(reader, symbols, attribute.m_Value, error, "string attribute");
	case AT_VOID:
		if (!ReadBlob(reader, blob, error, "void attribute")) return false;
		attribute.m_Value = FormatHex(blob);
		return true;
	case AT_COLOR:
		if (!reader.ReadBytes(color.size(), color.data())) { error = "Truncated color attribute '" + attribute.m_Name + "'."; return false; }
		attribute.m_Value = FormatColor(color, false);
		return true;
	case AT_VECTOR2:
	case AT_VECTOR3:
	case AT_VECTOR4:
	case AT_QANGLE:
	case AT_QUATERNION:
	case AT_VMATRIX:
		if (!ReadTuple(reader, TupleSize(attribute.m_Type), tuple)) { error = "Truncated vector or matrix attribute '" + attribute.m_Name + "'."; return false; }
		attribute.m_Value = FormatTuple(tuple, false);
		return true;
	default:
		break;
	}

	int32_t count = 0;
	if (!ReadCount(reader, 0, count, error, "attribute array"))
		return false;
	attribute.m_ElementIds.clear();

	switch (attribute.m_Type)
	{
	case AT_ELEMENT_ARRAY:
		if (static_cast<size_t>(count) > reader.Remaining() / 4) { error = "Element array count exceeds the remaining file size."; return false; }
		attribute.m_ElementIds.reserve(count);
		for (int32_t index = 0; index < count; ++index)
		{
			DmObjectId_t id;
			if (!ReadElementReference(reader, elements, id, error)) return false;
			attribute.m_ElementIds.push_back(id);
		}
		return true;
	case AT_INT_ARRAY:
	case AT_TIME_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining() / 4) { error = "Integer array count exceeds the remaining file size."; return false; }
		std::vector<int32_t> values(count);
		for (int32_t& value : values) if (!reader.ReadInt32(value)) return false;
		attribute.m_Value = FormatIntArray(values);
		return true;
	}
	case AT_FLOAT_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining() / 4) { error = "Float array count exceeds the remaining file size."; return false; }
		std::vector<float> values(count);
		for (float& value : values) if (!reader.ReadFloat(value)) return false;
		attribute.m_Value = FormatFloatArray(values);
		return true;
	}
	case AT_BOOL_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining()) { error = "Bool array count exceeds the remaining file size."; return false; }
		std::vector<bool> values;
		values.reserve(count);
		for (int32_t index = 0; index < count; ++index)
		{
			if (!reader.ReadByte(byte) || byte > 1) { error = "Invalid or truncated bool array."; return false; }
			values.push_back(byte != 0);
		}
		attribute.m_Value = FormatBoolArray(values);
		return true;
	}
	case AT_STRING_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining()) { error = "String array count exceeds the remaining file size."; return false; }
		std::vector<std::string> values;
		values.reserve(count);
		for (int32_t index = 0; index < count; ++index)
		{
			std::string value;
			if (!reader.ReadCString(value)) { error = "Truncated string array item."; return false; }
			values.push_back(std::move(value));
		}
		attribute.m_Value = FormatStringArray(values);
		return true;
	}
	case AT_VOID_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining() / 4) { error = "Void array count exceeds the remaining file size."; return false; }
		std::vector<ByteVector> values;
		values.reserve(count);
		for (int32_t index = 0; index < count; ++index)
		{
			ByteVector value;
			if (!ReadBlob(reader, value, error, "void array item")) return false;
			values.push_back(std::move(value));
		}
		attribute.m_Value = FormatBlobArray(values);
		return true;
	}
	case AT_COLOR_ARRAY:
	{
		if (static_cast<size_t>(count) > reader.Remaining() / 4) { error = "Color array count exceeds the remaining file size."; return false; }
		std::vector<ColorValue> values(count);
		for (ColorValue& value : values) if (!reader.ReadBytes(value.size(), value.data())) return false;
		attribute.m_Value = FormatColorArray(values);
		return true;
	}
	case AT_VECTOR2_ARRAY:
	case AT_VECTOR3_ARRAY:
	case AT_VECTOR4_ARRAY:
	case AT_QANGLE_ARRAY:
	case AT_QUATERNION_ARRAY:
	case AT_VMATRIX_ARRAY:
	{
		const size_t tupleSize = TupleSize(attribute.m_Type);
		if (tupleSize > reader.Remaining() / 4 || static_cast<size_t>(count) > reader.Remaining() / (tupleSize * 4))
		{
			error = "Vector or matrix array count exceeds the remaining file size.";
			return false;
		}
		std::vector<FloatTuple> values;
		values.reserve(count);
		for (int32_t index = 0; index < count; ++index)
		{
			FloatTuple value;
			if (!ReadTuple(reader, tupleSize, value)) return false;
			values.push_back(std::move(value));
		}
		attribute.m_Value = FormatTupleArray(values);
		return true;
	}
	default:
		error = "Unsupported DMX attribute type " + FormatInt(static_cast<uint8_t>(attribute.m_Type)) + ".";
		return false;
	}
}

class SymbolTable
{
public:
	bool Add(std::string_view value, std::string& error)
	{
		if (ContainsNull(value))
		{
			error = "DMX symbols cannot contain embedded NUL bytes.";
			return false;
		}
		const std::string key(value);
		if (m_Indices.contains(key))
			return true;
		if (m_Values.size() >= static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
		{
			error = "The DMX symbol table exceeds the binary format limit.";
			return false;
		}
		const int32_t index = static_cast<int32_t>(m_Values.size());
		m_Values.push_back(key);
		m_Indices.emplace(m_Values.back(), index);
		return true;
	}

	int32_t Index(std::string_view value) const
	{
		const auto iterator = m_Indices.find(std::string(value));
		return iterator == m_Indices.end() ? -1 : iterator->second;
	}

	const std::vector<std::string>& Values() const
	{
		return m_Values;
	}

private:
	std::vector<std::string> m_Values;
	std::unordered_map<std::string, int32_t> m_Indices;
};

int32_t FindElementIndex(const std::vector<DmxElement>& elements, const DmObjectId_t& id)
{
	for (size_t index = 0; index < elements.size(); ++index)
	{
		if (elements[index].m_Id == id)
			return static_cast<int32_t>(index);
	}
	return -1;
}

bool WriteElementReference(Writer& writer, const std::vector<DmxElement>& elements, const DmObjectId_t& id, std::string& error)
{
	if (!IsUniqueIdValid(id))
	{
		writer.WriteInt32(NullElementIndex);
		return true;
	}
	const int32_t index = FindElementIndex(elements, id);
	if (index >= 0)
	{
		writer.WriteInt32(index);
		return true;
	}
	const std::string text = FormatObjectId(id);
	if (text.empty())
	{
		error = "Element reference contains an invalid GUID.";
		return false;
	}
	writer.WriteInt32(ExternalElementIndex);
	writer.WriteCString(text);
	return true;
}

bool WriteCount(Writer& writer, size_t count, std::string& error, std::string_view context)
{
	if (count > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
	{
		error = std::string(context) + " exceeds the binary DMX count limit.";
		return false;
	}
	writer.WriteInt32(static_cast<int32_t>(count));
	return true;
}

bool WriteBlob(Writer& writer, const ByteVector& value, std::string& error)
{
	if (!WriteCount(writer, value.size(), error, "Binary block"))
		return false;
	writer.WriteBytes(value.data(), value.size());
	return true;
}

bool WriteAttributeValue(Writer& writer, const SymbolTable& symbols, const std::vector<DmxElement>& elements,
	const DmxAttribute& attribute, std::string& error)
{
	int32_t integer = 0;
	float real = 0.0f;
	bool boolean = false;
	ByteVector blob;
	FloatTuple tuple;
	ColorValue color{};

	switch (attribute.m_Type)
	{
	case AT_ELEMENT:
		return WriteElementReference(writer, elements, attribute.m_ElementIds.empty() ? DmObjectId_t{} : attribute.m_ElementIds.front(), error);
	case AT_INT:
	case AT_TIME:
		ParseScalarInt(attribute.m_Value, integer);
		writer.WriteInt32(integer);
		return true;
	case AT_FLOAT:
		ParseScalarFloat(attribute.m_Value, real);
		writer.WriteFloat(real);
		return true;
	case AT_BOOL:
		ParseScalarBool(attribute.m_Value, boolean);
		writer.WriteByte(boolean ? 1 : 0);
		return true;
	case AT_STRING:
		writer.WriteInt32(symbols.Index(attribute.m_Value));
		return true;
	case AT_VOID:
		ParseHex(attribute.m_Value, blob);
		return WriteBlob(writer, blob, error);
	case AT_COLOR:
		ParseColor(attribute.m_Value, color, false);
		writer.WriteBytes(color.data(), color.size());
		return true;
	case AT_VECTOR2:
	case AT_VECTOR3:
	case AT_VECTOR4:
	case AT_QANGLE:
	case AT_QUATERNION:
	case AT_VMATRIX:
		ParseTuple(attribute.m_Value, TupleSize(attribute.m_Type), tuple, false);
		for (float value : tuple) writer.WriteFloat(value);
		return true;
	default:
		break;
	}

	if (attribute.m_Type == AT_ELEMENT_ARRAY)
	{
		if (!WriteCount(writer, attribute.m_ElementIds.size(), error, "Element array")) return false;
		for (const DmObjectId_t& id : attribute.m_ElementIds)
			if (!WriteElementReference(writer, elements, id, error)) return false;
		return true;
	}

	if (attribute.m_Type == AT_INT_ARRAY || attribute.m_Type == AT_TIME_ARRAY)
	{
		std::vector<int32_t> values;
		ParseIntArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "Integer array")) return false;
		for (int32_t value : values) writer.WriteInt32(value);
		return true;
	}
	if (attribute.m_Type == AT_FLOAT_ARRAY)
	{
		std::vector<float> values;
		ParseFloatArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "Float array")) return false;
		for (float value : values) writer.WriteFloat(value);
		return true;
	}
	if (attribute.m_Type == AT_BOOL_ARRAY)
	{
		std::vector<bool> values;
		ParseBoolArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "Bool array")) return false;
		for (bool value : values) writer.WriteByte(value ? 1 : 0);
		return true;
	}
	if (attribute.m_Type == AT_STRING_ARRAY)
	{
		std::vector<std::string> values;
		ParseStringArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "String array")) return false;
		for (const std::string& value : values) writer.WriteCString(value);
		return true;
	}
	if (attribute.m_Type == AT_VOID_ARRAY)
	{
		std::vector<ByteVector> values;
		ParseBlobArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "Void array")) return false;
		for (const ByteVector& value : values) if (!WriteBlob(writer, value, error)) return false;
		return true;
	}
	if (attribute.m_Type == AT_COLOR_ARRAY)
	{
		std::vector<ColorValue> values;
		ParseColorArray(attribute.m_Value, values);
		if (!WriteCount(writer, values.size(), error, "Color array")) return false;
		for (const ColorValue& value : values) writer.WriteBytes(value.data(), value.size());
		return true;
	}
	if (TupleSize(attribute.m_Type) != 0 && IsArrayType(attribute.m_Type))
	{
		std::vector<FloatTuple> values;
		ParseTupleArray(attribute.m_Value, TupleSize(attribute.m_Type), values);
		if (!WriteCount(writer, values.size(), error, "Vector or matrix array")) return false;
		for (const FloatTuple& value : values)
			for (float component : value) writer.WriteFloat(component);
		return true;
	}

	error = "Unsupported DMX attribute type " + FormatInt(static_cast<uint8_t>(attribute.m_Type)) + ".";
	return false;
}

bool GatherSymbols(const std::vector<DmxElement>& elements, SymbolTable& symbols, std::string& error)
{
	for (const DmxElement& element : elements)
	{
		if (!symbols.Add(element.m_Type, error) || !symbols.Add(element.m_Name, error))
			return false;
		for (const DmxAttribute& attribute : element.m_Attributes)
		{
			if (!symbols.Add(attribute.m_Name, error))
				return false;
			if (attribute.m_Type == AT_STRING)
			{
				if (!symbols.Add(attribute.m_Value, error)) return false;
			}
		}
	}
	return true;
}

bool ReadFile(const std::filesystem::path& path, ByteVector& bytes, std::string& error)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		error = "Could not open '" + path.string() + "' for reading.";
		return false;
	}
	const std::streamoff length = stream.tellg();
	if (length < 0 || static_cast<uintmax_t>(length) > (std::numeric_limits<size_t>::max)())
	{
		error = "Could not determine a valid size for '" + path.string() + "'.";
		return false;
	}
	bytes.resize(static_cast<size_t>(length));
	stream.seekg(0, std::ios::beg);
	if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
	{
		error = "Could not read all bytes from '" + path.string() + "'.";
		return false;
	}
	return true;
}

bool WriteFile(const std::filesystem::path& path, const ByteVector& bytes, std::string& error)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
	{
		error = "Could not open '" + path.string() + "' for writing.";
		return false;
	}
	if (!bytes.empty())
		stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!stream)
	{
		error = "Could not write all bytes to '" + path.string() + "'.";
		return false;
	}
	return true;
}

} // namespace DmxBinary

std::string FormatObjectId(const DmObjectId_t& id)
{
	char text[37]{};
	UniqueIdToString(id, text, sizeof(text));
	return text;
}

bool ParseObjectId(std::string_view text, DmObjectId_t& id)
{
	return UniqueIdFromString(&id, text.data(), static_cast<int>(text.size()));
}

bool DmxAttribute::IsElementReference() const
{
	return m_Type == AT_ELEMENT || m_Type == AT_ELEMENT_ARRAY;
}

bool DmxAttribute::IsArray() const
{
	return ::IsArrayType(m_Type);
}

DmxAttribute* DmxElement::FindAttribute(std::string_view name)
{
	const auto iterator = std::find_if(m_Attributes.begin(), m_Attributes.end(), [name](const DmxAttribute& attribute)
	{
		return attribute.m_Name == name;
	});
	return iterator == m_Attributes.end() ? nullptr : &*iterator;
}

const DmxAttribute* DmxElement::FindAttribute(std::string_view name) const
{
	const auto iterator = std::find_if(m_Attributes.begin(), m_Attributes.end(), [name](const DmxAttribute& attribute)
	{
		return attribute.m_Name == name;
	});
	return iterator == m_Attributes.end() ? nullptr : &*iterator;
}

ParticleDocument ParticleDocument::CreateEmpty()
{
	return {};
}

bool ParticleDocument::Load(const std::filesystem::path& path, std::string& error)
{
	using namespace DmxBinary;
	error.clear();
	ByteVector bytes;
	if (!ReadFile(path, bytes, error))
		return false;
	Reader reader(bytes);
	std::string header;
	if (!reader.ReadCString(header))
	{
		error = "The DMX header is missing its NUL terminator.";
		return false;
	}
	if (header != Header)
	{
		error = "Unsupported DMX header. Expected binary 5 / pcf 2.";
		return false;
	}

	int32_t symbolCount = 0;
	if (!ReadCount(reader, 1, symbolCount, error, "symbol"))
		return false;
	std::vector<std::string> symbols;
	symbols.reserve(symbolCount);
	for (int32_t index = 0; index < symbolCount; ++index)
	{
		std::string symbol;
		if (!reader.ReadCString(symbol))
		{
			error = "Truncated symbol table entry " + FormatInt(index) + ".";
			return false;
		}
		symbols.push_back(std::move(symbol));
	}

	int32_t elementCount = 0;
	if (!ReadCount(reader, ElementDictionarySize, elementCount, error, "element"))
		return false;
	if (elementCount == 0)
	{
		error = "A binary PCF must contain a root element.";
		return false;
	}
	std::vector<DmxElement> loadedElements(static_cast<size_t>(elementCount));
	for (int32_t index = 0; index < elementCount; ++index)
	{
		DmxElement& element = loadedElements[index];
		if (!ReadSymbolIndex(reader, symbols, element.m_Type, error, "element type") ||
			!ReadSymbolIndex(reader, symbols, element.m_Name, error, "element name"))
			return false;
		if (!reader.ReadBytes(sizeof(element.m_Id.m_Value), element.m_Id.m_Value))
		{
			error = "Truncated GUID for element " + FormatInt(index) + ".";
			return false;
		}
		if (!IsUniqueIdValid(element.m_Id))
		{
			error = "Element " + FormatInt(index) + " has a null GUID.";
			return false;
		}
		for (int32_t previous = 0; previous < index; ++previous)
		{
			if (loadedElements[previous].m_Id == element.m_Id)
			{
				error = "Elements " + FormatInt(previous) + " and " + FormatInt(index) + " have the same GUID.";
				return false;
			}
		}
	}

	for (int32_t elementIndex = 0; elementIndex < elementCount; ++elementIndex)
	{
		DmxElement& element = loadedElements[elementIndex];
		int32_t attributeCount = 0;
		if (!ReadCount(reader, AttributeHeaderSize, attributeCount, error, "attribute"))
			return false;
		element.m_Attributes.reserve(attributeCount);
		for (int32_t attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex)
		{
			DmxAttribute attribute;
			if (!ReadSymbolIndex(reader, symbols, attribute.m_Name, error, "attribute name"))
				return false;
			uint8_t type = 0;
			if (!reader.ReadByte(type))
			{
				error = "Truncated type for attribute '" + attribute.m_Name + "'.";
				return false;
			}
			attribute.m_Type = static_cast<DmAttributeType_t>(type);
			if (!IsValidType(attribute.m_Type))
			{
				error = "Attribute '" + attribute.m_Name + "' has unsupported type " + FormatInt(type) + ".";
				return false;
			}
			if (!ReadAttributeValue(reader, symbols, loadedElements, attribute, error))
				return false;
			element.m_Attributes.push_back(std::move(attribute));
		}
	}
	if (!reader.Finished())
	{
		error = "Unexpected trailing bytes after the final DMX attribute.";
		return false;
	}
	m_Elements = std::move(loadedElements);
	return true;
}

bool ParticleDocument::Save(const std::filesystem::path& path, std::string& error) const
{
	using namespace DmxBinary;
	error.clear();
	ParticleDocument canonicalDocument = *this;
	if (!canonicalDocument.ValidateAndCanonicalize(error))
		return false;
	const std::vector<DmxElement>& elements = canonicalDocument.m_Elements;

	SymbolTable symbols;
	if (!GatherSymbols(elements, symbols, error))
		return false;
	Writer writer;
	writer.WriteCString(Header);
	if (!WriteCount(writer, symbols.Values().size(), error, "Symbol table"))
		return false;
	for (const std::string& symbol : symbols.Values())
		writer.WriteCString(symbol);

	if (!WriteCount(writer, elements.size(), error, "Element dictionary"))
		return false;
	for (const DmxElement& element : elements)
	{
		writer.WriteInt32(symbols.Index(element.m_Type));
		writer.WriteInt32(symbols.Index(element.m_Name));
		writer.WriteBytes(element.m_Id.m_Value, sizeof(element.m_Id.m_Value));
	}
	for (const DmxElement& element : elements)
	{
		if (!WriteCount(writer, element.m_Attributes.size(), error, "Attribute list"))
			return false;
		for (const DmxAttribute& attribute : element.m_Attributes)
		{
			writer.WriteInt32(symbols.Index(attribute.m_Name));
			writer.WriteByte(static_cast<uint8_t>(attribute.m_Type));
			if (!WriteAttributeValue(writer, symbols, elements, attribute, error))
			{
				error = "Attribute '" + attribute.m_Name + "': " + error;
				return false;
			}
		}
	}
	return WriteFile(path, writer.Bytes(), error);
}

std::vector<DmxElement>& ParticleDocument::Elements()
{
	return m_Elements;
}

const std::vector<DmxElement>& ParticleDocument::Elements() const
{
	return m_Elements;
}

DmxElement* ParticleDocument::Root()
{
	return m_Elements.empty() ? nullptr : &m_Elements.front();
}

const DmxElement* ParticleDocument::Root() const
{
	return m_Elements.empty() ? nullptr : &m_Elements.front();
}

DmxElement* ParticleDocument::FindElement(const DmObjectId_t& id)
{
	const auto iterator = std::find_if(m_Elements.begin(), m_Elements.end(), [&id](const DmxElement& element)
	{
		return element.m_Id == id;
	});
	return iterator == m_Elements.end() ? nullptr : &*iterator;
}

const DmxElement* ParticleDocument::FindElement(const DmObjectId_t& id) const
{
	const auto iterator = std::find_if(m_Elements.begin(), m_Elements.end(), [&id](const DmxElement& element)
	{
		return element.m_Id == id;
	});
	return iterator == m_Elements.end() ? nullptr : &*iterator;
}

DmxElement& ParticleDocument::CreateElement(std::string type, std::string name)
{
	DmxElement element;
	CreateUniqueId(&element.m_Id);
	element.m_Type = std::move(type);
	element.m_Name = std::move(name);
	m_Elements.push_back(std::move(element));
	return m_Elements.back();
}

bool ParticleDocument::RemoveElement(const DmObjectId_t& id)
{
	const auto iterator = std::find_if(m_Elements.begin(), m_Elements.end(), [&id](const DmxElement& element)
	{
		return element.m_Id == id;
	});
	if (iterator == m_Elements.end() || iterator == m_Elements.begin())
		return false;
	m_Elements.erase(iterator);
	for (DmxElement& element : m_Elements)
	{
		for (DmxAttribute& attribute : element.m_Attributes)
		{
			if (attribute.m_Type == AT_ELEMENT)
			{
				if (!attribute.m_ElementIds.empty() && attribute.m_ElementIds.front() == id)
					attribute.m_ElementIds.clear();
			}
			else if (attribute.m_Type == AT_ELEMENT_ARRAY)
			{
				std::erase(attribute.m_ElementIds, id);
			}
		}
	}
	return true;
}

bool ParticleDocument::CanonicalizeAttribute(DmxAttribute& attribute, std::string& error)
{
	using namespace DmxBinary;
	error.clear();
	if (!IsValidType(attribute.m_Type))
	{
		error = "Unsupported DMX attribute type " + FormatInt(static_cast<uint8_t>(attribute.m_Type)) + ".";
		return false;
	}
	if (ContainsNull(attribute.m_Name))
	{
		error = "Attribute names cannot contain embedded NUL bytes.";
		return false;
	}
	if (attribute.IsElementReference())
	{
		if (attribute.m_Type == AT_ELEMENT && attribute.m_ElementIds.size() > 1)
		{
			error = "A scalar element attribute can contain at most one element ID.";
			return false;
		}
		attribute.m_Value.clear();
		return true;
	}
	if (!attribute.m_ElementIds.empty())
	{
		error = "A non-element attribute cannot contain element IDs.";
		return false;
	}

	int32_t integer = 0;
	float real = 0.0f;
	bool boolean = false;
	ByteVector blob;
	FloatTuple tuple;
	ColorValue color{};
	std::vector<int32_t> integers;
	std::vector<float> reals;
	std::vector<bool> booleans;
	std::vector<std::string> strings;
	std::vector<ByteVector> blobs;
	std::vector<FloatTuple> tuples;
	std::vector<ColorValue> colors;

	switch (attribute.m_Type)
	{
	case AT_INT:
	case AT_TIME:
		if (!ParseScalarInt(attribute.m_Value, integer)) { error = "Expected a signed 32-bit integer."; return false; }
		attribute.m_Value = FormatInt(integer);
		return true;
	case AT_FLOAT:
		if (!ParseScalarFloat(attribute.m_Value, real)) { error = "Expected a 32-bit floating-point value."; return false; }
		attribute.m_Value = FormatFloat(real);
		return true;
	case AT_BOOL:
		if (!ParseScalarBool(attribute.m_Value, boolean)) { error = "Expected true, false, 1, or 0."; return false; }
		attribute.m_Value = boolean ? "true" : "false";
		return true;
	case AT_STRING:
		if (ContainsNull(attribute.m_Value)) { error = "Strings cannot contain embedded NUL bytes."; return false; }
		return true;
	case AT_VOID:
		if (!ParseHex(attribute.m_Value, blob)) { error = "Expected an even number of hexadecimal digits."; return false; }
		attribute.m_Value = FormatHex(blob);
		return true;
	case AT_COLOR:
		if (!ParseColor(attribute.m_Value, color, false)) { error = "Expected four color bytes in the range 0 to 255."; return false; }
		attribute.m_Value = FormatColor(color, false);
		return true;
	case AT_VECTOR2:
	case AT_VECTOR3:
	case AT_VECTOR4:
	case AT_QANGLE:
	case AT_QUATERNION:
	case AT_VMATRIX:
		if (!ParseTuple(attribute.m_Value, TupleSize(attribute.m_Type), tuple, false)) { error = "Expected the required number of floating-point components."; return false; }
		attribute.m_Value = FormatTuple(tuple, false);
		return true;
	case AT_INT_ARRAY:
	case AT_TIME_ARRAY:
		if (!ParseIntArray(attribute.m_Value, integers)) { error = "Expected a JSON-like array of signed 32-bit integers."; return false; }
		attribute.m_Value = FormatIntArray(integers);
		return true;
	case AT_FLOAT_ARRAY:
		if (!ParseFloatArray(attribute.m_Value, reals)) { error = "Expected a JSON-like array of floating-point values."; return false; }
		attribute.m_Value = FormatFloatArray(reals);
		return true;
	case AT_BOOL_ARRAY:
		if (!ParseBoolArray(attribute.m_Value, booleans)) { error = "Expected a JSON-like array of bool values."; return false; }
		attribute.m_Value = FormatBoolArray(booleans);
		return true;
	case AT_STRING_ARRAY:
		if (!ParseStringArray(attribute.m_Value, strings)) { error = "Expected a JSON-like array of quoted strings."; return false; }
		attribute.m_Value = FormatStringArray(strings);
		return true;
	case AT_VOID_ARRAY:
		if (!ParseBlobArray(attribute.m_Value, blobs)) { error = "Expected a JSON-like array of quoted hexadecimal blocks."; return false; }
		attribute.m_Value = FormatBlobArray(blobs);
		return true;
	case AT_COLOR_ARRAY:
		if (!ParseColorArray(attribute.m_Value, colors)) { error = "Expected an array of four-byte color arrays."; return false; }
		attribute.m_Value = FormatColorArray(colors);
		return true;
	case AT_VECTOR2_ARRAY:
	case AT_VECTOR3_ARRAY:
	case AT_VECTOR4_ARRAY:
	case AT_QANGLE_ARRAY:
	case AT_QUATERNION_ARRAY:
	case AT_VMATRIX_ARRAY:
		if (!ParseTupleArray(attribute.m_Value, TupleSize(attribute.m_Type), tuples)) { error = "Expected an array of fixed-size floating-point arrays."; return false; }
		attribute.m_Value = FormatTupleArray(tuples);
		return true;
	default:
		error = "Unsupported DMX attribute type.";
		return false;
	}
}

bool ParticleDocument::ValidateAndCanonicalize(std::string& error)
{
	using namespace DmxBinary;
	error.clear();
	if (m_Elements.empty())
	{
		error = "The document has no root element.";
		return false;
	}
	if (m_Elements.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
	{
		error = "The document has too many elements for binary DMX.";
		return false;
	}
	for (size_t elementIndex = 0; elementIndex < m_Elements.size(); ++elementIndex)
	{
		DmxElement& element = m_Elements[elementIndex];
		if (!IsUniqueIdValid(element.m_Id))
		{
			error = "Element " + std::to_string(elementIndex) + " has a null GUID.";
			return false;
		}
		if (ContainsNull(element.m_Type) || ContainsNull(element.m_Name))
		{
			error = "Element " + std::to_string(elementIndex) + " has a type or name containing an embedded NUL byte.";
			return false;
		}
		for (size_t previous = 0; previous < elementIndex; ++previous)
		{
			if (m_Elements[previous].m_Id == element.m_Id)
			{
				error = "Elements " + std::to_string(previous) + " and " + std::to_string(elementIndex) + " have the same GUID.";
				return false;
			}
		}
		if (element.m_Attributes.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
		{
			error = "Element '" + element.m_Name + "' has too many attributes.";
			return false;
		}
		for (DmxAttribute& attribute : element.m_Attributes)
		{
			std::string attributeError;
			if (!CanonicalizeAttribute(attribute, attributeError))
			{
				error = "Element '" + element.m_Name + "', attribute '" + attribute.m_Name + "': " + attributeError;
				return false;
			}
			if (attribute.m_ElementIds.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
			{
				error = "Element attribute '" + attribute.m_Name + "' contains too many references.";
				return false;
			}
		}
	}
	return true;
}

} // namespace ParticleTools
