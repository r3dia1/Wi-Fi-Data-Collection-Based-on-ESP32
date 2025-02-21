#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <chrono> // 用於計時
using namespace std;

mutex file_mutex;
char* location;

void save_rssi_to_csv(const string& rssi_data, const string& filename) {
    vector<int> rssi_values;
    stringstream ss(rssi_data);
    string token;

    // 分割字符串
    while (getline(ss, token, '-')) {
        try {
            if (!token.empty()) {
                rssi_values.push_back(stoi("-" + token));
            }
        } catch (exception& e) {
            cerr << "Error parsing RSSI value: " << e.what() << endl;
            return;
        }
    }

    if (rssi_values.size() == 3) {
        lock_guard<mutex> guard(file_mutex); // 鎖定文件操作
        ofstream csvfile(filename, ios_base::app);

        static bool is_first_entry = true;
        if (is_first_entry) {
            csvfile << "Router 1 RSSI,Router 2 RSSI,Router 3 RSSI,Location: " << location << endl;
            is_first_entry = false;
        }

        csvfile << rssi_values[0] << "," << rssi_values[1] << "," << rssi_values[2] << "\n";
    } else {
        cerr << "Error: Incorrect RSSI data format." << endl;
    }
}

void handle_client(SOCKET clientSocket) {
    while (true) {
        char receiveBuffer[128];
        int rbyteCount = recv(clientSocket, receiveBuffer, sizeof(receiveBuffer) - 1, 0);

        if (rbyteCount <= 0) {
            if (rbyteCount == 0) {
                cout << "Client disconnected." << endl;
            } else {
                cerr << "Receive error: " << WSAGetLastError() << endl;
            }
            closesocket(clientSocket);
            break;
        }

        receiveBuffer[rbyteCount] = '\0'; // 添加字符串結束符
        cout << "Received RSSI: " << receiveBuffer << endl;
        save_rssi_to_csv(receiveBuffer, "rssi_data.csv");
    }
}

int main(int argc, char *argv[]) {
    location = argv[1];
    // cout << location << endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Winsock initialization failed!" << endl;
        return -1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in service = {};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.82.216");
    service.sin_port = htons(3333);

    if (bind(serverSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    cout << "Server listening on port 3333..." << endl;

    // 設定超時機制
    auto start_time = chrono::steady_clock::now();
    const chrono::seconds timeout_duration(60); // 超時 30 秒

    while (true) {
        // 使用 non-blocking 模式的 accept，設置為超時
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        timeval timeout;
        timeout.tv_sec = 1;  // 每次檢查 1 秒
        timeout.tv_usec = 0;

        int select_result = select(0, &readfds, NULL, NULL, &timeout);

        if (select_result > 0 && FD_ISSET(serverSocket, &readfds)) {
            SOCKET clientSocket = accept(serverSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                cerr << "Accept failed: " << WSAGetLastError() << endl;
                break;
            }
            cout << "New client connected!" << endl;

            thread client_thread(handle_client, clientSocket);
            client_thread.detach(); // 分離執行緒

            // 重置計時器
            start_time = chrono::steady_clock::now();
        }

        // 檢查是否超時
        auto elapsed_time = chrono::steady_clock::now() - start_time;
        if (elapsed_time > timeout_duration) {
            cout << "No new connections for 30 seconds. Exiting..." << endl;
            break;
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
