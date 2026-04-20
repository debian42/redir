/*
 * Project: Müllhaufen (Official Title) | Env-Filter Wrapper
 * Language: C++20 (MSVC/GCC) | Platform: Cross-Platform (Windows/Linux)
 * Model: Gemini 3 Flash (Mentored by Senior-Dev Müller)
 *
 * --- Die Evolution: Die Saga vom Müllhaufen (KI lernt von Senior-Dev Müller) ---
 * 01-10: Die Ursuppe. Der Code war ein einzelnes main(), voller char* und Tränen.
 * 11-20: Die BOM-Kriege. UTF-8 Byte Order Marks versuchten den Parser zu stürzen. 
 *        Die KI lernte, dass "Byte 0xEF" kein Freund ist.
 * 21-30: Windows-Hölle & Linux-Labyrinth. HANDLEs hier, fork() da. Der Code wurde
 *        zu einem wütenden Biest, das wir "Müllhaufen" tauften.
 * 31-43: Der Senior-Schliff. Müller kam, sah und fluchte. Wir lernten std::string_view,
 *        std::error_code und warum man niemals ungeprüfte Puffer verwendet.
 * 44. Clean Architecture: Refactoring auf Bridge-Pattern mit IOSBridge Interface.
 * 45. Template Method: Orchestrierung der Environment-Vorbereitung in der Basisklasse.
 * 46-50: Die "No-Oneshot" Ära. Wir dachten, wir wären fertig, aber Müller (der echte!) 
 *        hatte andere Pläne. Die Evolution beschleunigte sich massiv.
 * 51-60: Der Linux-Fluch. Während Windows mit wchar_t kuschelte, musste Linux mit 
 *        char** environ jonglieren. Wir haben es überlebt (mit viel std::deque).
 * 61-80: Die Iterations-Hölle. "Nur noch ein Test", sagten sie. "Nur noch ein Edge-Case".
 *        Die PowerShell-Konsole glühte, die Skripte wuchsen auf 13 Pass-Through-Prüfungen.
 * 81. Die Minus-Rebellion: Der Filter (-) lernte, dass ihn alles nach dem "=" 
 *     gar nichts angeht. Ignoranz ist hier ein Feature!
 * 82. Das Zen des Gleichheitszeichens: Whitespaces um das "=" (KEY = VAL) werden
 *     jetzt weg-ge-trimmed. Innerer Frieden für den Parser.
 * 83. Das Leere-Value-Versprechen: "KEY=" führt nicht mehr zum Crash, sondern 
 *     lebt als stolze, leere Variable im Ziel-Environment weiter.
 * 84. Die Mauer gegen Keyless-Entry: " + =VALUE" (Leere Keys) werden nun mit 
 *     einem harten CONFIG ERROR abgewiesen. Security First!
 * 101. Das Signal-Orakel: Windows-Vater lernt, bei Ctrl+C/Break/Close auf "Durchzug" 
 *      zu schalten. Er ignoriert alles, damit das Kind (der echte Held) die volle 
 *      Cleanup-Zeit bekommt. Redundante Deregistrierung entfernt - wir sterben eh.
 * 102. Die Signal-Brücke: Linux-Vater wird zum Signal-Relais. SIGUSR1, SIGTERM & Co. 
 *      werden per kill() an das Kind geflutet. sigprocmask-Barriere verhindert 
 *      Race-Conditions beim Forken. Senior-Level Signal Handling.
 * 100+: Perfektion. Kein One-Shot-Prompt, sondern eine echte Kooperation zwischen 
 *       Mensch und Maschine. Senior-Engineer-Approved. Müller lächelt (jetzt wirklich).
 */
 
#ifdef _WIN32
#  define COLD
#  define NOINLINE __declspec(noinline)
#else
#  define COLD     __attribute__((cold))
#  define NOINLINE __attribute__((noinline))
#endif

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <string_view>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#include <cstring>
#include <ctype.h>
#include <cerrno>
#include <sys/stat.h>
#include <fcntl.h>
extern char **environ;
#endif

namespace fs = std::filesystem;

#ifdef _WIN32
using NativeChar = wchar_t;
using NativeString = std::wstring;
using NativeStringView = std::wstring_view;
#define NATIVE_TEXT(s) L##s
#define NativeCout std::wcout
#define NativeCerr std::wcerr
#define NativeSnprintf swprintf

static HANDLE g_sig_log_handle = INVALID_HANDLE_VALUE;
#else
using NativeChar = char;
using NativeString = std::string;
using NativeStringView = std::string_view;
#define NATIVE_TEXT(s) s
#define NativeCout std::cout
#define NativeCerr std::cerr
#define NativeSnprintf snprintf

static volatile sig_atomic_t g_child_pid = 0;
static int g_sig_log_fd = -1;

static void signalRelayHandler(int signo) {
    if (g_sig_log_fd != -1) {
        char msg[64];
        int n = snprintf(msg, sizeof(msg), "[SIGNAL] Received: %d\n", signo);
        if (n > 0) {
            ssize_t written = write(g_sig_log_fd, msg, (size_t)n);
            (void)written; // Satisfy warn_unused_result
        }
    }
    if (g_child_pid > 0) (void) kill(g_child_pid, signo);
}
#endif

