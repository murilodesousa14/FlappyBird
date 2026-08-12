cmake_minimum_required(VERSION 3.12)
project(FlappyBird)

set(CMAKE_CXX_STANDARD 17)

# Diretório dos cabeçalhos
include_directories(include ${OpenCV_INCLUDE_DIRS})

# Busca as dependências do OpenCV
find_package(OpenCV REQUIRED)

# Executável do projeto
add_executable(FlappyBirdApp src/main.cpp)

# Expõe os headers do projeto e do OpenCV ao compilador
target_include_directories(FlappyBirdApp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Vincula as bibliotecas do OpenCV e suporte a threads
target_link_libraries(FlappyBirdApp PRIVATE ${OpenCV_LIBS} pthread)
