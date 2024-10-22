#include "test_runner.hpp"

int main()
{
    TestRunner::instance()->runAllTests();
    return 0;
}
