#include "sampler.h"

template <class Element, class Host>
void ReservoirSampler<Element, Host>::sample(Element* e) {
    if (_running_counter < _reservoir_size) {
        _reservoir.push_back(std::move(*e));
    } else {
        int target = rand() % _running_counter;
        if (target < _reservoir_size) {
            _reservoir.at(target) = std::move(*e);
        }
    }
    _running_counter++;
}

template <class Element, class Host>
void ReservoirSampler<Element, Host>::flush(Host* host, void (Host::*callback)(Element*)) {
    int limit = _running_counter < _reservoir_size ? _running_counter : _reservoir_size;
    for (int i = 0; i < limit; i++) {
        Element e = std::move(_reservoir.at(i));
        (host->*callback)(&e);
    }
    _reservoir.clear();
    _running_counter = 0;
}
