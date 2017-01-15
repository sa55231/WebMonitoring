#pragma once

#include <tchar.h>
#include <windows.h>
#include <chrono>
#include <string>

void printError(TCHAR* msg);
std::string ConvertToString(const wchar_t* wstr);
std::wstring ConvertToWString(const char* str);
std::string GetHtmlResource(int id);
std::string createDurationString(std::chrono::milliseconds time);
std::wstring GetLastErrorMessage(DWORD* eNum);