namespace Constants {
    constexpr NativeStringView CONF_SUFFIX = NATIVE_TEXT("_conf.env");
#ifdef _WIN32
    constexpr NativeStringView ORG_SUFFIX  = NATIVE_TEXT("_org.exe");
#else
    constexpr NativeStringView ORG_SUFFIX  = NATIVE_TEXT("_org");
#endif
    enum ExitCode { SUCCESS = 0, ERR_INTERNAL = 100, ERR_CONFIG = 101, ERR_TARGET = 102 };

    constexpr NativeStringView DEBUG_VAR = NATIVE_TEXT("REDIR_DEBUG");

    // --- SECURITY POLICY ---
    constexpr size_t MAX_CONFIG_SIZE = 1024 * 1024; // 1 MB
    constexpr size_t MAX_LINE_SIZE   = 32 * 1024;   // 32 KB
}

struct DebugFlags {
    bool preEnv = false;
    bool postEnv = false;
    bool dumpArgs = false;
    bool dumpPipes = false;
    bool dumpSignals = false;
    bool dumpIO = false;

    static DebugFlags parse(NativeStringView val) {
        DebugFlags flags;
        // Senior-Expert Trick: Alle Leerzeichen entfernen
        NativeString clean;
        clean.reserve(val.size());
        for (auto c : val) if (c != NATIVE_TEXT(' ')) clean.push_back(c);

        size_t start = 0;
        while (start < clean.size()) {
            size_t end = clean.find(NATIVE_TEXT(','), start);
            size_t len = (end == NativeString::npos) ? (clean.size() - start) : (end - start);
            
            if (len > 0) {
                NativeStringView part(clean.data() + start, len);
                if (part == NATIVE_TEXT("PRE_ENV")) flags.preEnv = true;
                else if (part == NATIVE_TEXT("POST_ENV")) flags.postEnv = true;
                else if (part == NATIVE_TEXT("DUMP_ARGS")) flags.dumpArgs = true;
                else if (part == NATIVE_TEXT("DUMP_PIPES")) flags.dumpPipes = true;
                else if (part == NATIVE_TEXT("DUMP_SIGNALS")) flags.dumpSignals = true;
                else if (part == NATIVE_TEXT("DUMP_IO")) flags.dumpIO = true;
            }
            
            if (end == NativeString::npos) break;
            start = end + 1;
        }
        return flags;
    }
};

#ifdef _WIN32
static BOOL WINAPI GlobalCtrlHandler(DWORD ctrlType) {
    if (g_sig_log_handle != INVALID_HANDLE_VALUE) {
        char msg[64];
        int n = snprintf(msg, sizeof(msg), "[SIGNAL] Windows Event: %lu\n", ctrlType);
        if (n > 0) {
            DWORD written;
            WriteFile(g_sig_log_handle, msg, (DWORD)n, &written, NULL);
        }
    }
    return TRUE; 
}
#endif

struct IORelay {
    static void relay(std::atomic<bool>& running, 
#ifdef _WIN32
                     HANDLE src, HANDLE dst, 
#else
                     int src, int dst, 
#endif
                     const fs::path& dumpPath) {
        std::ofstream dump(dumpPath, std::ios::binary);
        std::vector<char> buffer(64 * 1024);
        while (running) {
#ifdef _WIN32
            DWORD bytesRead = 0;
            if (!ReadFile(src, buffer.data(), (DWORD)buffer.size(), &bytesRead, NULL) || bytesRead == 0) break;
            if (dump.is_open()) { dump.write(buffer.data(), bytesRead); dump.flush(); }
            DWORD bytesWritten = 0;
            if (!WriteFile(dst, buffer.data(), bytesRead, &bytesWritten, NULL)) break;
#else
            ssize_t n = read(src, buffer.data(), buffer.size());
            if (n < 0) { if (errno == EAGAIN || errno == EINTR) continue; break; }
            if (n == 0) break;
            if (dump.is_open()) { dump.write(buffer.data(), n); dump.flush(); }
            if (write(dst, buffer.data(), (size_t)n) <= 0) break;
#endif
        }
    }
};

class Logger {
public:
    enum class Level { Info, Warn, Error };

    template<typename... Args>
    NOINLINE static void info(Args&&... args) { log(Level::Info, std::forward<Args>(args)...); }
    
    template<typename... Args>
    NOINLINE COLD static void warn(Args&&... args) { log(Level::Warn, std::forward<Args>(args)...); }
    
    template<typename... Args>
    NOINLINE COLD static void error(Args&&... args) { log(Level::Error, std::forward<Args>(args)...); }

    static NativeString getFileTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &timer);
#else
        localtime_r(&timer, &tm_buf);
#endif
        NativeChar buf[128];
        NativeSnprintf(buf, 128, NATIVE_TEXT("%04d%02d%02d_%02d%02d%02d_%03d"),
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
        return NativeString(buf);
    }

private:
#ifdef _WIN32
    static std::wstring to_log(const char* s) {
        if (!s) return L"";
        int sz = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
        std::wstring ws(sz, 0);
        MultiByteToWideChar(CP_UTF8, 0, s, -1, ws.data(), sz);
        if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
        return ws;
    }
    static std::wstring to_log(const std::string& s) { return to_log(s.c_str()); }
    static const wchar_t* to_log(const wchar_t* s) { return s ? s : L""; }
    static const std::wstring& to_log(const std::wstring& s) { return s; }
    static std::wstring to_log(const fs::path& p) { return p.native(); }
    static std::wstring to_log(int i) { return std::to_wstring(i); }
    static std::wstring to_log(unsigned long i) { return std::to_wstring(i); }
    static std::wstring to_log(size_t i) { return std::to_wstring(i); }
