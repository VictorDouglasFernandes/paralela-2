#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include "ThreadPool.h"
#include "FileTransfer.h"
#include <arpa/inet.h>
#include <filesystem>
#include <ctime>

class FileNode {
public:
    FileNode(int serverPort, const std::string& peerIP = "", int peerPort = 0) 
        : serverPort(serverPort), 
          peerIP(peerIP),
          peerPort(peerPort),
          threadPool(5),
          running(true) {}

    void start() {
        // Only start server in a separate thread
        std::thread serverThread(&FileNode::runServer, this);
        serverThread.detach();
    }

    void stop() {
        running = false;
    }

    void runInputHandler() {
        if (!peerIP.empty() && peerPort != 0) {
            listenForInput();
        }
    }

private:
    int serverPort;
    std::string peerIP;
    int peerPort;
    ThreadPool threadPool;
    std::atomic<bool> running;
    std::mutex sendQueueMutex;
    std::queue<std::string> sendQueue;

    int connectToServer() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return -1;
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(peerPort);
        
        if (inet_pton(AF_INET, peerIP.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address" << std::endl;
            return -1;
        }

        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            return -1;
        }

        return sock;
    }

    void handleClient(int clientSocket) {
        char operation;
        if (recv(clientSocket, &operation, 1, 0) <= 0) {
            close(clientSocket);
            return;
        }

        std::string archivePath = "./archive/";  // Change this to your shared folder path
        std::string timestamp = std::to_string(std::time(nullptr));
        
        try {
            std::filesystem::create_directories(archivePath);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create archive directory: " << e.what() << std::endl;
            close(clientSocket);
            return;
        }

        if (operation == 'S') {
            // First receive the filename
            char filename[256];
            if (recv(clientSocket, filename, sizeof(filename), 0) <= 0) {
                close(clientSocket);
                return;
            }
            
            // Create a unique filename in the archive folder
            std::string originalFilename = std::string(filename);
            std::string archiveFilename = archivePath + "received_" + timestamp + "_" + originalFilename;
            
            // Receive file
            if (FileTransfer::receiveFile(clientSocket, archiveFilename)) {
                std::cout << "File received successfully and saved as: " << archiveFilename << "\n";
            } else {
                std::cout << "Failed to receive file\n";
            }
        }

        close(clientSocket);
    }

    void listenForInput() {
        std::string filename;
        while (running) {
            std::cout << "Enter filename to send (or 'quit' to exit): ";
            std::getline(std::cin, filename);
            
            if (filename == "quit") {
                stop();
                break;
            }

            if (!filename.empty()) {
                sendFile(filename);
            }
        }
    }

    bool sendFile(const std::string& filename) {
        int sock = connectToServer();
        if (sock < 0) return false;

        std::cout << "Operation started: Sending file to peer\n";
        send(sock, "S", 1, 0);
        
        // Send the filename first
        send(sock, filename.c_str(), filename.length() + 1, 0);
        
        bool result = FileTransfer::sendFile(sock, filename);
        if (result) {
            std::cout << "File sent successfully\n";
        } else {
            std::cout << "Failed to send file\n";
        }

        close(sock);
        return result;
    }

    void runServer() {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(serverPort);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Failed to bind");
        }

        listen(serverSocket, 10);
        std::cout << "Server listening on port " << serverPort << std::endl;

        while (running) {
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

        close(serverSocket);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <server_port> [peer_ip peer_port]\n";
        return 1;
    }

    int serverPort = std::stoi(argv[1]);
    std::string peerIP = "";
    int peerPort = 0;

    if (argc == 4) {
        peerIP = argv[2];
        peerPort = std::stoi(argv[3]);
    }

    FileNode node(serverPort, peerIP, peerPort);
    
    // Start the server first
    node.start();
    
    // Give the server a moment to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Run the input handler in the main thread
    node.runInputHandler();
    
    node.stop();
    return 0;
} 