g++ -std=c++17 main.cpp -o a.out \
    `pkg-config --cflags --libs opencv4` \
    `pkg-config --cflags --libs freetype2` \
    -lopencv_freetype \
    -pthread
    
./a.out
