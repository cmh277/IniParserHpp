// ini.hpp - 合并版 INI 解析库
// 合并了 inih (ini.h + ini.c) 和 IniParser (IniParser.h + IniParser.cpp)
// 基于 BSD-3-Clause 许可证

#ifndef INI_PARSER_HPP
#define INI_PARSER_HPP

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <functional>
#include <locale>

// ==================== 原始 inih 部分 ====================
#ifdef __cplusplus
extern "C" {
#endif

#define INI_STOP_ON_FIRST_ERROR 0
#define INI_HANDLER_LINENO 0
#define INI_ALLOW_MULTILINE 1
#define INI_ALLOW_BOM 1
#define INI_ALLOW_INLINE_COMMENTS 1
#define INI_INLINE_COMMENT_PREFIXES ";"
#define INI_USE_STACK 1
#define INI_MAX_LINE 500
#define INI_ALLOW_REALLOC 0
#define INI_INITIAL_ALLOC 200
#define INI_CALL_HANDLER_ON_NEW_SECTION 0
#define INI_ALLOW_NO_VALUE 0
#define INI_CUSTOM_ALLOCATOR 0
#define INI_START_COMMENT_PREFIXES ";#"

	//setlocale(LC_ALL, "chs");    // Windows 中文简体

	typedef int (*ini_handler)(void* user, const char* section,
		const char* name, const char* value);
	typedef char* (*ini_reader)(char* str, int num, void* stream);

	static char* rstrip(char* s) {
		char* p = s + strlen(s);
		while (p > s && isspace((unsigned char)(*--p)))
			*p = '\0';
		return s;
	}

	static char* lskip(const char* s) {
		while (*s && isspace((unsigned char)(*s)))
			s++;
		return (char*)s;
	}

	static char* find_chars_or_comment(const char* s, const char* chars) {
		int was_space = 0;
		while (*s && (!chars || !strchr(chars, *s)) &&
			!(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s))) {
			was_space = isspace((unsigned char)(*s));
			s++;
		}
		return (char*)s;
	}

	static char* strncpy0(char* dest, const char* src, size_t size) {
		size_t i;
		for (i = 0; i < size - 1 && src[i]; i++)
			dest[i] = src[i];
		dest[i] = '\0';
		return dest;
	}

	typedef struct {
		const char* ptr;
		size_t num_left;
	} ini_parse_string_ctx;

	static char* ini_reader_string(char* str, int num, void* stream) {
		ini_parse_string_ctx* ctx = (ini_parse_string_ctx*)stream;
		const char* ctx_ptr = ctx->ptr;
		size_t ctx_num_left = ctx->num_left;
		char* strp = str;
		char c;

		if (ctx_num_left == 0 || num < 2)
			return NULL;

		while (num > 1 && ctx_num_left != 0) {
			c = *ctx_ptr++;
			ctx_num_left--;
			*strp++ = c;
			if (c == '\n')
				break;
			num--;
		}

		*strp = '\0';
		ctx->ptr = ctx_ptr;
		ctx->num_left = ctx_num_left;
		return str;
	}

	static int ini_parse_stream(ini_reader reader, void* stream, ini_handler handler,
		void* user) {
		char line[INI_MAX_LINE];
		char section[INI_MAX_LINE] = "";
		char prev_name[INI_MAX_LINE] = "";
		char* start;
		char* end;
		char* name;
		char* value;
		int lineno = 0;
		int error = 0;

		while (reader(line, INI_MAX_LINE, stream) != NULL) {
			lineno++;
			start = line;
			start = lskip(rstrip(start));

			if (strchr(INI_START_COMMENT_PREFIXES, *start)) {
			}
			else if (*prev_name && *start && start > line) {
				end = find_chars_or_comment(start, NULL);
				if (*end)
					*end = '\0';
				rstrip(start);
				if (!handler(user, section, prev_name, start) && !error)
					error = lineno;
			}
			else if (*start == '[') {
				end = find_chars_or_comment(start + 1, "]");
				if (*end == ']') {
					*end = '\0';
					strncpy0(section, start + 1, sizeof(section));
					*prev_name = '\0';
				}
				else if (!error) {
					error = lineno;
				}
			}
			else if (*start) {
				end = find_chars_or_comment(start, "=:");
				if (*end == '=' || *end == ':') {
					*end = '\0';
					name = rstrip(start);
					value = end + 1;
					end = find_chars_or_comment(value, NULL);
					if (*end)
						*end = '\0';
					value = lskip(value);
					rstrip(value);
					strncpy0(prev_name, name, sizeof(prev_name));
					if (!handler(user, section, name, value) && !error)
						error = lineno;
				}
				else if (!error) {
#if INI_ALLOW_NO_VALUE
					* end = '\0';
					name = rstrip(start);
					if (!handler(user, section, name, NULL) && !error)
						error = lineno;
#else
					error = lineno;
#endif
				}
			}

			if (error && INI_STOP_ON_FIRST_ERROR)
				break;
		}

		return error;
	}

	static int ini_parse_file(FILE* file, ini_handler handler, void* user) {
		return ini_parse_stream((ini_reader)fgets, file, handler, user);
	}

	static int ini_parse(const char* filename, ini_handler handler, void* user) {
		FILE* file;
		int error;

		file = fopen(filename, "r");
		if (!file)
			return -1;
		error = ini_parse_file(file, handler, user);
		fclose(file);
		return error;
	}

	static int ini_parse_string(const char* string, ini_handler handler, void* user) {
		ini_parse_string_ctx ctx;
		ctx.ptr = string;
		ctx.num_left = strlen(string);
		return ini_parse_stream((ini_reader)ini_reader_string, &ctx, handler, user);
	}

