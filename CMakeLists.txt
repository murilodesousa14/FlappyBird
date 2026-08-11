cmake_minimum_required(VERSION 3.12)
project(FlappyBird)

set(CMAKE_CXX_STANDARD 17)

# Diretório dos cabeçalhos
include_directories(include)

# Busca as dependências do OpenCV
find_package(OpenCV REQUIRED)

# Executável do projeto
add_executable(FlappyBirdApp src/main.cpp)

# Vincula as bibliotecas do OpenCV e suporte a threads
target_link_libraries(FlappyBirdApp ${OpenCV_LIBS} pthread)
