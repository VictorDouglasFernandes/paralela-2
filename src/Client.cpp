#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include "FileTransfer.h"

class FileClient {
public:
    FileClient(const std::string& serverIP, int port) 
        : serverIP(serverIP), port(port) {}

    bool sendFile(const std::string& filename) {
        int sock = connectToServer();
        if (sock < 0) return false;

        std::cout << "Operation started: Sending file to server\n";
        send(sock, "S", 1, 0);
        
        bool result = FileTransfer::sendFile(sock, filename);
        if (result) {
            std::cout << "File sent successfully\n";
        } else {
            std::cout << "Failed to send file\n";
        }

        close(sock);
        return result;
    }

    bool receiveFile(const std::string& filename) {
        int sock = connectToServer();
        if (sock < 0) return false;

        std::cout << "Operation started: Receiving file from server\n";
        send(sock, "R", 1, 0);
        
        bool result = FileTransfer::receiveFile(sock, filename, true);  // true to print content
        if (result) {
            std::cout << "File received successfully\n";
        } else {
            std::cout << "Failed to receive file\n";
        }

        close(sock);
        return result;
    }

private:
    std::string serverIP;
    int port;

    int connectToServer() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return -1;

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            close(sock);
            return -1;
        }

        return sock;
    }
}; 

int main(int argc, char* argv[]) {
    FileClient client("127.0.0.1", 8080);
    client.sendFile("file_to_send.txt");
    client.receiveFile("received_file.txt");
    return 0;
}