#include "stdafx.h"
#include "utils.h"
#include <sstream>
#include <tuple>
#include <iomanip>

std::wstring GetLastErrorMessage(DWORD* eNum)
{
    TCHAR sysMsg[256];
    TCHAR* p;

    *eNum = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, *eNum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        sysMsg, 256, NULL);

    // Trim the end of the line and terminate it with a null
    p = sysMsg;
    while ((*p > 31) || (*p == 9))
        ++p;
    do { *p-- = 0; } while ((p >= sysMsg) &&
        ((*p == '.') || (*p < 33)));

    std::wstring msg(sysMsg);

    return msg;
}

void printError(TCHAR* msg)
{
    DWORD eNum;
    std::wstring sysMsg = GetLastErrorMessage(&eNum);
    // Display the message
    _tprintf(TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum, sysMsg.c_str());
}

std::string ConvertToString(const wchar_t* wstr)
{
    int Newlength = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wstr, -1, NULL, 0, NULL, NULL);
    std::string NewStr(Newlength, 0);
    int Newresult = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wstr, -1, &NewStr[0], Newlength, NULL, NULL);
    return NewStr;
}
std::wstring ConvertToWString(const char* str)
{
    int length = MultiByteToWideChar(CP_ACP,MB_COMPOSITE, str, -1, NULL, 0);
    std::wstring newstr(length, 0);
    MultiByteToWideChar(CP_ACP, MB_COMPOSITE, str, -1, &newstr[0], length);
    return newstr;
}
std::string GetHtmlResource(int id)
{
    HMODULE handle = GetModuleHandle(NULL);
    HRSRC resourceHandle = ::FindResource(handle, MAKEINTRESOURCE(id), RT_HTML);
    DWORD resourceHandleSize = ::SizeofResource(handle, resourceHandle);
    HGLOBAL dataHandle = ::LoadResource(handle, resourceHandle);
    void* data = ::LockResource(dataHandle);
    std::string str((char*)data, resourceHandleSize);
    return str;
}

template <typename Container, typename Fun>
void tuple_for_each(const Container& c, Fun fun)
{
    for (auto& e : c)
        fun(std::get<0>(e), std::get<1>(e), std::get<2>(e));
}

std::string createDurationString(std::chrono::milliseconds time)
{
    using namespace std::chrono;

    using T = std::tuple<milliseconds, int, const char *>;

    constexpr T formats[] = {
        T{ hours(1), 2, "" },
        T{ minutes(1), 2, ":" },
        T{ seconds(1), 2, ":" },
        T{ milliseconds(1), 3, "." }
    };

    std::ostringstream o;
    tuple_for_each(formats, [&time, &o](auto denominator, auto width, auto separator) {
        o << separator << std::setw(width) << std::setfill('0') << (time / denominator);
        time = time % denominator;
    });
    return o.str();
}