#ifdef __cplusplus
} // extern "C"
#endif

// ==================== IniParser 部分 ====================
#include "LogHelper.h"
#if defined(WIN32) || defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define PATH_MAX MAX_PATH
#elif defined(linux) || defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

class IniParser 
{
private:
	int _error;
	std::map<std::string, std::string> _values;

	static std::string MakeKey(const std::string& section, const std::string& name) {
		std::string key = section + "=" + name;
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		return key;
	}

	static int ValueHandler(void* user, const char* section, const char* name,
		const char* value) {
		if (!name) return 1;
		IniParser* reader = static_cast<IniParser*>(user);
		std::string key = MakeKey(section, name);
		if (reader->_values[key].size() > 0)
			reader->_values[key] += "\n";
		reader->_values[key] += value ? value : "";
		return 1;
	}

public:

	IniParser() : _error(0) {}

	// 允许移动语义
	IniParser(IniParser&&) = default;
	IniParser& operator=(IniParser&&) = default;

	// 禁止拷贝
	IniParser(const IniParser&) = delete;
	IniParser& operator=(const IniParser&) = delete;

	// 初始化配置文件
	bool InitializeFromFile(const std::string& fileName) 
	{
		char path[PATH_MAX] = {};
		if (!_getcwd(path, sizeof(path))) {
			LOG_ERROR("Failed to get current directory");
			return false;
		}
		std::string tempPath(path);
		tempPath.append(fileName);

		_error = ini_parse(tempPath.c_str(), ValueHandler, this);
		LOG_INFO("INI file path is: {}", tempPath.c_str());
		if (_error) {
			LOG_ERROR("Failed to parse INI file: {}", tempPath.c_str());
		}
		else {
			LOG_INFO("Successfully parsed INI file: {}", tempPath.c_str());
		}
	}

	// 初始化配置字符串
	void InitializeFromString(const char* buffer, size_t buffer_size) {
		std::string content(buffer, buffer_size);
		_error = ini_parse_string(content.c_str(), ValueHandler, this);
	}

	int ParseError() const { return _error; }

	std::string Get(const std::string& section, const std::string& name,
		const std::string& default_value = "") const {
		std::string key = MakeKey(section, name);
		return _values.count(key) ? _values.find(key)->second : default_value;
	}

	std::string GetString(const std::string& section, const std::string& name,
		const std::string& default_value = "") const {
		const std::string str = Get(section, name, "");
		return (str.empty() ? default_value : str);
	}

	long GetInteger(const std::string& section, const std::string& name,
		long default_value = 0) const {
		std::string valstr = Get(section, name, "");
		const char* value = valstr.c_str();
		char* end;
		long n = strtol(value, &end, 0);
		return end > value ? n : default_value;
	}

	double GetReal(const std::string& section, const std::string& name,
		double default_value = 0.0) const {
		std::string valstr = Get(section, name, "");
		const char* value = valstr.c_str();
		char* end;
		double n = strtod(value, &end);
		return end > value ? n : default_value;
	}

	bool GetBoolean(const std::string& section, const std::string& name,
		bool default_value = false) const {
		std::string valstr = Get(section, name, "");
		std::transform(valstr.begin(), valstr.end(), valstr.begin(), ::tolower);
		if (valstr == "true" || valstr == "yes" || valstr == "on" || valstr == "1")
			return true;
		else if (valstr == "false" || valstr == "no" || valstr == "off" || valstr == "0")
			return false;
		return default_value;
	}

	bool HasSection(const std::string& section) const {
		const std::string key = MakeKey(section, "");
		auto pos = _values.lower_bound(key);
		if (pos == _values.end())
			return false;
		return pos->first.compare(0, key.length(), key) == 0;
	}

	bool HasValue(const std::string& section, const std::string& name) const {
		std::string key = MakeKey(section, name);
		return _values.count(key);
	}
};

#endif // INI_PARSER_HPP