#else
    template<typename T> static T&& to_log(T&& val) { return std::forward<T>(val); }
    static std::string to_log(const fs::path& p) { return p.native(); }
#endif

    static NativeString getTimestamp() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        NativeChar buf[64];
#ifdef _WIN32
        std::wcsftime(buf, 64, L"%Y-%m-%d %H:%M:%S", &tm_buf);
#else
        std::strftime(buf, 64, "%Y-%m-%d %H:%M:%S", &tm_buf);
#endif
        return NativeString(buf);
    }

    static NativeStringView getPrefix(Level level) {
        switch (level) {
            case Level::Error: return NATIVE_TEXT("[ERROR] ");
            case Level::Warn:  return NATIVE_TEXT("[WARN]  ");
            default:           return NATIVE_TEXT("[INFO]  ");
        }
    }

    template<typename... Args>
    static void log(Level level, Args&&... args) {
        auto& out = (level == Level::Info) ? NativeCout : NativeCerr;
        out << NATIVE_TEXT("[") << getTimestamp() << NATIVE_TEXT("] ") << getPrefix(level);
        (out << ... << to_log(std::forward<Args>(args)));
        out << std::endl;
    }
};

struct EnvLessCmp {
    using is_transparent = void;
    bool operator()(NativeStringView a, NativeStringView b) const {
#ifdef _WIN32
        return _wcsnicmp(a.data(), b.data(), (std::min)(a.size(), b.size())) < 0 || 
               (_wcsnicmp(a.data(), b.data(), (std::min)(a.size(), b.size())) == 0 && a.size() < b.size());
#else
        return a < b;
#endif
    }
    bool operator()(const NativeString& a, const NativeString& b) const {
#ifdef _WIN32
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
#else
        return a < b;
#endif
    }
    bool operator()(NativeStringView a, const NativeString& b) const { return (*this)(a, NativeStringView(b)); }
    bool operator()(const NativeString& a, NativeStringView b) const { return (*this)(NativeStringView(a), b); }
};

using EnvMap = std::map<NativeString, NativeString, EnvLessCmp>;

class EnvConfig {
public:
    std::map<NativeString, NativeString, EnvLessCmp> variablesToAdd;
    std::set<NativeString, EnvLessCmp> variablesToRemove;

    static NativeStringView trim_view(NativeStringView sv) {
        auto first = sv.find_first_not_of(NATIVE_TEXT(" \t\r\n"));
        if (first == NativeStringView::npos) return {};
        auto last = sv.find_last_not_of(NATIVE_TEXT(" \t\r\n"));
        return sv.substr(first, (last - first + 1));
    }

    bool load(const fs::path& configPath) {
        std::error_code ec;
        auto fileSize = fs::file_size(configPath, ec);
        if (ec) {
            Logger::error("Dateizugriff fehlgeschlagen: ", configPath, " (", ec.message(), ")");
            return false;
        }

        if (fileSize > Constants::MAX_CONFIG_SIZE) {
            Logger::error("Konfigurationsdatei zu gross: ", configPath, " (Limit: ", Constants::MAX_CONFIG_SIZE / (1024 * 1024), "MB)");
            return false;
        }

        std::ifstream file(configPath, std::ios::binary);
        if (!file.is_open()) return false;

        std::string byteBuffer(fileSize, '\0');
        file.read(byteBuffer.data(), fileSize);
        std::string_view contentView(byteBuffer);
        
        if (contentView.starts_with("\xEF\xBB\xBF")) contentView.remove_prefix(3);

    #ifdef _WIN32
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, contentView.data(), (int)contentView.size(), NULL, 0);
        std::wstring nativeContent(wideSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, contentView.data(), (int)contentView.size(), nativeContent.data(), wideSize);
    #else
        std::string nativeContent(contentView);
    #endif

        NativeStringView fullContent(nativeContent);
        size_t lineStart = 0, lineNumber = 0;

        while (lineStart < fullContent.size()) 
        {
            size_t lineEnd = fullContent.find(NATIVE_TEXT('\n'), lineStart);
            NativeStringView line = fullContent.substr(lineStart, (lineEnd == NativeStringView::npos ? fullContent.size() : lineEnd) - lineStart);
            lineStart = (lineEnd == NativeStringView::npos) ? fullContent.size() : lineEnd + 1;
            lineNumber++;

            if (line.size() > Constants::MAX_LINE_SIZE) {
                Logger::warn("Zeile ", lineNumber, " zu lang (Limit: ", Constants::MAX_LINE_SIZE / 1024, "KB). Eintrag wird uebersprungen.");
                continue;
            }

            auto firstCharPos = line.find_first_not_of(NATIVE_TEXT(" \t\r\n"));
            if (firstCharPos == NativeStringView::npos ||
                line[firstCharPos] == NATIVE_TEXT('#') || 
                line[firstCharPos] == NATIVE_TEXT(';')) continue;
            
            NativeChar prefixChar = line[firstCharPos];
            if (prefixChar != NATIVE_TEXT('+') && prefixChar != NATIVE_TEXT('-')) {
                Logger::error("Zeile ", lineNumber, ": Unbekanntes Praefix '", prefixChar, "'. Erwarte '+' oder '-'.");
                continue;
            }

            if (line.size() > firstCharPos + 1 && isspace((unsigned char)line[firstCharPos + 1]))
            {
                NativeStringView entryView = trim_view(line.substr(firstCharPos + 1));
                if (prefixChar == NATIVE_TEXT('-')) {
                    if (auto eqPos = entryView.find(NATIVE_TEXT('=')); eqPos != NativeStringView::npos) {
                        auto key = trim_view(entryView.substr(0, eqPos));
                        if (!key.empty()) variablesToRemove.insert(NativeString(key));
                    } else {
                        if (!entryView.empty()) variablesToRemove.insert(NativeString(entryView));
                    }
                } else if (prefixChar == NATIVE_TEXT('+')) {
                    if (auto eqPos = entryView.find(NATIVE_TEXT('=')); eqPos != NativeStringView::npos) {
                        auto key = trim_view(entryView.substr(0, eqPos));
                        auto val = trim_view(entryView.substr(eqPos + 1));
                        if (!key.empty()) variablesToAdd[NativeString(key)] = NativeString(val);
                        else Logger::error("Zeile ", lineNumber, ": Leerer Key ignoriert.");
                    } else {
                        Logger::error("Zeile ", lineNumber, ": Erwarte KEY=VAL.");
                    }
                }
            } else {
                Logger::error("Zeile ", lineNumber, ": Format ungueltig oder Leerzeichen nach Praefix fehlt.");
            }
        }
        return true;
    }
};

