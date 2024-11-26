#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <string>
#include "ThreadPool.h"
#include "FileTransfer.h"

class FileServer {
public:
    FileServer(int port, int maxConnections = 5) 
        : port(port), 
          threadPool(maxConnections),
          maxConnections(maxConnections) {
        
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Failed to bind");
        }
    }

    void start() {
        listen(serverSocket, 10);
        std::cout << "Server listening on port " << port << std::endl;

        while (true) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

            if (clientSocket < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            threadPool.enqueue([this, clientSocket]() {
                handleClient(clientSocket);
            });
        }
    }

private:
    int serverSocket;
    int port;
    ThreadPool threadPool;
    int maxConnections;

    void handleClient(int clientSocket) {
        char command[2];
        recv(clientSocket, command, 1, 0);
        command[1] = '\0';

        std::string timestamp = std::to_string(std::time(nullptr));
        std::string archivePath = "./archive/";
        
        try {
            std::filesystem::create_directories(archivePath);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create archive directory: " << e.what() << std::endl;
            close(clientSocket);
            return;
        }

        if (command[0] == 'S') {
            std::cout << "Operation started: Receiving file from client\n";
            std::string filename = archivePath + "received_" + timestamp + ".txt";
            FileTransfer::receiveFile(clientSocket, filename);
            std::cout << "File saved as: " << filename << "\n";
        } 
        else if (command[0] == 'R') {
            std::cout << "Operation started: Sending file to client\n";
            FileTransfer::sendFile(clientSocket, "file_to_send.txt");
            std::cout << "File sent successfully\n";
        }

        close(clientSocket);
    }
};

int main(int argc, char* argv[]) {
    FileServer server(8080);
    server.start();
    return 0;
}