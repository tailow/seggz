#include <iostream>

#include "uci.h"
#include "hash.h"
#include "../lib/thc/thc.h"

using namespace std;

int main()
{
    hsh::generateRandomNumbers();

    uci::loop();
}
