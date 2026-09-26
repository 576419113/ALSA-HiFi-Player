#pragma once
#include <cstdint>
#include <utility>
#include <vector>

/*! 内存池类 */
class MemoryPool
{
private:
    struct Node
    {
        uint32_t width;
        char *address;
        bool used;
        Node(uint32_t w, char *a = nullptr, bool u = false): width(w), address(a), used(u) {};
    };
    std::vector<Node> pool {};
public:
    ~MemoryPool();
    char *get(uint32_t width = 0);
    void resize(char *&addr, uint32_t width);
    void free(char *&addr);
};

/*! 析构函数，销毁所有内存 */
MemoryPool::~MemoryPool()
{
    for (Node &node : pool) {
        delete[] node.address;
        node.address = nullptr;
    }
}

/*! 分配一段新内存 */
char *MemoryPool::get(uint32_t width)
{
    for (Node &node : pool) {
        if (node.width == width && !node.used) {
            return node.address;
        }
    }
    char *result = new char[width];
    if (!width) {
        result = nullptr;
    }
    Node node(width, result, true);
    pool.push_back(std::move(node));
    return result;
}

/*! 重新分配内存地址指向的长度，若内存地址不被此类管理，则不进行任何操作 */
void MemoryPool::resize(char *&addr, uint32_t width)
{
    bool find = false;
    for (Node &node : pool) {
        if (node.address == addr) {
            node.used = false;
            if (node.width == width) {
                return;
            }
            find = true;
            break;
        }
    }
    if (!find) {
        return;
    }
    for (Node &node : pool) {
        if (node.width == width && !node.used) {
            addr = node.address;
        }
    }
    char *result = new char[width];
    Node node(width, result, true);
    pool.push_back(std::move(node));
    addr = result;
}

/*! 将内存地址指向的区域标记为 unused */
void MemoryPool::free(char *&addr)
{
    for (Node &node : pool) {
        if (node.address == addr) {
            node.used = false;
            addr = nullptr;
            return;
        }
    }
}
