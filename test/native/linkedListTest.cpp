/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "linkedList.h"
#include "testRunner.hpp"

TEST_CASE(LinkedList_insert8) {
    LinkedListNode* node3 = new LinkedListNode(nullptr, 3);
    LinkedListNode* node2 = new LinkedListNode(node3, 2);
    LinkedListNode* node1 = new LinkedListNode(node2, 1);

    insert8(node2, 10);

    CHECK_EQ(node1->value, 1);
    CHECK_EQ(node1->next->value, 2);
    CHECK_EQ(node1->next->next->value, 10);
    CHECK_EQ(node1->next->next->next->value, 3);
    CHECK_EQ(node1->next->next->next->next, nullptr);
}

TEST_CASE(LinkedList_insert16) {
    LinkedListNode* node3 = new LinkedListNode(nullptr, 3);
    LinkedListNode* node2 = new LinkedListNode(node3, 2);
    LinkedListNode* node1 = new LinkedListNode(node2, 1);

    insert16(node2, 5 << 8 | 6);

    CHECK_EQ(node1->value, 1);
    CHECK_EQ(node1->next->value, 2);
    CHECK_EQ(node1->next->next->value, 5);
    CHECK_EQ(node1->next->next->next->value, 6);
    CHECK_EQ(node1->next->next->next->next->value, 3);
    CHECK_EQ(node1->next->next->next->next->next, nullptr);
}

TEST_CASE(LinkedList_insert32) {
    LinkedListNode* node3 = new LinkedListNode(nullptr, 3);
    LinkedListNode* node2 = new LinkedListNode(node3, 2);
    LinkedListNode* node1 = new LinkedListNode(node2, 1);

    insert32(node2, ((5 << 8 | 6) << 8 | 7) << 8 | 8);

    CHECK_EQ(node1->value, 1);
    CHECK_EQ(node1->next->value, 2);
    CHECK_EQ(node1->next->next->value, 5);
    CHECK_EQ(node1->next->next->next->value, 6);
    CHECK_EQ(node1->next->next->next->next->value, 7);
    CHECK_EQ(node1->next->next->next->next->next->value, 8);
    CHECK_EQ(node1->next->next->next->next->next->next->value, 3);
    CHECK_EQ(node1->next->next->next->next->next->next->next, nullptr);
}
