#include "stdafx.h"
#include "utils.h"
#include <sstream>
#include <tuple>
#include <iomanip>
#include <Accctrl.h>
#include <Aclapi.h>

void SetFilePermission(LPCTSTR FileName)
{
    PSID pEveryoneSID = NULL;
    PACL pACL = NULL;
    EXPLICIT_ACCESS ea[1];
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

    // Create a well-known SID for the Everyone group.
    AllocateAndInitializeSid(&SIDAuthWorld, 1,
        SECURITY_WORLD_RID,
        0, 0, 0, 0, 0, 0, 0,
        &pEveryoneSID);

    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will allow Everyone read access to the key.
    ZeroMemory(&ea, 1 * sizeof(EXPLICIT_ACCESS));
    ea[0].grfAccessPermissions = 0xFFFFFFFF;
    ea[0].grfAccessMode = GRANT_ACCESS;
    ea[0].grfInheritance = NO_INHERITANCE;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSID;

    // Create a new ACL that contains the new ACEs.
    SetEntriesInAcl(1, ea, NULL, &pACL);

    // Initialize a security descriptor.  
    PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
        SECURITY_DESCRIPTOR_MIN_LENGTH);

    InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);

    // Add the ACL to the security descriptor. 
    SetSecurityDescriptorDacl(pSD,
        TRUE,     // bDaclPresent flag   
        pACL,
        FALSE);   // not a default DACL 


                  //Change the security attributes
    SetFileSecurity(FileName, DACL_SECURITY_INFORMATION, pSD);

    if (pEveryoneSID)
        FreeSid(pEveryoneSID);
    if (pACL)
        LocalFree(pACL);
    if (pSD)
        LocalFree(pSD);
}

std::wstring GetLastErrorMessage(DWORD* eNum)
{
    *eNum = GetLastError();    
    return GetErrorMessage(*eNum);
}

std::wstring GetErrorMessage(DWORD eNum)
{
    TCHAR sysMsg[256] = { 0 };
    TCHAR* p;
    DWORD charNum = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, eNum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        sysMsg, 256, NULL);

    // Trim the end of the line and terminate it with a null
    p = sysMsg;
    while ((*p > 31) || (*p == 9))
        ++p;
    do { *p-- = 0; } while ((p >= sysMsg) &&
        ((*p == '.') || (*p < 33)));


    std::wstring msg;
    if (charNum > 0) {
        msg.append(sysMsg);
    }

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