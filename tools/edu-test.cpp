// SPDX-License-Identifier: GPL-2.0-only
#include "edu_lab.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

class Device {
public:
    int fd;
    Device() : fd(open("/dev/edu_lab", O_RDWR | O_CLOEXEC)) {
        if (fd < 0) throw std::runtime_error(std::string("open: ") + strerror(errno));
    }
    ~Device() { close(fd); }
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
};
static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static uint32_t number(const char* text) {
    check(*text && *text != '-', "expected unsigned 32-bit number");
    char* end;
    errno = 0;
    auto n = strtoull(text, &end, 0);
    check(!errno && !*end && n <= UINT32_MAX, "invalid 32-bit number");
    return static_cast<uint32_t>(n);
}
static void live(Device& d, uint32_t input) {
    edu_value v{input, 0};
    check(ioctl(d.fd, EDU_IOC_LIVE, &v) == 0, "liveness ioctl failed");
    check(v.result == ~input, "register inversion mismatch");
    printf("LIVE %08x -> %08x verified\n", input, v.result);
}
static void liveness_test(Device& d) {
    for (uint32_t n : {0U, UINT32_MAX, 0x12345678U, 0xaaaaaaaaU}) live(d, n);
    check(ioctl(d.fd, _IO('E', 99), nullptr) == -1 && errno == ENOTTY,
          "unknown ioctl must return ENOTTY");
    check(ioctl(d.fd, _IOR('E', 1, uint32_t), nullptr) == -1 && errno == ENOTTY,
          "wrong ioctl size/direction must return ENOTTY");
    check(ioctl(d.fd, EDU_IOC_LIVE, nullptr) == -1 && errno == EFAULT,
          "null buffer must return EFAULT");
    puts("PASS liveness and invalid interface requests");
}
int main(int argc, char** argv) {
    try {
        static_assert(sizeof(edu_value) == 8, "ABI size");
        if (argc == 2 && !strcmp(argv[1], "selftest")) {
            Device d;
            liveness_test(d);
        } else if (argc == 3 && !strcmp(argv[1], "live")) {
            Device d;
            live(d, number(argv[2]));
        } else {
            fputs("usage: edu-test selftest | live <u32>\n", stderr);
            return 2;
        }
        puts("device closed");
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s (errno=%d: %s)\n", e.what(), errno, strerror(errno));
        return 1;
    }
}
