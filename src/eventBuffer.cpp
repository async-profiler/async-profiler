#include "eventBuffer.h"

namespace {

    template<typename T>
    BufferedEventImpl<T>* allocEvent(LinearAllocator& allocator, T& event) {
        //TODO: double check alignment
        void* memory = allocator.alloc(sizeof(BufferedEventImpl<T>));
        BufferedEventImpl<T>* bufEv = new(memory) BufferedEventImpl<T>(event);
        return bufEv;
    }
    
    //returns the new head after reversing (aka the previously last list element)
    BufferedEvent* reverseList(BufferedEvent* head) {
        BufferedEvent* previous = nullptr;
        BufferedEvent* current = head;
        while (current != nullptr) {
            BufferedEvent* next = current->next;
            current->next = previous;

            previous = current;
            current = next;
        }
        return previous;
    }
}

void EventBuffer::publish(EventType type, Event& event, int tid, u32 call_trace_id) {
    BufferedEvent* outEvent = nullptr;
    switch (type)
    {
    case PERF_SAMPLE:
    case EXECUTION_SAMPLE:
    case INSTRUMENTED_METHOD:
        outEvent = allocEvent<ExecutionEvent>(_allocator, (ExecutionEvent&)event);
        break;
    case ALLOC_SAMPLE:
    case ALLOC_OUTSIDE_TLAB:
        outEvent = allocEvent<AllocEvent>(_allocator, (AllocEvent&)event);
        break;
    //TODO: add other event types
    }
    if(outEvent == nullptr) {
        //unsupported event type, do nothing
        return;
    }
    outEvent->type = type;
    outEvent->tid = tid;
    outEvent->call_trace_id = call_trace_id;

    //add the event as new head of the output list
    BufferedEvent* prevHead;
    do {
        prevHead = _producerHead;
        outEvent->next = prevHead;
    } while (!_producerHead.compare_exchange_strong(prevHead, outEvent));
}

BufferedEvent* EventBuffer::poll() {

    if(_consumerHead == nullptr) {
        //Move events from _outputHead list to _consumerHead
        //we reverse the list so that we consume the oldest events first
        _consumerHead = reverseList(_producerHead.exchange(nullptr));
    }

    if(_consumerHead != nullptr) {
        BufferedEvent* result = _consumerHead;
        _consumerHead = result->next;
        return result;
    }
    return nullptr;
}

void EventBuffer::clear() {
    _producerHead = nullptr;
    _consumerHead = nullptr;
    _allocator.clear();
}