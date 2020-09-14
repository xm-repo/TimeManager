
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <time.h>

//#include "date/date.h"
//#include "date/tz.h"
#include "modbus.h"
#include "cxxopts.hpp"

struct term
{
    std::string name;
    std::string port;
    int baud;
    int slave;
    int passw;
};

std::vector<term> readFile(const std::string& path = "config.txt")
{
    std::vector<term> terms;

    std::ifstream f(path);

    if (!f.good()) {
        printf("File %s not found \n", path.c_str());
        return terms;
    }

    int n = (int)std::count(std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>(), '\n') + 1;

    f.seekg(0, f.beg);

    for (int i = 0; i < n; i++)
    {
        try
        {
            term t;
            f >> t.name;
            f >> t.port;
            f >> t.baud;
            f >> t.slave;
            f >> t.passw;

            terms.push_back(t);
        }
        catch (...)
        {
            printf("Read failed: %s %d/%d \n", path.c_str(), i, n);
            break;
        }
    }

    f.close();
    return terms;
}

void printSystemTime()
{
    printf("System time:   ");

    time_t t = time(0);
    struct tm *now = localtime(&t);

    printf("%02d.%02d.%d ", now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);
    printf("%02d:%02d:%02d \n", now->tm_hour, now->tm_min, now->tm_sec);
}

int readTime(const term& t, const bool send = false, const bool verbose = false,
    const bool debug = false)
{
    printf("%s %s[%d] ~ %d[%04d]\n", t.name.c_str(), t.port.c_str(), t.baud, t.slave, t.passw);

    auto port = "\\\\.\\" + t.port;
    modbus_t *ctx = modbus_new_rtu(port.c_str(), t.baud, 'N', 8, 1);
    modbus_set_debug(ctx, debug);
    modbus_rtu_set_serial_mode(ctx, MODBUS_RTU_RS485);
    modbus_set_error_recovery(ctx, MODBUS_ERROR_RECOVERY_LINK);

    auto rc = modbus_set_slave(ctx, t.slave);
    if (rc == -1)
    {
        printf("Invalid slave ID: %d\n", t.slave);
        modbus_free(ctx);
        return 1;
    }

    if (modbus_connect(ctx) == -1)
    {
        printf("Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    modbus_flush(ctx);

    std::unique_ptr<modbus_t, std::function<void(modbus_t*)>> mb(ctx,
        [](modbus_t* ctx) { modbus_flush(ctx); modbus_close(ctx); modbus_free(ctx); });

    modbus_set_response_timeout(mb.get(), 60, 0);
    modbus_set_byte_timeout(mb.get(), 60, 0);

    uint16_t reg[256];
    memset(reg, 0, sizeof(reg));

    auto r = modbus_read_registers(mb.get(), 0, 8, reg);

    if (r == -1)
    {
        printf("Read failed: %s\n", modbus_strerror(errno));
        return 1;
    }

    int year = reg[0];
    int month = reg[1] >> 8;
    int day = reg[1] & 0xFF;
    int hour = reg[2] >> 8;
    int minute = reg[2] & 0xFF;
    int second = reg[3] / 1000;
    int millisecond = reg[3] % 1000;

    int verHigh = reg[7] >> 8;
    int verLow = reg[7] & 0xFF;

    if (verbose)
    {
        printf("PVars.Settings.SIRIUS_ID: %d \n", int(reg[4]));
        printf("PVars.Settings.Configuration: %d \n", int(reg[6]));
        printf("PVars.Settings.Version: %d.%02d \n", verHigh, verLow);
    }

    printSystemTime();
    printf("On-board time: %02d.%02d.%d %02d:%02d:%02d \n", day, month, year, hour, minute, second);

    if (send)
    {
        reg[0] = uint16_t(t.passw);

        std::this_thread::sleep_for(std::chrono::seconds{ 2 });
        auto r = modbus_write_registers(mb.get(), 0x0020, 1, reg);

        if (r == -1)
        {
            printf("Write password failed: %s\n", modbus_strerror(errno));
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::seconds{ 2 });

        time_t t = time(0);
        struct tm *now = localtime(&t);

        reg[0] = uint16_t(now->tm_year + 1900);
        reg[1] = uint16_t((now->tm_mon + 1) << 8)| uint16_t(now->tm_mday);
        reg[2] = uint16_t(now->tm_hour << 8) | uint16_t(now->tm_min);
        reg[3] = uint16_t(now->tm_sec) * 1000;

        r = modbus_write_registers(mb.get(), 0, 4, reg);

        if (r == -1)
        {
            printf("Write time failed: %s\n", modbus_strerror(errno));
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    auto kek = 25u;
    constexpr auto t = 4294967295 - 25 + 1;
    std::cout << (25u - 50);

    cxxopts::Options options("TimeManager", "File format: NAME COM7 9600 1 1628");

    options.allow_unrecognised_options().add_options()
        ("t,timeout", "Timeout", cxxopts::value<int>()->default_value("2"))
        ("d,debug", "Print debug info", cxxopts::value<bool>()->default_value("false"))
        ("v,verbose", "Show version,id,conf", cxxopts::value<bool>()->default_value("false"))
        ("s,send", "Send time?", cxxopts::value<bool>()->default_value("false"))
        ("f,file", "Config file", cxxopts::value<std::string>()->default_value("config.txt"))
        ("h,help", "Help");

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help({ "", "Group" }) << std::endl;
        return 0;
    }

    const bool send = result["send"].as<bool>();
    const int timeout = result["timeout"].as<int>();
    const std::string path = result["file"].as<std::string>();
    const bool verbose = result["verbose"].as<bool>();
    const bool debug = result["debug"].as<bool>();

    std::cout << "timeout: " << timeout << std::endl;
    std::cout << "file: " << path << std::endl;
    std::cout << "verbose: " << std::boolalpha << verbose << std::endl;
    std::cout << "debug: " << std::boolalpha << debug << std::endl;
    std::cout << "send: " << std::boolalpha << send << std::endl << std::endl;

    auto terms = readFile(path);

    for (auto &term : terms)
    {
        try
        {
            readTime(term, send, verbose, debug);
        }
        catch (...)
        {
            printf("Unknown error \n");
        }

        if (&term != &terms.back())
        {
            for (int i = timeout; i > 0; i--)
            {
                printf("\r%d", i);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            printf("\r   \n");
        }
    }

    system("pause");

    return 0;
}
