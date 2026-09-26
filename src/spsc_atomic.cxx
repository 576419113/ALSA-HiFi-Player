#pragma once
#include <atomic>
#include <cassert>
#include <unistd.h>

#if 0
template<typename T>
/*! SPSC 原子缓冲区 */
class SPSCAtomBuffer
{
private:
    std::vector<T> buffer;
    std::size_t width;
    alignas(64) std::atomic<std::size_t> write_index { 0 };
    alignas(64) std::atomic<std::size_t> read_index { 0 };
public:
    SPSCAtomBuffer(std::size_t width);
    ~SPSCAtomBuffer() {};
    void push(T &item);
    T &pop();
};

template<typename T>
/*! SPSC 原子缓冲区构造 */
SPSCAtomBuffer<T>::SPSCAtomBuffer(std::size_t w)
{
    assert((w & (w - 1)) == 0);
    buffer.resize(w);
    width = w;
}

template<typename T>
/*! SPSC 原子缓冲区入队 */
void SPSCAtomBuffer<T>::push(T &item)
{
    std::size_t w = write_index.load(std::memory_order_relaxed);
    while (w - read_index.load(std::memory_order_acquire) == width) {
        // 缓冲区满，等待 20ms
        usleep(20'000);
    }
    buffer[w & (width - 1)] = std::move(item);
    write_index.store(w + 1, std::memory_order_release);
}

template<typename T>
/*! SPSC 原子缓冲区出队*/
T &SPSCAtomBuffer<T>::pop()
{
    std::size_t r = read_index.load(std::memory_order_relaxed);
    while (r == write_index.load(std::memory_order_acquire)) {
        // 缓冲区空，等待 20ms
        usleep(20'000);
    }
    T &result = buffer[r & (width - 1)];
    read_index.store(r + 1, std::memory_order_release);
}
#endif

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
    T temp;
    while (pop(temp)) {
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