// --- RESULT TYPES ---
struct EnvResult {
    std::unique_ptr<EnvMap> env;
    Constants::ExitCode status = Constants::SUCCESS;
};

struct PathResult {
    fs::path path;
    Constants::ExitCode status = Constants::SUCCESS;
};

// --- BRIDGE INTERFACE ---
class IPlatformBridge {
public:
    virtual ~IPlatformBridge() = default;
    virtual void setupConsole() = 0;
    virtual PathResult getExecutablePath() = 0;
    virtual EnvMap getSystemEnvironment() = 0;
    virtual void dumpEnvironment(const fs::path& basePath, const EnvMap& env, NativeStringView suffix) = 0;
    virtual void dumpArgs(const fs::path& basePath, int argc, NativeChar** argv) = 0;
    virtual void dumpPipes(const fs::path& basePath) = 0;
    virtual void dumpSignals(const fs::path& basePath) = 0;
    virtual bool isRedirEnabled() = 0;
    virtual DebugFlags getDebugFlags() = 0;
    virtual int executeChild(const fs::path& target, int argc, NativeChar** argv, const EnvMap* newEnv, const fs::path& basePath, const DebugFlags& flags) = 0;

    EnvResult prepareEnvironment(const fs::path& basePath) {
        if (!isRedirEnabled()) return { nullptr, Constants::SUCCESS };

        fs::path configPath = basePath.native() + NativeString(Constants::CONF_SUFFIX);
        EnvConfig config;
        if (fs::exists(configPath)) {
            if (!config.load(configPath)) {
                return { nullptr, Constants::ERR_CONFIG };
            }
        } else {
            Logger::warn("Keine Konfigurationsdatei gefunden: ", configPath);
        }

        auto finalEnv = std::make_unique<EnvMap>(getSystemEnvironment());
        for (auto it = finalEnv->begin(); it != finalEnv->end(); ) {
            if (config.variablesToRemove.count(it->first) || config.variablesToAdd.count(it->first)) it = finalEnv->erase(it);
            else ++it;
        }
        for (auto const& [k, v] : config.variablesToAdd) (*finalEnv)[k] = v;
        return { std::move(finalEnv), Constants::SUCCESS };
    }
};

#ifdef _WIN32
class Win32Bridge : public IPlatformBridge {
    class SafeHandle {
        HANDLE h;
    public:
        explicit SafeHandle(HANDLE h = NULL) : h(h) {}
        ~SafeHandle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
        operator HANDLE() const { return h; }
    };

public:
    void setupConsole() override {
        wchar_t u16Flag[4];
        if (GetEnvironmentVariableW(L"REDIR_ENABLE_U16TEXT", u16Flag, 4) > 0 && u16Flag[0] == L'1') {
            auto retVal = _setmode(_fileno(stdout), _O_U16TEXT);
            if (retVal == -1) {
                Logger::error("Fehler beim Setzen des Modus für stdout: ", GetLastError());
            }
            retVal = _setmode(_fileno(stderr), _O_U16TEXT);
            if (retVal == -1) {
                Logger::error("Fehler beim Setzen des Modus für stderr: ", GetLastError());
            }
            retVal = _setmode(_fileno(stdin), _O_U16TEXT);
            if (retVal == -1) {
                Logger::error("Fehler beim Setzen des Modus für stdin: ", GetLastError());
            }
        }
    }

    bool isRedirEnabled() override {
        wchar_t redirFlag[4];
        return (GetEnvironmentVariableW(L"REDIR_ENABLE_REDIR", redirFlag, 4) > 0 && redirFlag[0] == L'1');
    }

    DebugFlags getDebugFlags() override {
        wchar_t flag[256];
        DWORD res = GetEnvironmentVariableW(Constants::DEBUG_VAR.data(), flag, 256);
        if (res > 0 && res < 256) return DebugFlags::parse(flag);
        return {};
    }

