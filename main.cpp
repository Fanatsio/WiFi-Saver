#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "ws2_32.lib")

const wchar_t* INTERFACE_NAME = L"Беспроводная сеть";
const char* CHECK_IP = "8.8.8.8";
const int CHECK_PORT = 53;
const int CHECK_TIMEOUT = 3;
const int RETRY_BEFORE_RESTART = 2;
const int SLEEP_INTERVAL = 5;

bool hasInternet() {
    WSADATA wsaData;
    SOCKET sock;
    sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    server.sin_addr.s_addr = inet_addr(CHECK_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(CHECK_PORT);

    DWORD timeout = CHECK_TIMEOUT * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    int result = connect(sock, (sockaddr*)&server, sizeof(server));

    closesocket(sock);
    WSACleanup();

    return result == 0;
}

void restartWiFi() {
    std::wcout << L"🔄 Перезапуск Wi-Fi...\n";

    std::wstring disableCmd = L"netsh interface set interface \"";
    disableCmd += INTERFACE_NAME;
    disableCmd += L"\" disable";

    std::wstring enableCmd = L"netsh interface set interface \"";
    enableCmd += INTERFACE_NAME;
    enableCmd += L"\" enable";

    int res1 = _wsystem(disableCmd.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int res2 = _wsystem(enableCmd.c_str());

    if (res1 == 0 && res2 == 0) {
        std::wcout << L"✅ Wi-Fi перезапущен\n";
    } else {
        std::wcout << L"❌ Ошибка при перезапуске\n";
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    int fail_count = 0;

    while (true) {
        if (hasInternet()) {
            std::wcout << L"🌐 Интернет есть\n";
            fail_count = 0;
        } else {
            fail_count++;
            std::wcout << L"⚠️ Нет интернета (" << fail_count << L")\n";

            if (fail_count >= RETRY_BEFORE_RESTART) {
                restartWiFi();
                fail_count = 0;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(SLEEP_INTERVAL));
    }

    return 0;
}