#include <iostream>
#include <chrono>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <sddl.h>

#pragma comment(lib, "ws2_32.lib")

// ------------------------
// НАСТРОЙКИ
// ------------------------
const wchar_t* INTERFACE_NAME = L"Беспроводная сеть";
const char* CHECK_IP = "8.8.8.8";
const int CHECK_PORT = 53;
const int CHECK_TIMEOUT = 3000;
const int RETRY_BEFORE_RESTART = 2;
const int SLEEP_INTERVAL = 5;

// ------------------------
// ЦВЕТА
// ------------------------
enum Color {
    GREEN = 10,
    RED = 12,
    YELLOW = 14,
    WHITE = 15
};

void setColor(Color color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// ------------------------
// ПРОВЕРКА АДМИНА
// ------------------------
bool isRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {

        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

// ------------------------
// ВРЕМЯ
// ------------------------
std::wstring getTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm localTime;
    localtime_s(&localTime, &t);

    wchar_t buffer[9];
    wcsftime(buffer, 9, L"%H:%M:%S", &localTime);
    return buffer;
}

// ------------------------
// ОЧИСТКА СТРОКИ
// ------------------------
void clearLine() {
    std::wcout << L"\r" << std::wstring(100, L' ') << L"\r";
}

// ------------------------
// ЗАПУСК КОМАНДЫ
// ------------------------
bool runCommand(const std::wstring& command) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    std::wstring cmd = L"cmd.exe /C " + command;

    std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back(L'\0');

    BOOL result = CreateProcessW(
        NULL,
        cmdLine.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!result) {
        setColor(RED);
        std::wcout << L"❌ Ошибка CreateProcess: " << GetLastError() << L"\n";
        setColor(WHITE);
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

// ------------------------
// ПРОВЕРКА ИНТЕРНЕТА
// ------------------------
bool hasInternet() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

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

// ------------------------
// ПЕРЕЗАПУСК WIFI
// ------------------------
void restartWiFi() {
    setColor(RED);
    std::wcout << L"\n[" << getTime() << L"] 🔄 Перезапуск Wi-Fi...\n";

    std::wstring disableCmd = L"netsh interface set interface \"" + std::wstring(INTERFACE_NAME) + L"\" disable";
    std::wstring enableCmd  = L"netsh interface set interface \"" + std::wstring(INTERFACE_NAME) + L"\" enable";

    bool res1 = runCommand(disableCmd);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    bool res2 = runCommand(enableCmd);

    if (res1 && res2) {
        setColor(GREEN);
        std::wcout << L"[" << getTime() << L"] ✅ Wi-Fi восстановлен\n";
    } else {
        setColor(RED);
        std::wcout << L"[" << getTime() << L"] ❌ Ошибка перезапуска Wi-Fi\n";
    }

    setColor(WHITE);
}

// ------------------------
// MAIN
// ------------------------
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    // 🔐 Проверка администратора
    if (!isRunAsAdmin()) {
        setColor(RED);
        std::wcout << L"❌ Программа должна быть запущена от имени администратора!\n";
        setColor(WHITE);

        std::wcout << L"Нажмите Enter для выхода...\n";
        std::cin.get();
        return 1;
    }

    // Winsock init
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::wcout << L"❌ Ошибка WSAStartup\n";
        return 1;
    }

    int fail_count = 0;

    std::wcout << L"==============================\n";
    std::wcout << L"      WiFi Monitor v1.0       \n";
    std::wcout << L"==============================\n\n";

    while (true) {
        clearLine();

        std::wstring time = getTime();

        if (hasInternet()) {
            setColor(GREEN);
            std::wcout << L"[" << time << L"] 🌐 ONLINE  | Интернет работает стабильно   ";
            fail_count = 0;
        } else {
            fail_count++;

            setColor(YELLOW);
            std::wcout << L"[" << time << L"] ⚠️ OFFLINE | Попытка: " << fail_count << L"   ";

            if (fail_count >= RETRY_BEFORE_RESTART) {
                restartWiFi();
                fail_count = 0;

                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

        setColor(WHITE);
        std::this_thread::sleep_for(std::chrono::seconds(SLEEP_INTERVAL));
    }

    WSACleanup();
    return 0;
}