#include <RepeatedTask/RepeatedTask.h>

#include <iostream>
#include <thread>
#include <chrono>

void callback()
{
    std::cout << "Hello, world!" << std::endl;
}

int main()
{
    RepeatedTask task(&callback, std::chrono::seconds(1));

    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
