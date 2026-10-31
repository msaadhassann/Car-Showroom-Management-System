#pragma once
#include <string>

struct Car {
    int id = 0;
    std::string brand;
    std::string model;
    int year = 0;
    long long price = 0;
    int km = 0;
    std::string fuel;
    std::string transmission;
    int engineCC = 0;          // engine capacity in cc (0 for electric)
    std::string color;
};