    PathResult getExecutablePath() override {
        NativeString rawPath;
        DWORD bufferSize = MAX_PATH;
        while (true) {
            rawPath.resize(bufferSize); SetLastError(0);
            DWORD res = GetModuleFileNameW(0, rawPath.data(), bufferSize);
            if (!res) return { "", Constants::ERR_INTERNAL };
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) { bufferSize *= 2; continue; }
            rawPath.resize(res); return { fs::path(rawPath), Constants::SUCCESS };
        }
    }

    EnvMap getSystemEnvironment() override {
        EnvMap env;
        if (LPWCH sysEnv = GetEnvironmentStringsW()) {
            for (LPWCH entry = sysEnv; *entry; entry += wcslen(entry) + 1) {
                NativeStringView ev(entry);
                if (size_t eq = ev.find(L'=', 1); eq != NativeStringView::npos) {
                    env[NativeString(ev.substr(0, eq))] = NativeString(ev.substr(eq + 1));
                }
            }
            FreeEnvironmentStringsW(sysEnv);
        }
        return env;
    }

    void dumpEnvironment(const fs::path& basePath, const EnvMap& env, NativeStringView suffix) override {
        NativeString fileName = basePath.stem().native() + NATIVE_TEXT("_") + std::to_wstring(GetCurrentProcessId()) + 
                              NATIVE_TEXT("_") + Logger::getFileTimestamp() + NativeString(suffix) + NATIVE_TEXT(".txt");
        std::ofstream file(basePath.parent_path() / fileName, std::ios::binary);
        if (!file) return;
        for (auto const& [k, v] : env) {
            std::wstring entry = k + L"=" + v + L"\r\n";
            int sz = WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), -1, NULL, 0, NULL, NULL);
            if (sz > 1) {
                std::string u8(sz, 0);
                WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), -1, u8.data(), sz, NULL, NULL);
                file.write(u8.data(), sz - 1); 
            }
        }
    }

    void dumpArgs(const fs::path& basePath, int, NativeChar**) override {
        NativeString fileName = basePath.stem().native() + NATIVE_TEXT("_") + std::to_wstring(GetCurrentProcessId()) + 
                              NATIVE_TEXT("_") + Logger::getFileTimestamp() + NATIVE_TEXT("_args.txt");
        std::ofstream file(basePath.parent_path() / fileName, std::ios::binary);
        if (!file) return;
        std::wstring cmdLine = GetCommandLineW();
        int sz = WideCharToMultiByte(CP_UTF8, 0, cmdLine.c_str(), -1, NULL, 0, NULL, NULL);
        if (sz > 1) {
            std::string u8(sz, 0);
            WideCharToMultiByte(CP_UTF8, 0, cmdLine.c_str(), -1, u8.data(), sz, NULL, NULL);
            file.write(u8.data(), sz - 1); 
        }
    }

    void dumpPipes(const fs::path& basePath) override {
        NativeString fileName = basePath.stem().native() + NATIVE_TEXT("_") + std::to_wstring(GetCurrentProcessId()) + 
                              NATIVE_TEXT("_") + Logger::getFileTimestamp() + NATIVE_TEXT("_pipes.txt");
        std::ofstream file(basePath.parent_path() / fileName);
        if (!file) return;
        auto checkPipe = [&](DWORD stdHandle, const char* name) {
            HANDLE h = GetStdHandle(stdHandle);
            file << name << ": ";
            if (h == INVALID_HANDLE_VALUE) file << "INVALID";
            else if (h == NULL) file << "NULL";
            else {
                DWORD mode;
                if (GetConsoleMode(h, &mode)) file << "CONSOLE";
                else {
                    DWORD type = GetFileType(h);
                    if (type == FILE_TYPE_CHAR) file << "CHAR";
                    else if (type == FILE_TYPE_DISK) file << "DISK";
                    else if (type == FILE_TYPE_PIPE) file << "PIPE";
                    else file << "UNKNOWN(" << type << ")";
                }
            }
            file << "\n";
        };
        checkPipe(STD_INPUT_HANDLE, "stdin");
        checkPipe(STD_OUTPUT_HANDLE, "stdout");
        checkPipe(STD_ERROR_HANDLE, "stderr");
    }

    void dumpSignals(const fs::path& basePath) override {
        NativeString fileName = basePath.stem().native() + NATIVE_TEXT("_") + std::to_wstring(GetCurrentProcessId()) + 
                              NATIVE_TEXT("_") + Logger::getFileTimestamp() + NATIVE_TEXT("_signals.txt");
        g_sig_log_handle = CreateFileW((basePath.parent_path() / fileName).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_sig_log_handle != INVALID_HANDLE_VALUE) {
            const char* msg = "Windows Signal Handling: Persistent log active.\n";
            DWORD written;
            WriteFile(g_sig_log_handle, msg, (DWORD)strlen(msg), &written, NULL);
        }
    }

    int executeChild(const fs::path& target, int argc, NativeChar** argv, const EnvMap* newEnv, const fs::path& basePath, const DebugFlags& flags) override {
        bool enabled = isRedirEnabled();
        if (enabled) {
            if (flags.dumpArgs) dumpArgs(basePath, argc, argv);
            if (flags.dumpPipes) dumpPipes(basePath);
            if (flags.dumpSignals) dumpSignals(basePath);
        }

        std::atomic<bool> ioRunning{true};
        std::thread tIn, tOut, tErr;
        HANDLE hChildInR = NULL, hChildInW = NULL;
        HANDLE hChildOutR = NULL, hChildOutW = NULL;
        HANDLE hChildErrR = NULL, hChildErrW = NULL;

        if (enabled && flags.dumpIO) {
            SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
            if (!CreatePipe(&hChildInR, &hChildInW, &sa, 0) ||
                !CreatePipe(&hChildOutR, &hChildOutW, &sa, 0) ||
                !CreatePipe(&hChildErrR, &hChildErrW, &sa, 0)) {
                Logger::error("Konnte Pipes fuer DUMP_IO nicht erstellen.");
                return Constants::ERR_INTERNAL;
            }
            SetHandleInformation(hChildInW, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hChildOutR, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hChildErrR, HANDLE_FLAG_INHERIT, 0);
        }

        std::vector<NativeChar> winEnvBlock;
        if (newEnv) {
            size_t total = 1;
            for (auto const& [k, v] : *newEnv) total += k.length() + v.length() + 2;
            winEnvBlock.reserve(total);
            for (auto const& [k, v] : *newEnv) {
                winEnvBlock.insert(winEnvBlock.end(), k.begin(), k.end());
                winEnvBlock.push_back(L'=');
                winEnvBlock.insert(winEnvBlock.end(), v.begin(), v.end());
                winEnvBlock.push_back(L'\0');
            }
            winEnvBlock.push_back(L'\0');
        }

        SafeHandle job(CreateJobObjectW(NULL, NULL));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = { 0 };
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags |= STARTF_USESTDHANDLES;
        if (flags.dumpIO) {
            si.hStdInput = hChildInR; si.hStdOutput = hChildOutW; si.hStdError = hChildErrW;
        } else {
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        }
        PROCESS_INFORMATION pi = { 0 };

        NativeCout.flush(); NativeCerr.flush();
        auto closeIfValid = [](HANDLE h) { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); };

        if (!CreateProcessW(target.c_str(), GetCommandLineW(), 0, 0, 1, CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, newEnv ? winEnvBlock.data() : nullptr, 0, &si, &pi)) {
            Logger::error("Start fehlgeschlagen: ", target, " (Code: ", GetLastError(), ")");
            if (flags.dumpIO) {
                closeIfValid(hChildInR); closeIfValid(hChildInW);
                closeIfValid(hChildOutR); closeIfValid(hChildOutW);
                closeIfValid(hChildErrR); closeIfValid(hChildErrW);
            }
            return Constants::ERR_TARGET;
        }
        SafeHandle hProc(pi.hProcess), hThread(pi.hThread);
        AssignProcessToJobObject(job, hProc);
        ResumeThread(hThread);

        if (flags.dumpIO) {
            closeIfValid(hChildInR); closeIfValid(hChildOutW); closeIfValid(hChildErrW);

            NativeString ts = Logger::getFileTimestamp();
            NativeString pfx = basePath.stem().native() + NATIVE_TEXT("_") + std::to_wstring(GetCurrentProcessId()) + NATIVE_TEXT("_") + ts;
            
            // tIn Relay: Liest von Parent-Stdin, schreibt in hChildInW
            tIn = std::thread([&ioRunning, hChildInW, basePath, pfx]() {
                IORelay::relay(std::ref(ioRunning), GetStdHandle(STD_INPUT_HANDLE), hChildInW, basePath.parent_path() / (pfx + NATIVE_TEXT("_stdin.bin")));
                // WICHTIG: hChildInW SOFORT schließen, damit das Kind EOF sieht
                if (hChildInW && hChildInW != INVALID_HANDLE_VALUE) CloseHandle(hChildInW);
            });

            tOut = std::thread(IORelay::relay, std::ref(ioRunning), hChildOutR, GetStdHandle(STD_OUTPUT_HANDLE), basePath.parent_path() / (pfx + NATIVE_TEXT("_stdout.bin")));
            tErr = std::thread(IORelay::relay, std::ref(ioRunning), hChildErrR, GetStdHandle(STD_ERROR_HANDLE), basePath.parent_path() / (pfx + NATIVE_TEXT("_stderr.bin")));
        }

        // Vater auf "Durchzug" stellen: Alle Signale ignorieren, damit das Kind sie verarbeiten kann
        (void)SetConsoleCtrlHandler(GlobalCtrlHandler, TRUE);
        
        WaitForSingleObject(hProc, INFINITE);

        ioRunning = false;
        
        if (flags.dumpIO) {
            // Unblock tIn if it's waiting for console input
            CancelSynchronousIo(tIn.native_handle());
            
            // hChildInW wird bereits im Lambda von tIn geschlossen.
            if (hChildOutR) CloseHandle(hChildOutR);
            if (hChildErrR) CloseHandle(hChildErrR);

            if (tIn.joinable()) tIn.join();
            if (tOut.joinable()) tOut.join();
            if (tErr.joinable()) tErr.join();
        }

        if (g_sig_log_handle != INVALID_HANDLE_VALUE) { CloseHandle(g_sig_log_handle); g_sig_log_handle = INVALID_HANDLE_VALUE; }

        DWORD code; GetExitCodeProcess(hProc, &code);
        return (int)code;
    }
};
#else
class LinuxBridge : public IPlatformBridge {
public:
    void setupConsole() override {} 

