#include "GameEngine.hpp"

int main() {
    GameEngine game;
    if (game.loadResources()) {
        game.run();
    }
    return 0;
}
