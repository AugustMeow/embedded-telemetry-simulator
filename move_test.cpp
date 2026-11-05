#include <iostream>
#include <string>
#include <utility>

int main()
{
    std::string a(1000, 'A');

    std::string b = a;

    std::string c = std::move(a);

    std::cout << "a size: " << a.size() << '\n';
    //std::cout << "b size: " << b.size() << '\n';
    std::cout << "c size: " << c.size() << '\n';
}
