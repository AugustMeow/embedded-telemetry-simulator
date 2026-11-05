#include <thread>
#include <iostream>

int main()
{
    int value = 0;

    std::thread t1([&]() {
        value++;
    });

    std::thread t2([&]() {
        value++;
    });

    t1.join();
    t2.join();

    std::cout << value << '\n';

    return 0;
}
