#pragma once
#include <string>
#include <fstream>

using namespace std;

class ScoreManager {
private:
    string filename;
    int highScore;

public:
    ScoreManager(const string& file = "save/highscore.txt") : filename(file), highScore(0) {
        loadHighScore();
    }

    int loadHighScore() {
        ifstream inFile(filename);
        if (inFile.is_open()) {
            inFile >> highScore;
            inFile.close();
        } else {
            highScore = 0;
        }
        return highScore;
    }

    void saveHighScore(int score) {
        if (score > highScore) {
            highScore = score;
            ofstream outFile(filename);
            if (outFile.is_open()) {
                outFile << highScore;
                outFile.close();
            }
        }
    }

    int getHighScore() const { return highScore; }
};
