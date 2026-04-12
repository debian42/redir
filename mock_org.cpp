#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <poll.h>
extern char **environ;
#endif

/**
 * @brief Test-Programm
 */

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
#endif

int main(int argc, char* argv[]) {
    std::cout << "[MOCK_ORG] Startet, Herr Mueller..." << std::endl;

    std::cout << "[MOCK_ORG] Argumente: ";
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;

    bool hasData = false;
#ifdef _WIN32
    if (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) != FILE_TYPE_CHAR) {
        // Auf Windows blockiert getline meist nicht, wenn die Pipe leer ist, 
        // aber wir bleiben konsistent.
        hasData = true; 
    }
#else
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    // poll mit 0ms timeout: Pruefe ob Daten SOFORT lesbar sind
    if (poll(&pfd, 1, 0) > 0) hasData = true;
#endif

    if (hasData) {
        std::string input;
        if (std::getline(std::cin, input)) {
            std::cout << "[MOCK_ORG] Stdin empfangen: '" << input << "'" << std::endl;
        }
    }

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
