#pragma once
#include <atomic>
#include <cassert>

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