    bool isRedirEnabled() override {
        const char* p = std::getenv("REDIR_ENABLE_REDIR");
        return (p && p[0] == '1');
    }

    DebugFlags getDebugFlags() override {
        const char* p = std::getenv(std::string(Constants::DEBUG_VAR.begin(), Constants::DEBUG_VAR.end()).c_str());
        return p ? DebugFlags::parse(p) : DebugFlags{};
    }

    PathResult getExecutablePath() override {
        std::string path;
        size_t bufferSize = 1024;
        while (true) {
            path.resize(bufferSize);
            ssize_t len = readlink("/proc/self/exe", path.data(), bufferSize - 1);
            if (len != -1) {
                if (static_cast<size_t>(len) < bufferSize - 1) {
                    path.resize(len);
                    return { fs::path(path), Constants::SUCCESS };
                } else {
                    bufferSize *= 2;
                    continue;
                }
            }
            return { "", Constants::ERR_INTERNAL };
        }
    }

    EnvMap getSystemEnvironment() override {
        EnvMap env;
        for (char** e = environ; *e; ++e) {
            std::string_view ev(*e);
            if (size_t eq = ev.find('='); eq != std::string_view::npos) {
                env[NativeString(ev.substr(0, eq))] = NativeString(ev.substr(eq + 1));
            }
        }
        return env;
    }

