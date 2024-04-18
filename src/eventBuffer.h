#ifndef _EVENTBUFFER_H
#define _EVENTBUFFER_H

#include "event.h"
#include "linearAllocator.h"
#include <atomic>

struct BufferedEvent {
    BufferedEvent* next; //pointers for single linked list management
    EventType type;
    int tid; //native thread id on which the event was recorded
    u32 call_trace_id; //reference to the call stacktrace stored in CallTraceStorage
};

template<typename EventStruct>
struct BufferedEventImpl : public BufferedEvent {
    EventStruct event;
    BufferedEventImpl(const EventStruct& ev) : event(ev){}
};

// Simple Single Consumer, Multi Producer buffer based on single linked lists
// Producing events is async-signal safe
class EventBuffer {
    LinearAllocator _allocator;

    std::atomic<BufferedEvent*> _producerHead;
    std::atomic<BufferedEvent*> _consumerHead;

public:
    void publish(EventType type, Event& event, int tid, u32 call_trace_id);
    // consumes the next available event , returns nullptr if no event is available
    BufferedEvent* poll();
    // clear may only be called if no producers and consumers are currently active
    void clear();
};

#endif