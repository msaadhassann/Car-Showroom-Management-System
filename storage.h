#pragma once
#include "car.h"
#include "hashtable.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Removes characters that would break the CSV file, and trims spaces
inline std::string sanitize(std::string s) {
    for (char& ch : s)
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') ch = ' ';
    size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(' ');
    return s.substr(a, b - a + 1);
}

inline std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> parts;
    std::string cur;
    std::stringstream ss(line);
    while (std::getline(ss, cur, ',')) parts.push_back(cur);
    return parts;
}

// File format: id,brand,model,year,price,km,fuel,transmission,engineCC,color
// (older 8-column files still load; engine and color get default values)
// Returns the largest id found (so new cars can continue from there)
inline int loadCars(const std::string& path, HashTable& table) {
    std::ifstream fin(path);
    if (!fin.is_open()) return 0;

    std::string line;
    int maxId = 0;
    bool header = true;
    while (std::getline(fin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (header) { header = false; continue; }
        if (line.empty()) continue;

        std::vector<std::string> p = splitLine(line);
        if (p.size() < 8) continue;
        try {
            Car c;
            c.id = std::stoi(p[0]);
            c.brand = p[1];
            c.model = p[2];
            c.year = std::stoi(p[3]);
            c.price = std::stoll(p[4]);
            c.km = std::stoi(p[5]);
            c.fuel = p[6];
            c.transmission = p[7];
            c.engineCC = (p.size() > 8) ? std::stoi(p[8]) : 0;
            c.color = (p.size() > 9) ? p[9] : "Unknown";
            if (table.insert(c)) maxId = std::max(maxId, c.id);
        } catch (...) {
            // skip bad rows
        }
    }
    return maxId;
}

inline void saveCars(const std::string& path, const HashTable& table) {
    std::vector<const Car*> cars = table.all();
    std::sort(cars.begin(), cars.end(),
              [](const Car* a, const Car* b) { return a->id < b->id; });

    std::ofstream fout(path);
    if (!fout.is_open()) return;
    fout << "id,brand,model,year,price,km,fuel,transmission,engineCC,color\n";
    for (const Car* c : cars) {
        fout << c->id << ',' << c->brand << ',' << c->model << ',' << c->year << ','
             << c->price << ',' << c->km << ',' << c->fuel << ',' << c->transmission << ',' << c->engineCC << ',' << c->color << '\n';
    }
}
