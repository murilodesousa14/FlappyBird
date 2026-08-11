#pragma once
#include <cstdlib>
#include <thread>

using namespace std;

inline void playPointSound() {
    thread([]() {
        int res = system("paplay assets/sounds/point.wav > /dev/null 2>&1");
        if (res != 0) {
            res = system("canberra-gtk-play -f assets/sounds/point.wav > /dev/null 2>&1");
        }
        if (res != 0) {
            system("aplay assets/sounds/point.wav > /dev/null 2>&1");
        }
    }).detach();
}

inline void playDieSound() {
    thread([]() {
        int res = system("paplay assets/sounds/die.wav > /dev/null 2>&1");
        if (res != 0) {
            res = system("canberra-gtk-play -f assets/sounds/die.wav > /dev/null 2>&1");
        }
        if (res != 0) {
            system("aplay assets/sounds/die.wav > /dev/null 2>&1");
        }
    }).detach();
}
