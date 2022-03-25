
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#ifndef _SAMPLER_H
#define _SAMPLER_H

template <typename Element, typename Host>
class ReservoirSampler {
  private:
    int _reservoir_size;
    std::vector<Element> _reservoir;
    unsigned long _running_counter;
  public:
    ReservoirSampler(int size) : _reservoir_size(size), _running_counter(0) {
        srand(time(NULL));
    }

    void sample(Element* e);
    void flush(Host* host, void (Host::*callback)(Element*));
};
#endif // _SAMPLER_H