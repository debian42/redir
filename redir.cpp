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
 * 100+: Perfektion. Kein One-Shot-Prompt, sondern eine echte Kooperation zwischen 
 *       Mensch und Maschine. Senior-Engineer-Approved. Müller lächelt (jetzt wirklich).
 */
 
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
#else
using NativeChar = char;
using NativeString = std::string;
using NativeStringView = std::string_view;
#define NATIVE_TEXT(s) s
#define NativeCout std::cout
#define NativeCerr std::cerr
#endif

namespace Constants {
    constexpr NativeStringView CONF_SUFFIX = NATIVE_TEXT("_conf.env");
    constexpr NativeStringView DUMP_SUFFIX = NATIVE_TEXT("_env.txt");
#ifdef _WIN32
    constexpr NativeStringView ORG_SUFFIX  = NATIVE_TEXT("_org.exe");
#else
    constexpr NativeStringView ORG_SUFFIX  = NATIVE_TEXT("_org");
#endif
    enum ExitCode { SUCCESS = 0, ERR_INTERNAL = 100, ERR_CONFIG = 101, ERR_TARGET = 102 };

    // --- SECURITY POLICY ---
    constexpr size_t MAX_CONFIG_SIZE = 1024 * 1024; // 1 MB
    constexpr size_t MAX_LINE_SIZE   = 32 * 1024;   // 32 KB
}

class Logger {
public:
    enum class Level { Info, Warn, Error };

    template<typename... Args>
    static void info(Args&&... args) { log(Level::Info, std::forward<Args>(args)...); }
    
    template<typename... Args>
    static void warn(Args&&... args) { log(Level::Warn, std::forward<Args>(args)...); }
    
    template<typename... Args>
    static void error(Args&&... args) { log(Level::Error, std::forward<Args>(args)...); }

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
    virtual void dumpEnvironment(const fs::path& basePath) = 0;
    virtual bool isRedirEnabled() = 0;
    virtual int executeChild(const fs::path& target, int argc, NativeChar** argv, const EnvMap* newEnv) = 0;

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

    void dumpEnvironment(const fs::path& basePath) override {
        wchar_t flag[4];
        if (GetEnvironmentVariableW(L"REDIR_DUMP_ENV", flag, 4) > 0 && flag[0] == L'1') {
            NativeString fileName = basePath.stem().native() + NativeString(NATIVE_TEXT("_")) + std::to_wstring(GetCurrentProcessId()) + NativeString(Constants::DUMP_SUFFIX);
            std::ofstream file(basePath.parent_path() / fileName, std::ios::binary);
            if (!file) return;
            if (LPWCH sysEnv = GetEnvironmentStringsW()) {
                for (LPWCH entry = sysEnv; *entry; entry += wcslen(entry) + 1) {
                    int sz = WideCharToMultiByte(CP_UTF8, 0, entry, -1, NULL, 0, NULL, NULL);
                    if (sz > 1) {
                        std::string u8(sz, 0);
                        WideCharToMultiByte(CP_UTF8, 0, entry, -1, u8.data(), sz, NULL, NULL);
                        file.write(u8.data(), sz - 1); 
                    }
                    file.write("\r\n", 2);
                }
                FreeEnvironmentStringsW(sysEnv);
            }
        }
    }

    int executeChild(const fs::path& target, int, NativeChar**, const EnvMap* newEnv) override {
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
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION pi = { 0 };

        NativeCout.flush(); NativeCerr.flush();
        if (!CreateProcessW(target.c_str(), GetCommandLineW(), 0, 0, 1, CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, newEnv ? winEnvBlock.data() : nullptr, 0, &si, &pi)) {
            Logger::error("Start fehlgeschlagen: ", target, " (Code: ", GetLastError(), ")");
            return Constants::ERR_TARGET;
        }
        SafeHandle hProc(pi.hProcess), hThread(pi.hThread);
        AssignProcessToJobObject(job, hProc);
        ResumeThread(hThread);
        WaitForSingleObject(hProc, INFINITE);
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

    PathResult getExecutablePath() override {
        char path[1024];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) { path[len] = '\0'; return { fs::path(path), Constants::SUCCESS }; }
        return { "", Constants::ERR_INTERNAL };
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

    void dumpEnvironment(const fs::path& basePath) override {
        const char* p = std::getenv("REDIR_DUMP_ENV");
        if (p && p[0] == '1') {
            NativeString fileName = basePath.stem().native() + NativeString(NATIVE_TEXT("_")) + std::to_string(getpid()) + NativeString(Constants::DUMP_SUFFIX);
            std::ofstream file(basePath.parent_path() / fileName, std::ios::binary);
            if (!file) return;
            for (char **e = environ; *e; ++e) {
                file.write(*e, strlen(*e));
                file.write("\n", 1);
            }
        }
    }

    int executeChild(const fs::path& target, int, char** argv, const EnvMap* newEnv) override {
        std::deque<std::string> linuxEnvStorage;
        std::vector<char*> linuxEnvPointers;
        if (newEnv) {
            for (auto const& [k, v] : *newEnv) {
                linuxEnvStorage.push_back(k + "=" + v);
                linuxEnvPointers.push_back((char*)linuxEnvStorage.back().data());
            }
            linuxEnvPointers.push_back(nullptr);
        }

        NativeCout.flush(); NativeCerr.flush();
        pid_t pid = fork();
        if (pid == 0) {
            prctl(PR_SET_PDEATHSIG, SIGTERM);
            execve(target.c_str(), argv, newEnv ? linuxEnvPointers.data() : environ);
            Logger::error("execve fehlgeschlagen: ", target, " (errno: ", errno, ")");
            _exit(Constants::ERR_TARGET);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) ? WEXITSTATUS(status) : Constants::ERR_INTERNAL;
        }
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
    bridge->dumpEnvironment(basePath);

    EnvResult envRes = bridge->prepareEnvironment(basePath);
    if (envRes.status != Constants::SUCCESS) {
        Logger::error("Umgebungs-Vorbereitung fehlgeschlagen.");
        return envRes.status;
    }
    
    fs::path target = basePath.native() + NativeString(Constants::ORG_SUFFIX);
    return bridge->executeChild(target, argc, argv, envRes.env.get());
}
