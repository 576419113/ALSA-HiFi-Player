#pragma once
#include <atomic>
#include <cstddef>
#include <vector>
#include <cassert>

/*! SPSC 原子环形缓冲区 */
template<typename T>
class SPSCAtomBuffer
{
private:
    alignas(64) std::atomic<size_t> write_index { 0 };
    alignas(64) std::atomic<size_t> read_index { 0 };
    std::vector<T> buffer;
    size_t capacity;
public:
    SPSCAtomBuffer(size_t size);
    ~SPSCAtomBuffer();
    void push(const T &value);
    bool pop(T &out);
};

template<typename T>
/*! SPSC 环形原子缓冲区构造，参数需为2的幂 */
SPSCAtomBuffer<T>::SPSCAtomBuffer(size_t t)
{
    capacity = t;
    assert((capacity & (capacity - 1)) == 0);
    buffer.resize(t);
}

template<typename T>
/*! SPSC 环形原子缓冲区析构 */
SPSCAtomBuffer<T>::~SPSCAtomBuffer()
{
}

template<typename T>
/*! SPSC 环形原子缓冲区入队 */
void SPSCAtomBuffer<T>::push(const T &value)
{
    size_t w = write_index.load(std::memory_order_relaxed);
    while (w - read_index.load(std::memory_order_acquire) == capacity) {
        // 缓冲区满
        continue;
    }
    buffer[w & (capacity - 1)] = value;
    write_index.store(w + 1, std::memory_order_release);
}

template<typename T>
/*! SPSC 环形原子缓冲区出队 */
bool SPSCAtomBuffer<T>::pop(T &out)
{
    size_t r = read_index.load(std::memory_order_relaxed);
    if (r == write_index.load(std::memory_order_acquire)) {
        // 检查为空
        return false;
    }
    out = buffer[r & (capacity - 1)];
    read_index.store(r + 1, std::memory_order_release);
    return true;
}

/*! SPSC 原子队列 */
template<typename T>
class SPSCAtomQueue
{
private:
    struct Node
    {
        T data;
        std::atomic<Node *> next;
        Node(T item = T {}): data(item), next(nullptr)
        {
        }
    };
    alignas(64) std::atomic<Node *> head;
    alignas(64) std::atomic<Node *> tail;
public:
    SPSCAtomQueue();
    ~SPSCAtomQueue();
    void push(T content);
    bool pop(T &out);
};

/*! SPSC 原子队列初始化 */
template<typename T>
SPSCAtomQueue<T>::SPSCAtomQueue()
{
    Node *dummy = new Node();
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

/*! SPSC 原子队列销毁 */
template<typename T>
SPSCAtomQueue<T>::~SPSCAtomQueue()
{
    while (pop()) {
    }
    delete head.load(std::memory_order_relaxed);
}

/*! SPSC 原子队列入列 */
template<typename T>
void SPSCAtomQueue<T>::push(T content)
{
    Node *node = new Node(content);
    Node *old_tail = tail.load(std::memory_order_relaxed);
    old_tail->next.store(node, std::memory_order_release);
    tail.store(node, std::memory_order_relaxed);
}

/*! SPSC 原子队列出列 */
template<typename T>
bool SPSCAtomQueue<T>::pop(T &out)
{
    Node *old_head = head.load(std::memory_order_relaxed);
    Node *next_node = old_head->next.load(std::memory_order_acquire);
    if (next_node == nullptr) {
        return false; // 队列空
    }
    out = next_node->data;
    head.store(next_node, std::memory_order_relaxed);
    delete old_head;
    return true;
}
