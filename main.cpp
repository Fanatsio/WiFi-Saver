#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

const wchar_t* INTERFACE_NAME = L"Беспроводная сеть";

const char* CHECK_IP          = "8.8.8.8";
const int   CHECK_PORT        = 53;
const int   CHECK_TIMEOUT     = 3000;
const int   RETRY_BEFORE_RESTART = 2;
const int   SLEEP_INTERVAL    = 5;
const int   POST_RESTART_WAIT = 10;

enum Color {
    GREEN  = 10,
    RED    = 12,
    YELLOW = 14,
    WHITE  = 15
};

void setColor(Color color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

std::wstring getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &t);

    wchar_t buffer[9];
    wcsftime(buffer, 9, L"%H:%M:%S", &localTime);
    return buffer;
}

class WiFiManager {
private:
    std::wstring interfaceName;

public:
    WiFiManager() : interfaceName(INTERFACE_NAME) {
        setColor(WHITE);
        std::wcout << L"✅ Используется интерфейс: " << interfaceName << L"\n\n";
    }

    void restartWiFi() {
        setColor(RED);
        std::wcout << L"\n[" << getCurrentTime() << L"] 🔄 Перезапуск Wi-Fi...\n";

        std::wstring disableCmd = L"netsh interface set interface \"" + interfaceName + L"\" disable";
        std::wstring enableCmd  = L"netsh interface set interface \"" + interfaceName + L"\" enable";

        runCommand(disableCmd);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        bool success = runCommand(enableCmd);

        if (success) {
            setColor(GREEN);
            std::wcout << L"[" << getCurrentTime() << L"] ✅ Wi-Fi перезапущен. Ожидание восстановления...\n";
            std::this_thread::sleep_for(std::chrono::seconds(POST_RESTART_WAIT));
        } else {
            setColor(RED);
            std::wcout << L"[" << getCurrentTime() << L"] ❌ Ошибка при перезапуске Wi-Fi\n";
        }
        setColor(WHITE);
    }

private:
    bool runCommand(const std::wstring& command) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi{};

        std::wstring fullCmd = L"cmd.exe /C " + command;
        std::vector<wchar_t> cmdLine(fullCmd.begin(), fullCmd.end());
        cmdLine.push_back(L'\0');

        if (!CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return exitCode == 0;
    }
};

bool hasInternet() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(CHECK_PORT);

    if (inet_pton(AF_INET, CHECK_IP, &server.sin_addr) <= 0) {
        closesocket(sock);
        return false;
    }

    DWORD timeout = CHECK_TIMEOUT;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    int result = connect(sock, (sockaddr*)&server, sizeof(server));
    closesocket(sock);
    return result == 0;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    if (!isAdmin) {
        setColor(RED);
        std::wcout << L"❌ Программа должна быть запущена от имени администратора!\n";
        setColor(WHITE);
        std::wcout << L"Нажмите Enter для выхода...\n";
        std::cin.get();
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::wcout << L"❌ Ошибка WSAStartup\n";
        return 1;
    }

    WiFiManager wifiManager;

    std::wcout << L"==============================\n";
    std::wcout << L"      WiFi Monitor v1.1       \n";
    std::wcout << L"==============================\n\n";

    int failCount = 0;

    while (true) {
        std::wstring timeStr = getCurrentTime();

        if (hasInternet()) {
            setColor(GREEN);
            std::wcout << L"\r[" << timeStr << L"] 🌐 ONLINE  | Интернет работает стабильно          ";
            failCount = 0;
        } else {
            failCount++;
            setColor(YELLOW);
            std::wcout << L"\r[" << timeStr << L"] ⚠️ OFFLINE | Попытка: " << failCount << L"                    ";

            if (failCount >= RETRY_BEFORE_RESTART) {
                wifiManager.restartWiFi();
                failCount = 0;

                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (hasInternet()) {
                    setColor(GREEN);
                    std::wcout << L"[" << getCurrentTime() << L"] ✅ Интернет восстановлен после перезапуска\n";
                }
            }
        }

        setColor(WHITE);
        std::this_thread::sleep_for(std::chrono::seconds(SLEEP_INTERVAL));
    }

    WSACleanup();
    return 0;
}