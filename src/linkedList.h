#include <stdlib.h>

class LinkedListNode {
  public:
    LinkedListNode* next;
    u8 value;

    LinkedListNode() : LinkedListNode(nullptr, 0) {}
    LinkedListNode(LinkedListNode* next, u8 value) : next(next), value(value) {}

    ~LinkedListNode() {
        delete next;
    }
};

LinkedListNode* toLinkedList(const u8* arr, size_t len) {
    LinkedListNode* nodes = new LinkedListNode[len];
    for (size_t i = 0; i < len - 1; ++i) {
        nodes[i].next = nodes + i + 1;
        nodes[i].value = arr[i];
    }
    nodes[len-1].value = arr[len-1];
    return nodes;
}

LinkedListNode* insert8(LinkedListNode* node, u8 value) {
    node->next = new LinkedListNode(node->next, value);
    return node->next;
}

LinkedListNode* insert16(LinkedListNode* node, u16 value) {
    return insert8(insert8(node, (value >> 8) & 0xFF), value & 0xFF);
}

LinkedListNode* insert32(LinkedListNode* node, u32 value) {
    return insert8(insert8(insert8(insert8(node, (value >> 24) & 0xFF), (value >> 16) & 0xFF), (value >> 8) & 0xFF), value & 0xFF);
}

// Detach the segment [prev+1:prev+1+segment_size)
// E.g. a->b->c
// detachSegment(b, 1) => a->b
// detachSegment(a, 2) => a
// detachSegment(a, 1) => a->c
// detachSegment(a, 0) => a->b->c
void detachSegment(LinkedListNode* prev, u32 segment_size) {
    if (segment_size == 0) return;

    LinkedListNode* detached_head = prev->next;
    for (int i = 0; i < segment_size - 1; ++i) {
        prev->next = prev->next->next;
    }
    LinkedListNode* after_segment = prev->next->next;
    prev->next->next = nullptr;
    prev->next = after_segment;
    delete detached_head;
}