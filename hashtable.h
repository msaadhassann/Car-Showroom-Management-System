#pragma once
#include "car.h"
#include <vector>

// Hash table with separate chaining (linked lists) and automatic rehashing.
// Key: car id.  Average time: insert / find / remove = O(1).
class HashTable {
    struct Node {
        Car car;
        Node* next;
    };

    Node** buckets;
    int cap;     // number of buckets
    int count;   // number of cars stored

    int indexFor(int id, int capacity) const {
        return ((id % capacity) + capacity) % capacity;
    }

    // Grow the table when it gets too full (load factor > 0.75)
    void rehash() {
        int newCap = cap * 2 + 1;
        Node** newBuckets = new Node*[newCap]();
        for (int i = 0; i < cap; i++) {
            Node* cur = buckets[i];
            while (cur) {
                Node* next = cur->next;
                int idx = indexFor(cur->car.id, newCap);
                cur->next = newBuckets[idx];
                newBuckets[idx] = cur;
                cur = next;
            }
        }
        delete[] buckets;
        buckets = newBuckets;
        cap = newCap;
    }

public:
    HashTable(int initialCap = 101) : cap(initialCap), count(0) {
        buckets = new Node*[cap]();
    }

    ~HashTable() {
        clear();
        delete[] buckets;
    }

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    void clear() {
        for (int i = 0; i < cap; i++) {
            Node* cur = buckets[i];
            while (cur) {
                Node* next = cur->next;
                delete cur;
                cur = next;
            }
            buckets[i] = nullptr;
        }
        count = 0;
    }

    int size() const { return count; }

    // Returns false if a car with this id already exists
    bool insert(const Car& c) {
        if (find(c.id)) return false;
        if (count + 1 > cap * 3 / 4) rehash();
        int idx = indexFor(c.id, cap);
        buckets[idx] = new Node{c, buckets[idx]};
        count++;
        return true;
    }

    Car* find(int id) const {
        for (Node* n = buckets[indexFor(id, cap)]; n; n = n->next)
            if (n->car.id == id) return &n->car;
        return nullptr;
    }

    bool remove(int id) {
        int idx = indexFor(id, cap);
        Node* prev = nullptr;
        Node* cur = buckets[idx];
        while (cur) {
            if (cur->car.id == id) {
                if (prev) prev->next = cur->next;
                else buckets[idx] = cur->next;
                delete cur;
                count--;
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    // Collect pointers to all cars (order is arbitrary)
    std::vector<const Car*> all() const {
        std::vector<const Car*> result;
        result.reserve(count);
        for (int i = 0; i < cap; i++)
            for (Node* n = buckets[i]; n; n = n->next)
                result.push_back(&n->car);
        return result;
    }
};
