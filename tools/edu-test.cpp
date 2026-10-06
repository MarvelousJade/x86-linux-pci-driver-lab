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
#include <chrono>
#include <csignal>
#include <pthread.h>
#include <fstream>
#include <sstream>
#include <sys/syscall.h>

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
static void set_hold(bool held) {
    const char* path = "/sys/module/edu_lab/parameters/hold_completion";
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    check(fd >= 0, "open hold_completion test hook failed");
    const auto written = write(fd, held ? "1" : "0", 1);
    close(fd);
    check(written == 1, "write hold_completion test hook failed");
}
class HeldCompletion {
public:
    HeldCompletion() { set_hold(true); }
    ~HeldCompletion() { try { set_hold(false); } catch (...) {} }
};
static void recover(Device& d) {
    set_hold(false);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    for (;;) {
        edu_value v{12, 0};
        const int rc = ioctl(d.fd, EDU_IOC_FACTORIAL, &v);
        if (rc == 0) {
            check(v.result == expected_factorial(12), "late old result returned as new request");
            break;
        }
        check(errno == EBUSY, "unexpected recovery error");
        check(std::chrono::steady_clock::now() < deadline, "recovery deadline exceeded");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    factorial(d, 7);
}
static uint64_t irq_count();
static void signal_handler(int) {}
static void recovery_test(Device& d) {
    HeldCompletion hook;
    edu_value old{8, 0};
    const auto start = std::chrono::steady_clock::now();
    check(ioctl(d.fd, EDU_IOC_FACTORIAL, &old) == -1 && errno == ETIMEDOUT,
          "held real IRQ completion must time out");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    check(ms >= 900 && ms < 3000, "timeout wait outside test budget");
    edu_value next{12, 0};
    check(ioctl(d.fd, EDU_IOC_FACTORIAL, &next) == -1 && errno == EBUSY,
          "new request must be refused while old completion is held");
    live(d, 0xfeedbeef);
    recover(d);
    printf("PASS timeout (%lld ms), quarantine, late completion and exact-result recovery\n",
           static_cast<long long>(ms));

    // Establish a real in-flight request before sending a signal. Otherwise
    // EINTR could test queue acquisition rather than completion recovery.
    set_hold(true);
    const auto before = irq_count();
    struct sigaction action{};
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    check(sigaction(SIGUSR1, &action, nullptr) == 0, "sigaction failed");
    std::atomic<bool> finished{false};
    int rc = 0, error = 0;
    std::thread caller([&] {
        edu_value v{9, 0};
        rc = ioctl(d.fd, EDU_IOC_FACTORIAL, &v);
        error = errno;
        finished = true;
    });
    try {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
        bool observed = false;
        while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
            if (irq_count() > before) { observed = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(observed && !finished.load(), "signal test needs an active wait with real IRQ");
        // No SA_RESTART; send only to the dedicated ioctl caller.
        while (!finished.load()) {
            pthread_kill(caller.native_handle(), SIGUSR1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (...) {
        try { set_hold(false); } catch (...) {}
        caller.join();
        throw;
    }
    caller.join();
    check(rc == -1 && error == EINTR, "signal must interrupt completion wait");
    check(ioctl(d.fd, EDU_IOC_FACTORIAL, &next) == -1 && errno == EBUSY,
          "interrupted in-flight work must retain ownership");
    recover(d);
    puts("PASS signal interruption followed by recovery");
}
static uint64_t irq_count() {
    std::ifstream file("/proc/interrupts");
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("edu_lab") == std::string::npos) continue;
        const auto colon = line.find(':');
        check(colon != std::string::npos, "bad interrupt line");
        std::istringstream columns(line.substr(colon + 1));
        uint64_t value, sum = 0;
        while (columns >> value) sum += value;
        return sum;
    }
    throw std::runtime_error("EDU missing from /proc/interrupts");
}
static void detach(const char* bdf, bool remove) {
    // Only used in the disposable guest, with the BDF selected by guest-init.
    const std::string path = remove ? std::string("/sys/bus/pci/devices/") + bdf + "/remove"
                                   : "/sys/bus/pci/drivers/edu_lab/unbind";
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    check(fd >= 0, "open detach sysfs failed");
    const std::string value = remove ? "1" : bdf;
    const auto rc = write(fd, value.data(), value.size());
    close(fd);
    check(rc == static_cast<ssize_t>(value.size()), "detach sysfs write failed");
}
static void lifecycle_test(Device& d, const char* bdf, bool remove) {
    check(syscall(SYS_delete_module, "edu_lab", O_NONBLOCK) == -1 &&
          (errno == EWOULDBLOCK || errno == EBUSY), "open fd must pin module");
    HeldCompletion hook;
    const auto before = irq_count();
    std::atomic<bool> finished{false};
    int result = 0, error = 0;
    std::thread caller([&] {
        edu_value v{11, 0};
        result = ioctl(d.fd, EDU_IOC_FACTORIAL, &v);
        error = errno;
        finished = true;
    });
    bool irq_observed = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
    try {
        while (!finished.load() && std::chrono::steady_clock::now() < deadline) {
            if (irq_count() > before) { irq_observed = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(irq_observed && !finished.load(), "could not establish active wait with real IRQ");
        detach(bdf, remove);
    } catch (...) {
        try { set_hold(false); } catch (...) {}
        caller.join(); // don't let a joinable thread turn a diagnostic into terminate()
        throw;
    }
    caller.join();
    check(result == -1 && error == ETIMEDOUT, "in-flight request must finish its bounded wait");
    for (const auto cmd : {EDU_IOC_LIVE, EDU_IOC_FACTORIAL, static_cast<unsigned long>(_IO('E', 99))}) {
        edu_value v{5, 0};
        check(ioctl(d.fd, cmd, &v) == -1 && errno == ENODEV,
              "detached handle must return ENODEV without MMIO");
    }
    const int new_fd = open("/dev/edu_lab", O_RDWR);
    if (new_fd >= 0) close(new_fd);
    check(new_fd == -1 && errno == ENOENT, "detached misc node must disappear");
    printf("PASS %s with active wait, queued notifier, open fd; stale ioctls ENODEV\n",
           remove ? "PCI removal" : "unbind");
}
int main(int argc, char** argv) {
    try {
        static_assert(sizeof(edu_value) == 8, "ABI size");
        if (argc == 2 && !strcmp(argv[1], "selftest")) {
            Device d;
            liveness_test(d);
            computation_test(d);
        } else if (argc == 2 && !strcmp(argv[1], "recovery")) {
            Device d;
            recovery_test(d);
        } else if (argc == 4 && !strcmp(argv[1], "lifecycle")) {
            check(!strcmp(argv[3], "unbind") || !strcmp(argv[3], "remove"), "detach action");
            Device d;
            lifecycle_test(d, argv[2], !strcmp(argv[3], "remove"));
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
            fputs("usage: edu-test selftest | recovery | lifecycle <BDF> <unbind|remove> | live <u32> | factorial <0..12>\n", stderr);
            return 2;
        }
        puts("device closed");
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "FAIL: %s (errno=%d: %s)\n", e.what(), errno, strerror(errno));
        return 1;
    }
}
