#pragma once
#include <algorithm>
#include <vector>

// Self-balancing binary search tree (AVL tree).
// Each node stores a car's price and id. Keys are compared as (price, id),
// so two cars with the same price can both be stored.
// insert / remove / search: O(log n).   Range query: O(log n + k), k = matches.
class AVLTree {
    struct Node {
        long long price;
        int id;
        int height;
        Node* left;
        Node* right;
        Node(long long p, int i) : price(p), id(i), height(1), left(nullptr), right(nullptr) {}
    };

    Node* root = nullptr;
    int count = 0;

    // ---------- helpers ----------
    static int heightOf(Node* n) { return n ? n->height : 0; }

    static void updateHeight(Node* n) {
        n->height = 1 + std::max(heightOf(n->left), heightOf(n->right));
    }

    // positive = left side taller, negative = right side taller
    static int balanceFactor(Node* n) {
        return n ? heightOf(n->left) - heightOf(n->right) : 0;
    }

    // is key (p1, i1) smaller than key (p2, i2)?
    static bool keyLess(long long p1, int i1, long long p2, int i2) {
        return p1 < p2 || (p1 == p2 && i1 < i2);
    }

    // ---------- rotations ----------
    // Right rotation: the left child x moves up, y becomes x's right child.
    // Before:  y has left child x (x has subtrees A, B) and right subtree C.
    // After:   x has left subtree A and right child y (y has subtrees B, C).
    Node* rotateRight(Node* y) {
        Node* x = y->left;
        Node* b = x->right;
        x->right = y;
        y->left = b;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    Node* rotateLeft(Node* x) {
        Node* y = x->right;
        Node* b = y->left;
        y->left = x;
        x->right = b;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    // Fix a node whose subtrees may have become unbalanced (difference > 1)
    Node* rebalance(Node* n) {
        updateHeight(n);
        int bf = balanceFactor(n);

        if (bf > 1) {                                   // left side too tall
            if (balanceFactor(n->left) < 0)             // Left-Right case
                n->left = rotateLeft(n->left);
            return rotateRight(n);                      // Left-Left case
        }
        if (bf < -1) {                                  // right side too tall
            if (balanceFactor(n->right) > 0)            // Right-Left case
                n->right = rotateRight(n->right);
            return rotateLeft(n);                       // Right-Right case
        }
        return n;                                       // already balanced
    }

    // ---------- insert / remove ----------
    Node* insert(Node* n, long long p, int id) {
        if (!n) {
            count++;
            return new Node(p, id);
        }
        if (keyLess(p, id, n->price, n->id))
            n->left = insert(n->left, p, id);
        else if (keyLess(n->price, n->id, p, id))
            n->right = insert(n->right, p, id);
        else
            return n;                                   // same key already stored
        return rebalance(n);
    }

    static Node* smallest(Node* n) {
        while (n->left) n = n->left;
        return n;
    }

    Node* remove(Node* n, long long p, int id) {
        if (!n) return nullptr;

        if (keyLess(p, id, n->price, n->id)) {
            n->left = remove(n->left, p, id);
        } else if (keyLess(n->price, n->id, p, id)) {
            n->right = remove(n->right, p, id);
        } else {
            // found the node to delete
            if (!n->left || !n->right) {                // 0 or 1 child
                Node* child = n->left ? n->left : n->right;
                delete n;
                count--;
                return child;
            }
            // 2 children: copy the next larger key here, then delete that node
            Node* next = smallest(n->right);
            n->price = next->price;
            n->id = next->id;
            n->right = remove(n->right, next->price, next->id);
        }
        return rebalance(n);
    }

    // ---------- range query ----------
    // In-order walk that skips branches which cannot contain matches
    void collect(Node* n, long long lo, long long hi, std::vector<int>& out) const {
        if (!n) return;
        if (lo <= n->price) collect(n->left, lo, hi, out);
        if (lo <= n->price && n->price <= hi) out.push_back(n->id);
        if (hi >= n->price) collect(n->right, lo, hi, out);
    }

    void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

public:
    AVLTree() = default;
    ~AVLTree() { destroy(root); }
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    void insert(long long price, int id) { root = insert(root, price, id); }

    // returns true if the (price, id) pair was found and removed
    bool remove(long long price, int id) {
        int before = count;
        root = remove(root, price, id);
        return count < before;
    }

    // ids of all cars with lo <= price <= hi, in ascending price order
    std::vector<int> rangeIds(long long lo, long long hi) const {
        std::vector<int> out;
        collect(root, lo, hi, out);
        return out;
    }

    int size() const { return count; }
    int height() const { return heightOf(root); }
};
