#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <thread>

// Função para reproduzir o som de PONTO em segundo plano
void playPointSound()
{
    std::thread([]()
                {
        int res = system("paplay point.wav > /dev/null 2>&1");
        if (res != 0) {
            res = system("canberra-gtk-play -f point.wav > /dev/null 2>&1");
        }
        if (res != 0) {
            system("aplay point.wav > /dev/null 2>&1");
        } })
        .detach();
}

// Função para reproduzir o som de GAME OVER em segundo plano
void playDieSound()
{
    std::thread([]()
                {
        int res = system("paplay die.wav > /dev/null 2>&1");
        if (res != 0) {
            res = system("canberra-gtk-play -f die.wav > /dev/null 2>&1");
        }
        if (res != 0) {
            system("aplay die.wav > /dev/null 2>&1");
        } })
        .detach();
}

// Estado do Jogo
enum GameState
{
    MENU,
    PLAYING,
    GAME_OVER
};

// Variaveis globais para callback do mouse no menu
bool mouseClicked = false;
cv::Point mousePos(-1, -1);

void onMouse(int event, int x, int y, int flags, void *userdata)
{
    mousePos = cv::Point(x, y);
    if (event == cv::EVENT_LBUTTONDOWN)
    {
        mouseClicked = true;
    }
}

// ============================================================================
// CLASSE: ScoreManager
// ============================================================================
class ScoreManager
{
private:
    std::string filename;
    int highScore;

public:
    ScoreManager(const std::string &file = "highscore.txt") : filename(file), highScore(0)
    {
        loadHighScore();
    }

    int loadHighScore()
    {
        std::ifstream inFile(filename);
        if (inFile.is_open())
        {
            inFile >> highScore;
            inFile.close();
        }
        else
        {
            highScore = 0;
        }
        return highScore;
    }

    void saveHighScore(int score)
    {
        if (score > highScore)
        {
            highScore = score;
            std::ofstream outFile(filename);
            if (outFile.is_open())
            {
                outFile << highScore;
                outFile.close();
            }
        }
    }

    int getHighScore() const { return highScore; }
};

// ============================================================================
// CLASSE: PipeManager
// ============================================================================
class PipeManager
{
private:
    float pipeX;
    int gapY;
    int pipeWidth;
    int gapHeight;
    float speed;
    bool passed;

    cv::Mat pipeImgBottom;
    cv::Mat pipeImgTop;

    void overlayImage(cv::Mat &bg, const cv::Mat &fg, cv::Point pt)
    {
        for (int y = 0; y < fg.rows; ++y)
        {
            int bgY = pt.y + y;
            if (bgY < 0 || bgY >= bg.rows)
                continue;

            for (int x = 0; x < fg.cols; ++x)
            {
                int bgX = pt.x + x;
                if (bgX < 0 || bgX >= bg.cols)
                    continue;

                if (fg.channels() == 4)
                {
                    cv::Vec4b fgPixel = fg.at<cv::Vec4b>(y, x);
                    unsigned char alpha = fgPixel[3];

                    if (alpha > 0)
                    {
                        cv::Vec3b &bgPixel = bg.at<cv::Vec3b>(bgY, bgX);
                        if (alpha == 255)
                        {
                            bgPixel = cv::Vec3b(fgPixel[0], fgPixel[1], fgPixel[2]);
                        }
                        else
                        {
                            float a = alpha / 255.0f;
                            bgPixel[0] = static_cast<uchar>(fgPixel[0] * a + bgPixel[0] * (1.0f - a));
                            bgPixel[1] = static_cast<uchar>(fgPixel[1] * a + bgPixel[1] * (1.0f - a));
                            bgPixel[2] = static_cast<uchar>(fgPixel[2] * a + bgPixel[2] * (1.0f - a));
                        }
                    }
                }
                else
                {
                    bg.at<cv::Vec3b>(bgY, bgX) = fg.at<cv::Vec3b>(y, x);
                }
            }
        }
    }

public:
    PipeManager() : pipeWidth(80), gapHeight(180), speed(5.0f), passed(false) {}

    bool loadTexture()
    {
        pipeImgBottom = cv::imread("pipe.png", cv::IMREAD_UNCHANGED);
        if (pipeImgBottom.empty())
            return false;

        if (pipeImgBottom.channels() == 3)
        {
            cv::cvtColor(pipeImgBottom, pipeImgBottom, cv::COLOR_BGR2BGRA);
        }

        cv::flip(pipeImgBottom, pipeImgTop, 0);
        return true;
    }

