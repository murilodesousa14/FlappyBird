g++ -std=c++17 -Iinclude src/main.cpp -o FlappyBirdApp `pkg-config --cflags --libs opencv4` -lpthread

./FlappyBirdApp
