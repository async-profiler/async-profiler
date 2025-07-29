/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H

#include <stdlib.h>
#include "arch.h"

class LinkedListNode {
  public:
    LinkedListNode* next;
    u8 value;
    bool mark;

    LinkedListNode() : LinkedListNode(nullptr, 0) {}
    LinkedListNode(LinkedListNode* next, u8 value) : next(next), value(value), mark(false) {}

    ~LinkedListNode() {
        delete next;
    }
};

LinkedListNode* toLinkedList(const u8* arr, size_t len) {
    if (len == 0) return nullptr;

    LinkedListNode* node = new LinkedListNode(nullptr, arr[0]);
    LinkedListNode* head = node;
    for (size_t i = 1; i < len; ++i) {
        node->next = new LinkedListNode(nullptr, arr[i]);
        node = node->next;
    }
    return head;
}

LinkedListNode* insert8(LinkedListNode* node, u8 value) {
    node->next = new LinkedListNode(node->next, value);
    return node->next;
}

LinkedListNode* insert16(LinkedListNode* node, u16 value) {
    return insert8(insert8(node, (value >> 8) & 0xFF), value & 0xFF);
}

#endif // _LINKEDLIST_H
