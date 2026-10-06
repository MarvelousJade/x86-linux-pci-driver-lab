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
#include <atomic>
#include <thread>
#include <vector>

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
static uint32_t expected_factorial(uint32_t n) {
    uint32_t result = 1;
    for (uint32_t i = 2; i <= n; ++i) result *= i;
    return result;
}
static void factorial(Device& d, uint32_t n) {
    edu_value v{n, 0};
    check(ioctl(d.fd, EDU_IOC_FACTORIAL, &v) == 0, "factorial ioctl failed");
    check(v.result == expected_factorial(n), "factorial mismatch");
}
static void computation_test(Device& d) {
    for (uint32_t n : {13U, UINT32_MAX}) {
        edu_value v{n, 0};
        check(ioctl(d.fd, EDU_IOC_FACTORIAL, &v) == -1 && errno == EINVAL,
              "out-of-range factorial must return EINVAL");
    }
    for (unsigned repeat = 0; repeat < 10; ++repeat)
        for (uint32_t n = 0; n <= EDU_MAX_FACTORIAL; ++n) factorial(d, n);
    puts("PASS factorial 0..12, invalid 13/u32-max, 130 repeated requests");
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> callers;
    for (unsigned t = 0; t < 4; ++t) callers.emplace_back([&, t] {
        try {
            Device own;
            ++ready;
            while (!go.load()) std::this_thread::yield();
            for (unsigned i = 0; i < 25; ++i) factorial(own, (i + t) % 13);
        } catch (...) { ++failures; ++ready; }
    });
    while (ready.load() < 4) std::this_thread::yield();
    go = true;
    for (auto& thread : callers) thread.join();
    check(!failures.load(), "concurrent caller failed");
    puts("PASS four concurrent handles, 100 factorial requests");
}
int main(int argc, char** argv) {
    try {
        static_assert(sizeof(edu_value) == 8, "ABI size");
        if (argc == 2 && !strcmp(argv[1], "selftest")) {
            Device d;
            liveness_test(d);
            computation_test(d);
        } else if (argc == 3 && !strcmp(argv[1], "live")) {
            Device d;
            live(d, number(argv[2]));
        } else if (argc == 3 && !strcmp(argv[1], "factorial")) {
            Device d;
            const auto n = number(argv[2]);
            check(n <= EDU_MAX_FACTORIAL, "factorial range is 0..12");
            factorial(d, n);
            printf("FACTORIAL %u -> %u verified\n", n, expected_factorial(n));
        } else {
            fputs("usage: edu-test selftest | live <u32> | factorial <0..12>\n", stderr);
            return 2;
        }
        puts("device closed");
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s (errno=%d: %s)\n", e.what(), errno, strerror(errno));
        return 1;
    }
}
