# IniParserHpp
// 合并版 INI文件解析库 
// 合并了 inih (ini.h + ini.c) 和 IniParser (IniParser.h + IniParser.cpp) 
// 基于 BSD-3-Clause 许可证

//使用方法
#include "IniParser.hpp"

IniParser m_iniParams;
m_iniParams.InitializeFromFile("/config/system_config.ini");
std::softName = m_iniParams.GetString("about", "name");