    void reset(int screenWidth, int screenHeight)
    {
        pipeX = screenWidth;
        int minGapY = 60;
        int maxGapY = screenHeight - gapHeight - 60;
        if (maxGapY <= minGapY)
            maxGapY = minGapY + 1;
        gapY = rand() % (maxGapY - minGapY + 1) + minGapY;
        passed = false;
    }

    void update(int screenWidth, int screenHeight)
    {
        pipeX -= speed;
        if (pipeX + pipeWidth < 0)
        {
            reset(screenWidth, screenHeight);
        }
    }

    void increaseSpeed() { speed += 0.3f; }

    void draw(cv::Mat &frame)
    {
        int x = static_cast<int>(pipeX);

        if (!pipeImgBottom.empty())
        {
            int topHeight = gapY;
            if (topHeight > 0)
            {
                cv::Mat resizedTop;
                cv::resize(pipeImgTop, resizedTop, cv::Size(pipeWidth, topHeight));
                overlayImage(frame, resizedTop, cv::Point(x, 0));
            }

            int bottomY = gapY + gapHeight;
            int bottomHeight = frame.rows - bottomY;
            if (bottomHeight > 0)
            {
                cv::Mat resizedBottom;
                cv::resize(pipeImgBottom, resizedBottom, cv::Size(pipeWidth, bottomHeight));
                overlayImage(frame, resizedBottom, cv::Point(x, bottomY));
            }
        }
        else
        {
            cv::rectangle(frame, cv::Point(x, 0), cv::Point(x + pipeWidth, gapY), cv::Scalar(0, 200, 0), -1);
            cv::rectangle(frame, cv::Point(x, gapY + gapHeight), cv::Point(x + pipeWidth, frame.rows), cv::Scalar(0, 200, 0), -1);
        }
    }

    bool checkCollision(const cv::Rect &birdBox)
    {
        cv::Rect topPipe(static_cast<int>(pipeX), 0, pipeWidth, gapY);
        cv::Rect bottomPipe(static_cast<int>(pipeX), gapY + gapHeight, pipeWidth, 1000);
        return (birdBox & topPipe).area() > 0 || (birdBox & bottomPipe).area() > 0;
    }

    bool isPassed(int birdX)
    {
        if (!passed && pipeX + pipeWidth < birdX)
        {
            passed = true;
            return true;
        }
        return false;
    }
};

// ============================================================================
// CLASSE: GameEngine
// ============================================================================
class GameEngine
{
private:
    cv::CascadeClassifier faceCascade;
    ScoreManager scoreMgr;
    PipeManager pipeMgr;

    cv::Ptr<cv::freetype::FreeType2> ft2;
    bool hasCustomFont;

    cv::Mat bgImage;

    std::vector<cv::Mat> birdFrames;
    int animFrameIndex;
    int animTimer;

    cv::Point2f smoothBirdPos;
    float alphaSmooth;

    int currentScore;
    GameState currentState;

    void overlayImage(cv::Mat &bg, const cv::Mat &fg, cv::Point pt)
    {
        for (int y = 0; y < fg.rows; ++y)
        {
            int bgY = pt.y + y;
            if (bgY < 0 || bgY >= bg.rows)
                continue;

            for (int x = 0; x < fg.cols; ++x)
            {
                int bgX = pt.x + x;
                if (bgX < 0 || bgX >= bg.cols)
                    continue;

                cv::Vec4b fgPixel = fg.at<cv::Vec4b>(y, x);
                unsigned char alpha = fgPixel[3];

                if (alpha > 0)
                {
                    cv::Vec3b &bgPixel = bg.at<cv::Vec3b>(bgY, bgX);
                    if (alpha == 255)
                    {
                        bgPixel = cv::Vec3b(fgPixel[0], fgPixel[1], fgPixel[2]);
                    }
                    else
                    {
                        float a = alpha / 255.0f;
                        bgPixel[0] = static_cast<uchar>(fgPixel[0] * a + bgPixel[0] * (1.0f - a));
                        bgPixel[1] = static_cast<uchar>(fgPixel[1] * a + bgPixel[1] * (1.0f - a));
                        bgPixel[2] = static_cast<uchar>(fgPixel[2] * a + bgPixel[2] * (1.0f - a));
                    }
                }
            }
        }
    }

    void drawText(cv::Mat &frame, const std::string &text, cv::Point pos, int fontSize, cv::Scalar color)
    {
        if (hasCustomFont)
        {
            ft2->putText(frame, text, pos, fontSize, color, -1, cv::LINE_AA, true);
        }
        else
        {
            cv::putText(frame, text, pos, cv::FONT_HERSHEY_SIMPLEX, fontSize / 28.0, color, 2);
        }
    }

