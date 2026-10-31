#include "httplib.h"
#include "car.h"
#include "hashtable.h"
#include "avl.h"
#include "trie.h"
#include "storage.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

using namespace std;

HashTable table;               // all cars, keyed by id
AVLTree priceTree;             // (price, id) pairs, for price range queries
Trie nameTrie;                 // brand/model words, for search-as-you-type
int nextId = 1;                // id given to the next car added
mutex mtx;                     // the server handles requests on several threads
const string FILE_NAME = "cars.csv";

string jsonEscape(const string& s) {
    string out;
    for (char ch : s) {
        if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
        else if (ch == '\n' || ch == '\r') out += ' ';
        else out += ch;
    }
    return out;
}

string carToJson(const Car& c) {
    return "{\"id\":" + to_string(c.id) +
           ",\"brand\":\"" + jsonEscape(c.brand) + "\"" +
           ",\"model\":\"" + jsonEscape(c.model) + "\"" +
           ",\"year\":" + to_string(c.year) +
           ",\"price\":" + to_string(c.price) +
           ",\"km\":" + to_string(c.km) +
           ",\"fuel\":\"" + jsonEscape(c.fuel) + "\"" +
           ",\"transmission\":\"" + jsonEscape(c.transmission) + "\"" +
           ",\"engineCC\":" + to_string(c.engineCC) +
           ",\"color\":\"" + jsonEscape(c.color) + "\"}";
}

// Reply for add/delete: tells the page what happened and which DSA was used
string reply(bool ok, const string& msg, const string& ds, const string& complexity) {
    return string("{\"ok\":") + (ok ? "true" : "false") +
           ",\"msg\":\"" + jsonEscape(msg) + "\"" +
           ",\"ds\":\"" + jsonEscape(ds) + "\"" +
           ",\"complexity\":\"" + jsonEscape(complexity) + "\"" +
           ",\"total\":" + to_string(table.size()) + "}";
}

// Reads optional ?min=&max= from the request. Returns false (and sets err) if invalid.
bool parseRange(const httplib::Request& req, long long& lo, long long& hi, string& err) {
    lo = 0;
    hi = LLONG_MAX;
    try {
        string a = req.get_param_value("min");
        string b = req.get_param_value("max");
        if (!a.empty()) lo = stoll(a);
        if (!b.empty()) hi = stoll(b);
    } catch (...) {
        err = "Min and max price must be numbers";
        return false;
    }
    if (lo > hi) {
        err = "Min price cannot be greater than max price";
        return false;
    }
    return true;
}

int main() {
    nextId = loadCars(FILE_NAME, table) + 1;
    for (const Car* c : table.all()) {                                    // build the AVL tree and the trie
        priceTree.insert(c->price, c->id);
        nameTrie.addCar(c->brand, c->model, c->id);
    }

    httplib::Server svr;
    svr.set_mount_point("/", "./web");

    // List all cars (sorted by id for now; real sorting comes next)
    svr.Get("/api/cars", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        vector<const Car*> cars = table.all();
        sort(cars.begin(), cars.end(),
             [](const Car* a, const Car* b) { return a->id < b->id; });

        string out = "[";
        for (size_t i = 0; i < cars.size(); i++) {
            if (i) out += ",";
            out += carToJson(*cars[i]);
        }
        out += "]";
        res.set_content(out, "application/json");
    });

    // Add a car
    svr.Post("/api/add", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        Car c;
        try {
            c.brand = sanitize(req.get_param_value("brand"));
            c.model = sanitize(req.get_param_value("model"));
            c.fuel = sanitize(req.get_param_value("fuel"));
            c.transmission = sanitize(req.get_param_value("transmission"));
            c.color = sanitize(req.get_param_value("color"));
            c.engineCC = stoi(req.get_param_value("engineCC"));
            c.year = stoi(req.get_param_value("year"));
            c.price = stoll(req.get_param_value("price"));
            c.km = stoi(req.get_param_value("km"));
        } catch (...) {
            res.set_content(reply(false, "Year, price, km and engine capacity must be numbers", "", ""), "application/json");
            return;
        }

        if (c.brand.empty() || c.model.empty() || c.fuel.empty() || c.transmission.empty() || c.color.empty()) {
            res.set_content(reply(false, "Please fill in all fields", "", ""), "application/json");
            return;
        }
        if (c.year < 1980 || c.year > 2027 || c.price <= 0 || c.km < 0) {
            res.set_content(reply(false, "Check year (1980-2027), price (> 0) and km (>= 0)", "", ""), "application/json");
            return;
        }
        if (c.engineCC < 0 || c.engineCC > 8000) {
            res.set_content(reply(false, "Engine capacity must be between 0 and 8000 cc (use 0 for electric)", "", ""), "application/json");
            return;
        }

        c.id = nextId++;
        table.insert(c);
        priceTree.insert(c.price, c.id);
        nameTrie.addCar(c.brand, c.model, c.id);
        saveCars(FILE_NAME, table);
        res.set_content(reply(true, "Car added with ID " + to_string(c.id),
                              "Hash Table (chaining)", "insert: O(1) average"),
                        "application/json");
    });

    // Delete a car by id
    svr.Post("/api/delete", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        int id = 0;
        try {
            id = stoi(req.get_param_value("id"));
        } catch (...) {
            res.set_content(reply(false, "Invalid car ID", "", ""), "application/json");
            return;
        }

        Car old;                                   // copy the car's details before it is deleted
        if (Car* existing = table.find(id)) old = *existing;
        if (table.remove(id)) {
            priceTree.remove(old.price, id);
            nameTrie.removeCar(old.brand, old.model, id);
            saveCars(FILE_NAME, table);
            res.set_content(reply(true, "Car " + to_string(id) + " deleted",
                                  "Hash Table (chaining)", "delete: O(1) average"),
                            "application/json");
        } else {
            res.set_content(reply(false, "No car with ID " + to_string(id), "Hash Table (chaining)", "lookup: O(1) average"),
                            "application/json");
        }
    });

    // Cars in a price range, cheapest first (uses the AVL tree)
    svr.Get("/api/range", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        long long lo, hi;
        string err;
        if (!parseRange(req, lo, hi, err)) {
            res.set_content(reply(false, err, "", ""), "application/json");
            return;
        }

        vector<int> ids = priceTree.rangeIds(lo, hi);   // ids in ascending price order
        string out = "[";
        bool first = true;
        for (int id : ids) {
            Car* c = table.find(id);                    // full details from the hash table
            if (!c) continue;
            if (!first) out += ",";
            first = false;
            out += carToJson(*c);
        }
        out += "]";
        res.set_content(out, "application/json");
    });

    // Search by brand/model text (uses the trie), optionally combined with a price range
    svr.Get("/api/search", [](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        long long lo, hi;
        string err;
        if (!parseRange(req, lo, hi, err)) {
            res.set_content(reply(false, err, "", ""), "application/json");
            return;
        }

        vector<int> ids = nameTrie.search(req.get_param_value("q"));   // ids of matching cars
        string out = "[";
        bool first = true;
        for (int id : ids) {
            Car* c = table.find(id);                                   // details from the hash table
            if (!c || c->price < lo || c->price > hi) continue;        // apply the price range if given
            if (!first) out += ",";
            first = false;
            out += carToJson(*c);
        }
        out += "]";
        res.set_content(out, "application/json");
    });

    system("start http://localhost:8080");   // Windows. Mac: "open", Linux: "xdg-open"
    svr.listen("localhost", 8080);
}
