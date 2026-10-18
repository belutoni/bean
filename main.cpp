/*
* beand - Windows Daemon used to organize the Downloads folder by file type
*       by moving files into their corresponding folders (e.g., .jpg to Pictures, .mp4 to Videos).
*/

#include "organizer.h"

int main() {
    org::Organizer::instance().watch();
    return 0;
}