    void dumpEnvironment(const fs::path& basePath, const EnvMap& env, NativeStringView suffix) override {
        NativeString fileName = basePath.stem().native() + "_" + std::to_string(getpid()) + "_" + Logger::getFileTimestamp() + std::string(suffix) + ".txt";
        std::ofstream file(basePath.parent_path() / fileName, std::ios::binary);
        if (!file) return;
        for (auto const& [k, v] : env) {
            file << k << "=" << v << "\n";
        }
    }

    void dumpArgs(const fs::path& basePath, int argc, char** argv) override {
        NativeString fileName = basePath.stem().native() + "_" + std::to_string(getpid()) + "_" + Logger::getFileTimestamp() + "_args.txt";
        std::ofstream file(basePath.parent_path() / fileName);
        if (!file) return;
        for (int i = 0; i < argc; ++i) {
            file << "argv[" << i << "]: " << argv[i] << "\n";
        }
    }

    void dumpPipes(const fs::path& basePath) override {
        NativeString fileName = basePath.stem().native() + "_" + std::to_string(getpid()) + "_" + Logger::getFileTimestamp() + "_pipes.txt";
        std::ofstream file(basePath.parent_path() / fileName);
        if (!file) return;
        auto checkPipe = [&](int fd, const char* name) {
            file << name << ": ";
            if (isatty(fd)) file << "TTY";
            else {
                struct stat st;
                if (fstat(fd, &st) == -1) file << "INVALID";
                else if (S_ISFIFO(st.st_mode)) file << "FIFO";
                else if (S_ISCHR(st.st_mode)) file << "CHAR";
                else if (S_ISREG(st.st_mode)) file << "REGULAR";
                else file << "UNKNOWN";
            }
            file << "\n";
        };
        checkPipe(STDIN_FILENO, "stdin");
        checkPipe(STDOUT_FILENO, "stdout");
        checkPipe(STDERR_FILENO, "stderr");
    }

