/** 
 *
 * @author Anders Lind (96395432+imgibson@users.noreply.github.com)
 * @date 2023-05-01
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <tchar.h>
#include <shellapi.h>

#include "resource.h"

struct HandleFunctor {
    using Handle = HANDLE;
    void operator()(Handle h) { CloseHandle(h); }
};

template <typename Functor, typename Functor::Handle InvalidHandle>
class Scope {
public:
    Scope(typename Functor::Handle handle) : m_handle{ handle } {}
    ~Scope() { if (isValid()) Functor()(m_handle); }
    bool isValid() const { return m_handle != InvalidHandle; }
    operator bool() const { return isValid(); }
private:
    typename Functor::Handle m_handle;
};

using MutexScope = Scope<HandleFunctor, nullptr>;

class Params {
public:
    TCHAR szCommand[4096];
    TCHAR szArguments[4096];
    TCHAR szStartupPath[4096];

    static Params* create(Params& self, LPCTSTR lpFilename, LPCTSTR lpSection) {
        const struct { LPCTSTR lpName; LPTSTR lpBuffer; DWORD nSize; } kEntries[] = {
            { _T("Command"), self.szCommand, ARRAYSIZE(self.szCommand) },
            { _T("Arguments"), self.szArguments, ARRAYSIZE(self.szArguments) },
            { _T("StartupPath"), self.szStartupPath, ARRAYSIZE(self.szStartupPath) }
        };
        for (const auto& e : kEntries) {
            DWORD nLength = GetPrivateProfileString(lpSection, e.lpName, _T(""), e.lpBuffer, e.nSize, lpFilename);
            if (nLength >= (e.nSize - 1)) {
                return nullptr;
            }
        }
        return &self;
    }
};

namespace {
    constexpr LPCTSTR kMutexName = _T("qemu-control-c154b33e36c329488700dae19dae8ddf");
    constexpr LPCTSTR kSection = _T("QEMU");

    struct {
        Params params{};
    } inst;
}

int WINAPI _tWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInst, _In_ LPTSTR cmdParam, _In_ int cmdShow) {
    if (MutexScope mutex = CreateMutex(0, FALSE, kMutexName)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return 0;
        } else {
            if (Params* params = Params::create(inst.params, cmdParam, kSection)) {
                if (lstrcmp(params->szCommand, _T("")) != 0) {
                    SHELLEXECUTEINFO sh{
                        .cbSize = sizeof(sh),
                        .fMask = SEE_MASK_NOCLOSEPROCESS,
                        .hwnd = nullptr,
                        .lpVerb = nullptr,
                        .lpFile = params->szCommand,
                        .lpParameters = params->szArguments,
                        .lpDirectory = params->szStartupPath,
                        .nShow = SW_SHOWDEFAULT,
                        .hInstApp = nullptr,
                        .lpIDList = nullptr,
                        .lpClass = nullptr,
                        .hkeyClass = nullptr,
                        .dwHotKey = 0,
                        .hIcon = nullptr,
                        .hProcess = nullptr
                    };
                    if ((ShellExecuteEx(&sh) != FALSE) && (sh.hProcess != nullptr)) {
                        if (WaitForSingleObject(sh.hProcess, INFINITE) == WAIT_OBJECT_0) {
                            DWORD exitCode{};
                            if (GetExitCodeProcess(sh.hProcess, &exitCode) != FALSE) {
                                return exitCode;
                            }
                        }
                    }
                }
            }
        }
    }
    return -1;
}
