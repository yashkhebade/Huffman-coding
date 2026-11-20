// source_code.cpp - Sample C++ source code for compression benchmarking
// Demonstrates how source code containing repeated keywords, identifiers,
// indentation spaces, and syntax symbols compresses under Huffman Coding.

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <map>

namespace dsa_demo {

template <typename T>
class BinarySearchTree {
public:
    struct TreeNode {
        T data;
        std::unique_ptr<TreeNode> left;
        std::unique_ptr<TreeNode> right;
        TreeNode(const T& val) : data(val), left(nullptr), right(nullptr) {}
    };

    BinarySearchTree() : root_(nullptr), count_(0) {}

    void insert(const T& value) {
        root_ = insertRecursive(std::move(root_), value);
        ++count_;
    }

    bool contains(const T& value) const {
        return searchRecursive(root_.get(), value);
    }

    void inorderTraversal() const {
        inorderHelper(root_.get());
        std::cout << std::endl;
    }

    size_t size() const { return count_; }

private:
    std::unique_ptr<TreeNode> root_;
    size_t count_;

    std::unique_ptr<TreeNode> insertRecursive(std::unique_ptr<TreeNode> current, const T& value) {
        if (!current) {
            return std::make_unique<TreeNode>(value);
        }
        if (value < current->data) {
            current->left = insertRecursive(std::move(current->left), value);
        } else if (value > current->data) {
            current->right = insertRecursive(std::move(current->right), value);
        }
        return current;
    }

    bool searchRecursive(const TreeNode* current, const T& value) const {
        if (!current) return false;
        if (current->data == value) return true;
        if (value < current->data) return searchRecursive(current->left.get(), value);
        return searchRecursive(current->right.get(), value);
    }

    void inorderHelper(const TreeNode* node) const {
        if (!node) return;
        inorderHelper(node->left.get());
        std::cout << node->data << " ";
        inorderHelper(node->right.get());
    }
};

} // namespace dsa_demo

int main() {
    dsa_demo::BinarySearchTree<int> tree;
    const std::vector<int> values = {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 65, 75, 85};
    for (int val : values) {
        tree.insert(val);
    }

    std::cout << "Binary Search Tree constructed with " << tree.size() << " nodes." << std::endl;
    std::cout << "Inorder traversal: ";
    tree.inorderTraversal();

    std::cout << "Search 40: " << (tree.contains(40) ? "FOUND" : "NOT FOUND") << std::endl;
    std::cout << "Search 99: " << (tree.contains(99) ? "FOUND" : "NOT FOUND") << std::endl;

    return 0;
}
