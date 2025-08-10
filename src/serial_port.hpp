// serial_port.hpp
#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <string>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <system_error>

class SerialPort {
public:
    SerialPort() : fd_(-1) {}
    
    ~SerialPort() {
        close();
    }

    bool open(const std::string& device_path) {
        fd_ = ::open(device_path.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd_ < 0) return false;

        struct termios options;
        if (tcgetattr(fd_, &options) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        // 配置为原始模式
        cfmakeraw(&options);
        options.c_cflag |= (CLOCAL | CREAD);
        options.c_cflag &= ~CRTSCTS;
        options.c_cc[VMIN] = 1;  // 默认阻塞模式

        if (tcsetattr(fd_, TCSANOW, &options) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }

        return true;
    }

    void close() {
        if (is_open()) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool is_open() const { return fd_ >= 0; }

    void set_blocking(bool blocking) {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (blocking) {
            flags &= ~O_NONBLOCK;
            // 设置阻塞读取参数
            struct termios options;
            tcgetattr(fd_, &options);
            options.c_cc[VMIN] = 1;
            options.c_cc[VTIME] = 0;
            tcsetattr(fd_, TCSANOW, &options);
        } else {
            flags |= O_NONBLOCK;
            // 设置非阻塞参数
            struct termios options;
            tcgetattr(fd_, &options);
            options.c_cc[VMIN] = 0;
            options.c_cc[VTIME] = 0;
            tcsetattr(fd_, TCSANOW, &options);
        }
        fcntl(fd_, F_SETFL, flags);
    }

    int read(uint8_t* buffer, size_t size) {
        return ::read(fd_, buffer, size);
    }

    int write(const uint8_t* buffer, size_t size) {
        int bytes_written = ::write(fd_, buffer, size);
        if (bytes_written > 0) {
            tcdrain(fd_);  // 等待数据完全发送
        }
        return bytes_written;
    }

private:
    int fd_;
};

#endif // SERIAL_PORT_HPP