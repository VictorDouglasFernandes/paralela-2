#pragma once
#include <string>
#include <cstdint>
#include <atomic>
#include <mutex>

class FileTransfer {
public:
    static constexpr int MAX_RETRIES = 3;
    static constexpr int RETRY_DELAY_MS = 1000;
    static constexpr int BASE_TRANSFER_RATE = 20; // 1 KB/s base rate

    static bool sendFile(int socket, const std::string& filename);
    static bool receiveFile(int socket, const std::string& filename, bool printContent = false);
    static void printFileContent(const std::string& filename);

private:
    static std::atomic<int> activeTransfers;
    static std::mutex transferMutex;

    static bool sendChunk(int socket, const char* data, size_t size);
    static bool receiveChunk(int socket, char* data, size_t size);
    static size_t calculateChunkSize();
}; 