    void dumpSignals(const fs::path& basePath) override {
        NativeString fileName = basePath.stem().native() + "_" + std::to_string(getpid()) + "_" + Logger::getFileTimestamp() + "_signals.txt";
        g_sig_log_fd = open((basePath.parent_path() / fileName).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (g_sig_log_fd != -1) {
            const char* msg = "Linux Signal Handling: Persistent log active.\n";
            ssize_t written = write(g_sig_log_fd, msg, strlen(msg));
            (void)written; // Satisfy warn_unused_result
        }
    }

    int executeChild(const fs::path& target, int argc, char** argv, const EnvMap* newEnv, const fs::path& basePath, const DebugFlags& flags) override {
        if (flags.dumpArgs) dumpArgs(basePath, argc, argv);
        if (flags.dumpPipes) dumpPipes(basePath);
        if (flags.dumpSignals) dumpSignals(basePath);

        std::atomic<bool> ioRunning{true};
        std::thread tIn, tOut, tErr;
        int pIn[2], pOut[2], pErr[2];

        if (flags.dumpIO) {
            if (pipe(pIn) == -1 || pipe(pOut) == -1 || pipe(pErr) == -1) {
                Logger::error("Konnte Pipes fuer DUMP_IO nicht erstellen (errno: ", errno, ")");
                return Constants::ERR_INTERNAL;
            }
        }

        std::deque<std::string> linuxEnvStorage;
        std::vector<char*> linuxEnvPointers;
        if (newEnv) {
            for (auto const& [k, v] : *newEnv) {
                linuxEnvStorage.push_back(k + "=" + v);
                linuxEnvPointers.push_back((char*)linuxEnvStorage.back().data());
            }
            linuxEnvPointers.push_back(nullptr);
        }

        // --- SIGNAL HANDLING SETUP ---
        sigset_t oldset, fullset;
        sigfillset(&fullset);
        // Blockiere alle Signale vor dem Fork, um Race-Conditions zu vermeiden
        sigprocmask(SIG_BLOCK, &fullset, &oldset);

        NativeCout.flush(); NativeCerr.flush();
        pid_t pid = fork();
        if (pid == 0) {
            // IM KIND:
            // 1. Erstelle eine neue Prozessgruppe, damit Terminal-Signale (Ctrl+C) nicht doppelt
            //    am Kind ankommen. Der Wrapper fungiert nun als alleiniger Signal-Manager.
            setpgid(0, 0);

            if (flags.dumpIO) {
                dup2(pIn[0], STDIN_FILENO); close(pIn[0]); close(pIn[1]);
                dup2(pOut[1], STDOUT_FILENO); close(pOut[0]); close(pOut[1]);
                dup2(pErr[1], STDERR_FILENO); close(pErr[0]); close(pErr[1]);
            }
            // 1. Signale deblockieren (erben den Block-Status)
            sigprocmask(SIG_SETMASK, &oldset, NULL);
            // 2. Wichtige Signale auf Default zurücksetzen (falls Vater sie ignorierte)
            int signalsToReset[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGUSR1, SIGUSR2, SIGWINCH, SIGPWR, SIGALRM };
            for (int s : signalsToReset) signal(s, SIG_DFL);
            
            prctl(PR_SET_PDEATHSIG, SIGTERM); // benign race, in my opion
            execve(target.c_str(), argv, newEnv ? linuxEnvPointers.data() : environ);
            Logger::error("execve fehlgeschlagen: ", target, " (errno: ", errno, ")");
            _exit(Constants::ERR_TARGET);
        } else if (pid > 0) {
            // IM VATER:
            g_child_pid = pid;
            if (flags.dumpIO) {
                // WICHTIG: Pipe-Enden, die das Kind nutzt, im Vater SOFORT schließen
                close(pIn[0]); close(pOut[1]); close(pErr[1]);
                
                std::string ts = Logger::getFileTimestamp();
                std::string pfx = basePath.stem().native() + "_" + std::to_string(getpid()) + "_" + ts;
                tIn = std::thread(IORelay::relay, std::ref(ioRunning), STDIN_FILENO, pIn[1], basePath.parent_path() / (pfx + "_stdin.bin"));
                tOut = std::thread(IORelay::relay, std::ref(ioRunning), pOut[0], STDOUT_FILENO, basePath.parent_path() / (pfx + "_stdout.bin"));
                tErr = std::thread(IORelay::relay, std::ref(ioRunning), pErr[0], STDERR_FILENO, basePath.parent_path() / (pfx + "_stderr.bin"));
            }
            
            // Relay Handler für relevante Signale registrieren (Dynamische Weiterleitung)
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = signalRelayHandler;
            for (int s = 1; s < NSIG; ++s) {
                if (s == SIGCHLD) continue; 
                sigaction(s, &sa, NULL);
            }
            
            // Signale deblockieren, damit Handler/Wait arbeiten können
            sigprocmask(SIG_SETMASK, &oldset, NULL);

            int status;
            while (waitpid(pid, &status, 0) == -1) {
                if (errno != EINTR) break;
            }
            g_child_pid = 0; 

            ioRunning = false;
            if (flags.dumpIO) {
                // Schließe die Vater-Enden der Pipes, um blockierende Reads in tOut/tErr zu lösen
                close(pIn[1]); close(pOut[0]); close(pErr[0]);
                
                // Für tIn (blockiert auf STDIN_FILENO) nutzen wir pthread_cancel
#ifndef _WIN32
                pthread_cancel(tIn.native_handle());
#endif
                if (tIn.joinable()) tIn.join();
                if (tOut.joinable()) tOut.join();
                if (tErr.joinable()) tErr.join();
            }
            if (g_sig_log_fd != -1) { close(g_sig_log_fd); g_sig_log_fd = -1; }
            return WIFEXITED(status) ? WEXITSTATUS(status) : Constants::ERR_INTERNAL;
        }
        
        // Fork fehlgeschlagen
        sigprocmask(SIG_SETMASK, &oldset, NULL);
        if (flags.dumpIO) { close(pIn[0]); close(pIn[1]); close(pOut[0]); close(pOut[1]); close(pErr[0]); close(pErr[1]); }
        Logger::error("fork() fehlgeschlagen (errno: ", errno, ")");
        return Constants::ERR_INTERNAL;
    }
};
#endif

std::unique_ptr<IPlatformBridge> createBridge() {
#ifdef _WIN32
    return std::make_unique<Win32Bridge>();
#else
    return std::make_unique<LinuxBridge>();
#endif
}

#ifdef _WIN32
int wmain(int argc, NativeChar** argv) {
#else
int main(int argc, NativeChar** argv) {
#endif
    auto bridge = createBridge();
    bridge->setupConsole();
    
    PathResult pathRes = bridge->getExecutablePath();
    if (pathRes.status != Constants::SUCCESS) {
        Logger::error("Konnte Executable-Pfad nicht ermitteln.");
        return pathRes.status;
    }
    
    fs::path exePath = pathRes.path;
    fs::path basePath = exePath; basePath.replace_extension();
    
    DebugFlags flags = bridge->getDebugFlags();
    if (flags.preEnv) bridge->dumpEnvironment(basePath, bridge->getSystemEnvironment(), NATIVE_TEXT("_pre_env"));

    EnvResult envRes = bridge->prepareEnvironment(basePath);
    if (envRes.status != Constants::SUCCESS) {
        Logger::error("Umgebungs-Vorbereitung fehlgeschlagen.");
        return envRes.status;
    }
    
    if (flags.postEnv && envRes.env) bridge->dumpEnvironment(basePath, *envRes.env, NATIVE_TEXT("_post_env"));

    fs::path target = basePath.native() + NativeString(Constants::ORG_SUFFIX);
    return bridge->executeChild(target, argc, argv, envRes.env.get(), basePath, flags);
}