    void drawCenteredText(cv::Mat &frame, const std::string &text, int y, int fontSize, cv::Scalar color)
    {
        int textWidth = 0;

        if (hasCustomFont)
        {
            int baseLine = 0;
            cv::Size textSize = ft2->getTextSize(text, fontSize, -1, &baseLine);
            textWidth = textSize.width;
        }
        else
        {
            int baseLine = 0;
            cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontSize / 28.0, 2, &baseLine);
            textWidth = textSize.width;
        }

        int x = (frame.cols - textWidth) / 2;
        drawText(frame, text, cv::Point(x, y), fontSize, color);
    }

public:
    GameEngine() : currentScore(0), currentState(MENU),
                   animFrameIndex(0), animTimer(0), alphaSmooth(0.25f), hasCustomFont(false) {}

    bool loadResources()
    {
        faceCascade.load("haarcascade_frontalface_default.xml");
        pipeMgr.loadTexture();

        bgImage = cv::imread("background.png");

        try
        {
            ft2 = cv::freetype::createFreeType2();
            ft2->loadFontData("04b_19.ttf", 0);
            hasCustomFont = true;
            std::cout << "[INFO] Fonte 04b_19 carregada com sucesso!" << std::endl;
        }
        catch (...)
        {
            std::cout << "[AVISO] '04b_19.ttf' nao encontrada. Usando fonte padrao..." << std::endl;
            hasCustomFont = false;
        }

        cv::Mat spriteSheet = cv::imread("bird.png", cv::IMREAD_UNCHANGED);
        if (spriteSheet.empty())
        {
            for (int i = 0; i < 3; ++i)
            {
                cv::Mat frameImg(45, 45, CV_8UC4, cv::Scalar(0, 0, 0, 0));
                cv::circle(frameImg, cv::Point(22, 22), 20 - (i * 2), cv::Scalar(0, 255, 255, 255), -1);
                birdFrames.push_back(frameImg);
            }
        }
        else
        {
            if (spriteSheet.channels() == 3)
            {
                cv::cvtColor(spriteSheet, spriteSheet, cv::COLOR_BGR2BGRA);
            }

            int singleWidth = spriteSheet.cols / 3;
            int singleHeight = spriteSheet.rows;

            for (int i = 0; i < 3; ++i)
            {
                cv::Rect cropRegion(i * singleWidth, 0, singleWidth, singleHeight);
                cv::Mat frameCrop = spriteSheet(cropRegion).clone();
                cv::resize(frameCrop, frameCrop, cv::Size(48, 36));
                birdFrames.push_back(frameCrop);
            }
        }
        return true;
    }

    void drawBgTexture(cv::Mat &frame, int screenWidth, int screenHeight)
    {
        if (!bgImage.empty())
        {
            cv::resize(bgImage, frame, cv::Size(screenWidth, screenHeight));
        }
        else
        {
            frame = cv::Mat(screenHeight, screenWidth, CV_8UC3, cv::Scalar(40, 40, 40));
        }
    }

    void run()
    {
        cv::VideoCapture cap(0);
        bool useRealWebcam = cap.isOpened();

        int screenWidth = 640;
        int screenHeight = 480;
        pipeMgr.reset(screenWidth, screenHeight);

        smoothBirdPos = cv::Point2f(screenWidth / 4.0f, screenHeight / 2.0f);

        std::string winName = "Flappy Bird - Visao Computacional";
        cv::namedWindow(winName, cv::WINDOW_AUTOSIZE);
        cv::setMouseCallback(winName, onMouse, NULL);

        while (true)
        {
            cv::Mat frame;

            if (currentState == PLAYING && useRealWebcam)
            {
                cap >> frame;
                if (!frame.empty())
                {
                    cv::flip(frame, frame, 1);
                    cv::resize(frame, frame, cv::Size(screenWidth, screenHeight));
                }
            }

            if (frame.empty())
            {
                frame = cv::Mat(screenHeight, screenWidth, CV_8UC3, cv::Scalar(40, 40, 40));
            }

            int key = cv::waitKey(30);
            if (key == 27)
                break;

            // ==========================================
            // TELA 1: MENU INICIAL
            // ==========================================
            if (currentState == MENU)
            {
                drawBgTexture(frame, screenWidth, screenHeight);
                scoreMgr.loadHighScore();

                drawCenteredText(frame, "Flappy Bird", 120, 42, cv::Scalar(0, 200, 255));

                int btnW = 160;
                int btnH = 60;
                int btnX = (screenWidth - btnW) / 2;
                int btnY = 210;
                cv::Rect playBtn(btnX, btnY, btnW, btnH);

                bool isHovered = playBtn.contains(mousePos);
                cv::Scalar btnColor = isHovered ? cv::Scalar(0, 230, 0) : cv::Scalar(0, 180, 0);

                cv::rectangle(frame, playBtn, btnColor, -1);
                cv::rectangle(frame, playBtn, cv::Scalar(0, 100, 0), 3);
                drawCenteredText(frame, "PLAY", btnY + 42, 28, cv::Scalar(255, 255, 255));

                drawCenteredText(frame, "Maior Pontuacao: " + std::to_string(scoreMgr.getHighScore()), 330, 20, cv::Scalar(255, 255, 255));

                if ((mouseClicked && isHovered) || key == 32 || key == 13)
                {
                    currentState = PLAYING;
                    currentScore = 0;
                    pipeMgr.reset(screenWidth, screenHeight);
                }
            }
            // ==========================================
            // TELA 2: JOGANDO
            // ==========================================
            else if (currentState == PLAYING)
            {
                if (useRealWebcam && !faceCascade.empty())
                {
                    cv::Mat grayFrame;
                    cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
                    std::vector<cv::Rect> faces;
                    faceCascade.detectMultiScale(grayFrame, faces, 1.1, 4, 0, cv::Size(60, 60));

                    if (!faces.empty())
                    {
                        smoothBirdPos.x = smoothBirdPos.x + alphaSmooth * ((faces[0].x + faces[0].width / 2) - smoothBirdPos.x);
                        smoothBirdPos.y = smoothBirdPos.y + alphaSmooth * ((faces[0].y + faces[0].height / 2) - smoothBirdPos.y);
                    }
                }

                animTimer++;
                if (animTimer % 4 == 0)
                {
                    animFrameIndex = (animFrameIndex + 1) % birdFrames.size();
                }

                cv::Mat currentBirdImg = birdFrames[animFrameIndex];
                cv::Rect birdBox(static_cast<int>(smoothBirdPos.x) - currentBirdImg.cols / 2,
                                 static_cast<int>(smoothBirdPos.y) - currentBirdImg.rows / 2,
                                 currentBirdImg.cols, currentBirdImg.rows);

                pipeMgr.update(screenWidth, screenHeight);

                // QUANDO PONTUAR -> SOM DE PONTO
                if (pipeMgr.isPassed(static_cast<int>(smoothBirdPos.x)))
                {
                    currentScore++;
                    pipeMgr.increaseSpeed();
                    playPointSound();
                    scoreMgr.saveHighScore(currentScore);
                }

                // QUANDO PERDER -> SOM DE COLISÃO / GAME OVER
                if (pipeMgr.checkCollision(birdBox) || smoothBirdPos.y <= 0 || smoothBirdPos.y >= screenHeight)
                {
                    currentState = GAME_OVER;
                    playDieSound(); // <--- Toca o arquivo die.wav
                    scoreMgr.saveHighScore(currentScore);
                }

                pipeMgr.draw(frame);
                overlayImage(frame, currentBirdImg, cv::Point(birdBox.x, birdBox.y));

                drawText(frame, "Pontos: " + std::to_string(currentScore), cv::Point(20, 45), 22, cv::Scalar(255, 255, 255));
                drawText(frame, "Recorde: " + std::to_string(scoreMgr.getHighScore()), cv::Point(20, 75), 22, cv::Scalar(0, 255, 255));
            }
            // ==========================================
            // TELA 3: GAME OVER
            // ==========================================
            else if (currentState == GAME_OVER)
            {
                drawBgTexture(frame, screenWidth, screenHeight);

                drawCenteredText(frame, "GAME OVER", screenHeight / 2 - 20, 32, cv::Scalar(0, 0, 255));
                drawCenteredText(frame, "Pressione 'R' para Reiniciar ou 'M' para o Menu", screenHeight / 2 + 25, 14, cv::Scalar(255, 255, 255));

                if (key == 'r' || key == 'R')
                {
                    currentScore = 0;
                    currentState = PLAYING;
                    pipeMgr.reset(screenWidth, screenHeight);
                }
                else if (key == 'm' || key == 'M')
                {
                    currentState = MENU;
                }
            }

            mouseClicked = false;
            cv::imshow(winName, frame);
        }
    }
};

int main()
{
    GameEngine game;
    if (game.loadResources())
    {
        game.run();
    }
    return 0;
}
