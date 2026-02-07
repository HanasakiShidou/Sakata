#include "HelloWorldClient.h"
#include "HelloWorldServer.h"
#include <cstdio>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <cassert>
#include <iostream>
#include <thread>

struct Signal {
    private:
        std::atomic<bool> signal_{false};
    public:
        void inline release(bool val) {
            signal_.store(val, std::memory_order_release);
        }
        bool inline acquire() {
            return signal_.load(std::memory_order_acquire);
        }
};

Signal terminate;

static uint8_t param_buffer[1024] = {0};
Signal param_buffer_signal;

static uint8_t result_buffer[1024] = {0};
Signal result_buffer_signal;

class HelloWorldServerImplement: public HelloWorldServer {
    public:
    bool HelloWorld(int32_t param) override {
        //std::cout << "Server: " << "Hello World from client, param " << param << std::endl;
        return true;
    }
    int32_t addNumbers(int32_t a, int32_t b) override {
        //std::cout << "Server: " << "addNumbers from client, a " << a << " b " << b << std::endl;
        return a + b;
    }
    void processData(uint8_t data[32], uint16_t size) override {
        //std::cout << "Server: " << "processData from clinet, data len " << size << std::endl;
    }
    int64_t getStatus() override {
        //std::cout << "Server: " << "get Status from client ret 1" << std::endl;
        return 1;
    }
    ~HelloWorldServerImplement() {}
} serverImpl;

void deamonThreadWork(void) {
    while (terminate.acquire() != true) {
        while ((param_buffer_signal.acquire() == true) && (result_buffer_signal.acquire() == false)) {
            int result_size = 0;
            serverImpl.handle_request(param_buffer, 2, result_buffer, result_size);
            std::memset(param_buffer, '\0', sizeof(param_buffer));
            param_buffer_signal.release(false);
            result_buffer_signal.release(true);
        }
    }
}

int main() {

    HelloWorldClient client(
        [] (const uint8_t* buffer, int size) -> bool {
            assert(size < sizeof(param_buffer));
            while (param_buffer_signal.acquire() != false);
            std::memcpy(param_buffer, buffer, size);
            param_buffer_signal.release(true);
            return true;
        },
        [] (uint8_t* buffer, int size) -> bool {
            assert(size < sizeof(result_buffer));
            while (result_buffer_signal.acquire() != true);
            std::memcpy(buffer, result_buffer, size);
            std::memset(result_buffer, '\0', sizeof(result_buffer));
            result_buffer_signal.release(false);
            return true;
        }
    );

    std::thread deamonThread(deamonThreadWork);

    for (int i = 0; i < 1000000; i++) {
        auto helloworldret = client.HelloWorld(5);
        //std::cout << "Client: call HelloWorld returns " << helloworldret << std::endl;
        auto addnumret = client.addNumbers(1, 2);
        //std::cout << "Client: call addNumbers(1, 2) returns " << addnumret << std::endl;
        uint8_t test_data[32] = {0};
        client.processData(test_data, 32);
        //std::cout << "Client: call processData(test_data,32) " << std::endl;
        auto status = client.getStatus();
        //std::cout << "Client: call getStatus() returns "<< status << std::endl;
    }
    
    terminate.release(true);
    deamonThread.join();
    return 0;
}