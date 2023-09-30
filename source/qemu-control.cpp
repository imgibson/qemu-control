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

#include <cstdint>
#include <type_traits>

template <std::size_t M>
class FormatBuffer final {
private:
    char m_buffer[M];

public:
    FormatBuffer() noexcept {
        clear();
    }

    template <typename... Types>
    FormatBuffer(const char* fmt, Types... args) noexcept {
        print(fmt, args...);
    }

    template <typename... Types>
    void print(const char* fmt, Types... args) noexcept {
        print(m_buffer, fmt, args...);
    }

    void clear() noexcept {
        m_buffer[0] = '\0';
    }

    template <std::size_t N, typename... Types>
    static void print(char (&buf)[N], const char* fmt, Types... args) noexcept {
        static_assert(N > 0);
        std::size_t i = 0;
        auto copyFromString = [&buf, &i](const char* str) noexcept -> void {
            do {
                buf[i++] = *str++;
            } while (*str != '\0' && i < (N - 1));
        };
        auto copyFromFormat = [&buf, &fmt, &i]() noexcept -> bool {
            do {
                if (*fmt == '%') {
                    ++fmt;
                    if (*fmt != '%') {
                        return true;
                    }
                }
                buf[i++] = *fmt++;
            } while (*fmt != '\0' && i < (N - 1));
            return false;
        };
        if (*fmt != '\0') {
            bool foundSpec = copyFromFormat();
            (
                [&]<typename T>(T value) noexcept -> void {
                    if (foundSpec != false && *fmt != '\0') {
                        char spec = *fmt++;
                        if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
                            if (spec == 's') {
                                copyFromString(value);
                            }
                        } else {
                            static_assert(std::is_same_v<T, void>);
                        }
                        if (*fmt != '\0' && i < (N - 1)) {
                            foundSpec = copyFromFormat();
                        }
                    }
                }(args),
                ...);
            if (*fmt != '\0') {
                if (foundSpec != false) {
                    buf[i++] = '%';
                }
                if (i < (N - 1)) {
                    copyFromString(fmt);
                }
            }
        }
        buf[i] = '\0';
    }

    const char* c_str() const noexcept {
        return m_buffer;
    }
};

template <std::size_t N, typename... Types>
void format(char (&buf)[N], const char* fmt, Types... args) noexcept {
    FormatBuffer<N>::print(buf, fmt, args...);
}

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
                    FormatBuffer<4096> buffer{ "%s %s %s %s %s %s %s", params->szBoot, params->szMachine, params->szDisplay, params->szClock, params->szTablet, params->szVirtual, params->szNetwork };
                    SHELLEXECUTEINFO sh{
                        .cbSize = sizeof(sh),
                        .fMask = SEE_MASK_NOCLOSEPROCESS,
                        .hwnd = nullptr,
                        .lpVerb = nullptr,
                        .lpFile = params->szCommand,
                        .lpParameters = buffer.c_str(),
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
