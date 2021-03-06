#include "uci.h"

#include <iostream>
#include <chrono>

using namespace std;

void timeman(int timeControl, int timeLeft, int increment)
{
    uci::searching = true;

    typedef std::chrono::high_resolution_clock Time;

    auto startTime = Time::now();

    int maxTime = min(timeControl / 30 + increment, timeLeft / 2);

    while (uci::searching)
    {
        auto stopTime = Time::now();

        chrono::duration<float> duration = stopTime - startTime;

        duration *= 1000;

        if (duration.count() > maxTime)
        {
            uci::searching = false;
        }
    }
}