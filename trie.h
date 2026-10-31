#pragma once
#include <algorithm>
#include <string>
#include <vector>

// Trie (prefix tree) for search-as-you-type on brand and model.
//
// Every word of a car's "brand model" text is stored letter by letter.
// For example "Toyota Corolla" stores the words "toyota" and "corolla".
// The node where a word ends remembers the ids of the cars that contain it.
//
// Searching "toy" walks t -> o -> y (3 steps, no matter how many cars exist),
// then collects every car id stored below that point.
// Typing several words ("toyota cor") keeps only cars matching ALL of them.
//
// Only letters a-z and digits 0-9 are stored (case ignored).
// Any other character (space, '-', ...) separates words.
class Trie {
    static const int ALPHABET = 36;            // 26 letters + 10 digits

    struct Node {
        Node* child[ALPHABET];
        std::vector<int> ids;                  // cars with a word that ENDS at this node
        Node() { for (int i = 0; i < ALPHABET; i++) child[i] = nullptr; }
    };

    Node* root;
    int nodes = 1;                             // number of nodes (root included)

    static int indexOf(char ch) {              // expects an already-lowercased a-z / 0-9
        if (ch >= 'a' && ch <= 'z') return ch - 'a';
        return 26 + (ch - '0');
    }

    static bool hasChildren(const Node* n) {
        for (int i = 0; i < ALPHABET; i++)
            if (n->child[i]) return true;
        return false;
    }

    void insertWord(const std::string& word, int id) {
        Node* cur = root;
        for (char ch : word) {
            int i = indexOf(ch);
            if (!cur->child[i]) {
                cur->child[i] = new Node();
                nodes++;
            }
            cur = cur->child[i];
        }
        if (std::find(cur->ids.begin(), cur->ids.end(), id) == cur->ids.end())
            cur->ids.push_back(id);
    }

    // Removes id from the word's end node, then deletes nodes that became useless.
    // Returns true if node n is now empty (no ids, no children) so the caller can delete it.
    bool removeWord(Node* n, const std::string& word, size_t depth, int id) {
        if (depth == word.size()) {
            n->ids.erase(std::remove(n->ids.begin(), n->ids.end(), id), n->ids.end());
        } else {
            int i = indexOf(word[depth]);
            Node* c = n->child[i];
            if (!c) return false;              // word was never stored
            if (removeWord(c, word, depth + 1, id)) {
                delete c;
                n->child[i] = nullptr;
                nodes--;
            }
        }
        return n->ids.empty() && !hasChildren(n);
    }

    // Gather the ids stored in n and everything below it
    void collect(const Node* n, std::vector<int>& out) const {
        out.insert(out.end(), n->ids.begin(), n->ids.end());
        for (int i = 0; i < ALPHABET; i++)
            if (n->child[i]) collect(n->child[i], out);
    }

    // ids of cars having a word that starts with prefix (sorted, no duplicates)
    std::vector<int> lookupPrefix(const std::string& prefix) const {
        const Node* cur = root;
        for (char ch : prefix) {
            cur = cur->child[indexOf(ch)];
            if (!cur) return {};               // nothing starts with this prefix
        }
        std::vector<int> out;
        collect(cur, out);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    // Intersection of two sorted lists (two-pointer merge)
    static std::vector<int> intersectSorted(const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> out;
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (a[i] < b[j]) i++;
            else if (a[i] > b[j]) j++;
            else { out.push_back(a[i]); i++; j++; }
        }
        return out;
    }

    static std::vector<std::string> uniqueWords(const std::string& text) {
        std::vector<std::string> words = tokenize(text);
        std::sort(words.begin(), words.end());
        words.erase(std::unique(words.begin(), words.end()), words.end());
        return words;
    }

    void destroy(Node* n) {
        if (!n) return;
        for (int i = 0; i < ALPHABET; i++) destroy(n->child[i]);
        delete n;
    }

public:
    Trie() : root(new Node()) {}
    ~Trie() { destroy(root); }
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    // Splits text into lowercase words made of a-z / 0-9 ("BR-V 1.5" -> br, v, 1, 5)
    static std::vector<std::string> tokenize(const std::string& text) {
        std::vector<std::string> words;
        std::string cur;
        for (char raw : text) {
            unsigned char ch = static_cast<unsigned char>(raw);
            bool isLetter = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            bool isDigit = (ch >= '0' && ch <= '9');
            if (isLetter) cur += static_cast<char>(ch | 0x20);   // to lowercase
            else if (isDigit) cur += static_cast<char>(ch);
            else if (!cur.empty()) { words.push_back(cur); cur.clear(); }
        }
        if (!cur.empty()) words.push_back(cur);
        return words;
    }

    void addCar(const std::string& brand, const std::string& model, int id) {
        for (const std::string& w : uniqueWords(brand + " " + model)) insertWord(w, id);
    }

    void removeCar(const std::string& brand, const std::string& model, int id) {
        for (const std::string& w : uniqueWords(brand + " " + model)) removeWord(root, w, 0, id);
    }

    // Cars whose brand/model contains a word starting with EACH typed word.
    // Returns ids in ascending order. Empty if the query has no usable words.
    std::vector<int> search(const std::string& query) const {
        std::vector<std::string> words = uniqueWords(query);
        if (words.empty()) return {};
        std::vector<int> result = lookupPrefix(words[0]);
        for (size_t k = 1; k < words.size() && !result.empty(); k++)
            result = intersectSorted(result, lookupPrefix(words[k]));
        return result;
    }

    int nodeCount() const { return nodes; }
};
