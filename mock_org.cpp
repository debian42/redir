#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <poll.h>
#include <signal.h>
extern char **environ;
#endif

/**
 * @brief Test-Programm für Redir Signal Forwarding & Basis-Validierung
 */

std::atomic<bool> keepRunning{true};

#ifdef _WIN32
std::string to_ansi(const wchar_t* wstr) {
    if (!wstr) return "";
    int sz = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, &s[0], sz, NULL, NULL);
    size_t last = s.find_last_not_of('\0');
    if (last != std::string::npos) s.resize(last + 1);
    else s.clear();
    return s;
}

BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    std::cout << "[MOCK_ORG] Console Event empfangen: " << ctrlType << std::endl;
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        std::cout << "[MOCK_ORG] Beende Test-Loop..." << std::endl;
        keepRunning = false;
        // Senior-Expert Trick: 200ms warten, damit Integration-Tests Zeit zum Reagieren haben
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return TRUE; 
    }
    return FALSE;
}
#else
void signalHandler(int signo) {
    std::cout << "[MOCK_ORG] Linux Signal empfangen: " << signo;
    switch(signo) {
        case SIGUSR1: std::cout << " (SIGUSR1 - Custom Logic!)"; break;
        case SIGUSR2: std::cout << " (SIGUSR2 - Custom Logic!)"; break;
        case SIGINT:  std::cout << " (SIGINT - Beende...)"; keepRunning = false; break;
        case SIGTERM: std::cout << " (SIGTERM - Beende...)"; keepRunning = false; break;
        case SIGWINCH: std::cout << " (SIGWINCH - Resize!)"; break;
        default: std::cout << " (Anderes Signal)"; break;
    }
    std::cout << std::endl;
    // Senior-Expert Trick: 200ms warten, damit Integration-Tests Zeit zum Reagieren haben
    if (signo == SIGINT || signo == SIGTERM) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}
#endif

int main(int argc, char* argv[]) {
    bool waitMode = false;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--wait") waitMode = true;
    }

    std::cout << "[MOCK_ORG] Startet, Herr Mueller..." << std::endl;
    std::cout << "[MOCK_ORG] PID: " << 
#ifdef _WIN32
        GetCurrentProcessId()
#else
        getpid()
#endif
        << std::endl;

    // --- WICHTIG FÜR TEST 1: Argumente ausgeben ---
    std::cout << "[MOCK_ORG] Argumente: ";
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;

    // --- WICHTIG FÜR TEST 1: Pipe-Input prüfen ---
    bool hasData = false;
#ifdef _WIN32
    if (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) != FILE_TYPE_CHAR) hasData = true;
#else
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    if (poll(&pfd, 1, 0) > 0) hasData = true;
#endif

    if (hasData) {
        std::string input;
        if (std::getline(std::cin, input)) {
            std::cout << "[MOCK_ORG] Stdin empfangen: '" << input << "'" << std::endl;
        }
    }

    if (waitMode) {
        std::cout << "[MOCK_ORG] Warte-Modus aktiv. Installiere Handler..." << std::endl;
#ifdef _WIN32
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = signalHandler;
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGWINCH, &sa, NULL);
#endif
        while (keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "[MOCK_ORG] Loop beendet." << std::endl;
        return 0;
    }

    // Normaler Environment-Dump Modus
    std::cout << "\n[MOCK_ORG] Rohes Environment (nach ANSI konvertiert):" << std::endl;
#ifdef _WIN32
    LPWCH es = GetEnvironmentStringsW();
    if (es) {
        for (LPWCH cur = es; *cur; cur += wcslen(cur) + 1) {
            std::cout << "  " << to_ansi(cur) << std::endl;
        }
        FreeEnvironmentStringsW(es);
    }
#else
    for (char **env = environ; *env; ++env) {
        std::cout << "  " << *env << std::endl;
    }
#endif

    return 42;
}
