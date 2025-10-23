/**
 *
 * @author Anders Lind (96395432+imgibson@users.noreply.github.com)
 * @date 2023-05-01
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <shellapi.h>

#include "resource.h"

#include <cstdlib>
#include <type_traits>

template <SIZE_T M>
class FormatBuffer final {
private:
    TCHAR m_buffer[M];

public:
    template <typename... Types>
    FormatBuffer(LPCTSTR fmt, Types... args) noexcept {
        print(fmt, args...);
    }

    template <typename... Types>
    void print(LPCTSTR fmt, Types... args) noexcept {
        print(m_buffer, fmt, args...);
    }

    template <SIZE_T N, typename... Types> requires (N > 0)
    static void print(TCHAR (&buf)[N], LPCTSTR fmt, Types... args) noexcept {
        SIZE_T i = 0;
        [[maybe_unused]] const auto copyFromString = [&buf, &i](LPCTSTR str) noexcept -> void {
            do {
                buf[i++] = *str++;
            } while (*str != _T('\0') && i < N - 1);
        };
        const auto copyFromFormat = [&buf, &fmt, &i]() noexcept -> bool {
            do {
                if (*fmt == _T('%') && *++fmt != _T('%')) {
                    return true;
                }
                buf[i++] = *fmt++;
            } while (*fmt != _T('\0') && i < N - 1);
            return false;
        };
        (
            [&]<typename T>(T value) noexcept -> void {
            if (*fmt != _T('\0') && i < N - 1) {
                const bool foundSpec = copyFromFormat();
                if (foundSpec != false && *fmt != _T('\0')) {
                    const TCHAR spec = *fmt++;
                    if constexpr (std::is_same_v<T, LPCTSTR> || std::is_same_v<T, LPTSTR>) {
                        if (spec == _T('s')) {
                            copyFromString(value);
                        }
                    } else {
                        static_assert(std::is_same_v<T, void>);
                    }
                }
            }
        }(args),
            ...);
        while (*fmt != _T('\0') && i < N - 1) {
            const bool foundSpec = copyFromFormat();
            if (foundSpec != false && *fmt != _T('\0')) {
                buf[i++] = *fmt++;
            }
        }
        buf[i] = _T('\0');
    }

    LPCTSTR c_str() const noexcept {
        return m_buffer;
    }
};

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
    TCHAR szCommand[256];
    TCHAR szStartupPath[256];
    TCHAR szBoot[256];
    TCHAR szMachine[256];
    TCHAR szDisplay[256];
    TCHAR szClock[256];
    TCHAR szTablet[256];
    TCHAR szVirtual[256];
    TCHAR szNetwork[256];

    static Params* create(Params& self, LPCTSTR lpFilename, LPCTSTR lpSection) {
        const struct { LPCTSTR lpName; LPTSTR lpBuffer; DWORD nSize; } kEntries[] = {
            { _T("Command"), self.szCommand, ARRAYSIZE(self.szCommand) },
            { _T("StartupPath"), self.szStartupPath, ARRAYSIZE(self.szStartupPath) },
            { _T("Boot"), self.szBoot, ARRAYSIZE(self.szBoot) },
            { _T("Machine"), self.szMachine, ARRAYSIZE(self.szMachine) },
            { _T("Display"), self.szDisplay, ARRAYSIZE(self.szDisplay) },
            { _T("Clock"), self.szClock, ARRAYSIZE(self.szClock) },
            { _T("Tablet"), self.szTablet, ARRAYSIZE(self.szTablet) },
            { _T("Virtual"), self.szVirtual, ARRAYSIZE(self.szVirtual) },
            { _T("Network"), self.szNetwork, ARRAYSIZE(self.szNetwork) }
        };
        for (const auto& entry : kEntries) {
            DWORD nLength = GetPrivateProfileString(lpSection, entry.lpName, _T(""), entry.lpBuffer, entry.nSize, lpFilename);
            if (nLength >= (entry.nSize - 1)) {
                return nullptr;
            }
        }
        return lstrcmp(self.szCommand, _T("")) != 0 ? &self : nullptr;
    }
};

namespace {

constexpr LPCTSTR kMutexName = _T("qemu-control-c154b33e36c329488700dae19dae8ddf");
constexpr LPCTSTR kSection = _T("QEMU");
constexpr LPCTSTR kAppName = _T("QEMU Control");

struct {
    Params params{};
} app;

} // namespace

int WINAPI _tWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInst, _In_ LPTSTR cmdParam, _In_ int cmdShow) {
    if (cmdParam == nullptr || lstrcmp(cmdParam, _T("")) == 0) {
        MessageBox(nullptr, _T("No configuration file specified."), kAppName, MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
    if (MutexScope mutex = CreateMutex(0, FALSE, kMutexName)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return EXIT_SUCCESS;
        }
        if (![](LPCTSTR lpFilePath) noexcept -> bool {
            DWORD attrs = GetFileAttributes(lpFilePath);
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                DWORD error = GetLastError();
                return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? false : true;
            }
            return attrs & FILE_ATTRIBUTE_DIRECTORY ? false : true;
        }(cmdParam)) {
            FormatBuffer<1024> message{ _T("Configuration file does not exist: %s"), cmdParam };
            MessageBox(nullptr, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
        } else if (Params* params = Params::create(app.params, cmdParam, kSection)) {
            FormatBuffer<4096> arguments{ _T("%s %s %s %s %s %s %s"), params->szBoot, params->szMachine, params->szDisplay, params->szClock, params->szTablet, params->szVirtual, params->szNetwork };
            SHELLEXECUTEINFO sh{
                    .cbSize = sizeof(sh),
                    .fMask = SEE_MASK_NOCLOSEPROCESS,
                    .hwnd = nullptr,
                    .lpVerb = nullptr,
                    .lpFile = params->szCommand,
                    .lpParameters = arguments.c_str(),
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
            if (ShellExecuteEx(&sh) != FALSE && sh.hProcess != nullptr) {
                if (WaitForSingleObject(sh.hProcess, INFINITE) == WAIT_OBJECT_0) {
                    DWORD exitCode{};
                    if (GetExitCodeProcess(sh.hProcess, &exitCode) != FALSE && exitCode == 0) {
                        return EXIT_SUCCESS;
                    }
                }
            } else {
                FormatBuffer<1024> message{ _T("Failed to execute command: %s"), params->szCommand };
                MessageBox(nullptr, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
            }
        } else {
            FormatBuffer<1024> message{ _T("Failed to read configuration from file: %s"), cmdParam };
            MessageBox(nullptr, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
        }
    }
    return EXIT_FAILURE;
}
