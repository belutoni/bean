// bean.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

/*
* Moves files from the Downloads/ folder to their corresponding folder
*/

#include "organizer.h"

int main() {
    Organizer::instance().watch();
    return 0;
}