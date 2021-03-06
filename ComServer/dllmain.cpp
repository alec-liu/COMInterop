#include "stdafx.h"
#include "ComServer.h"

#include <memory>
#include <type_traits>

using namespace ATL;

namespace
{
    const wchar_t EntryKeyFmt[] = L"SOFTWARE\\Classes\\CLSID\\%s";

    struct OleStr : public std::unique_ptr<std::remove_pointer<LPOLESTR>::type, decltype(&::CoTaskMemFree)>
    {
        OleStr(_In_ LPOLESTR raw)
            : std::unique_ptr<std::remove_pointer<LPOLESTR>::type, decltype(&::CoTaskMemFree)>(raw, ::CoTaskMemFree)
        { }
    };

    HRESULT RemoveClsid(_In_ REFCLSID clsid)
    {
        HRESULT hr;

        LPOLESTR clsidAsStrRaw;
        RETURN_IF_FAILED(::StringFromCLSID(clsid, &clsidAsStrRaw));

        OleStr clsidAsStr{ clsidAsStrRaw };
        CStringW regKeyPath;
        try
        {
            regKeyPath.AppendFormat(EntryKeyFmt, clsidAsStr.get());
        }
        catch (CAtlException &e)
        {
            return e.m_hr;
        }

        CRegKey regKey{ HKEY_LOCAL_MACHINE };
        regKey.RecurseDeleteKey(regKeyPath.GetString());

        return S_OK;
    }

    HRESULT RegisterClsid(_In_ REFCLSID clsid, _In_opt_z_ wchar_t *threadingModel)
    {
        HRESULT hr;

        // Remove the CLSID in case it exists and has undesirable settings
        RETURN_IF_FAILED(RemoveClsid(clsid));

        LPOLESTR clsidAsStrRaw;
        RETURN_IF_FAILED(::StringFromCLSID(clsid, &clsidAsStrRaw));

        OleStr clsidAsStr{clsidAsStrRaw };
        CStringW regKeyClsidPath;
        CStringW regKeyServerPath;
        try
        {
            regKeyClsidPath.AppendFormat(EntryKeyFmt, clsidAsStr.get());
            regKeyServerPath.AppendFormat(L"%s\\InProcServer32", regKeyClsidPath.GetString());
        }
        catch (CAtlException &e)
        {
            return e.m_hr;
        }

        CRegKey regKey;
        LSTATUS status = regKey.Create(HKEY_LOCAL_MACHINE, regKeyClsidPath.GetString());
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        status = regKey.Create(HKEY_LOCAL_MACHINE, regKeyServerPath.GetString());
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        wchar_t fullPath[MAX_PATH + 1];
        ::GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), fullPath, ARRAYSIZE(fullPath));

        // The default value for the key is the path to the DLL
        status = regKey.SetStringValue(nullptr, fullPath);
        if (status != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(status);

        // Set the threading model if provided
        if (threadingModel != nullptr)
        {
            status = regKey.SetStringValue(L"ThreadingModel", threadingModel);
            if (status != ERROR_SUCCESS)
                return HRESULT_FROM_WIN32(status);
        }

        return S_OK;
    }
}

STDAPI DllRegisterServer(void)
{
    HRESULT hr;

    RETURN_IF_FAILED(RegisterClsid(__uuidof(Server), L"Both"));

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    HRESULT hr;

    RETURN_IF_FAILED(RemoveClsid(__uuidof(Server)));

    return S_OK;
}

STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Out_ LPVOID FAR* ppv)
{
    if (rclsid == __uuidof(Server))
        return ClassFactory<Server>::Create(riid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}
