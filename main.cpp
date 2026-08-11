#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <thread>

using namespace std;
using namespace cv;

// Função para reproduzir o som de PONTO em segundo plano
void playPointSound()
{
    thread([]()
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
    thread([]()
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
Point mousePos(-1, -1);

void onMouse(int event, int x, int y, int flags, void *userdata)
{
    mousePos = Point(x, y);
    if (event == EVENT_LBUTTONDOWN)
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
    string filename;
    int highScore;

public:
    ScoreManager(const string &file = "highscore.txt") : filename(file), highScore(0)
    {
        loadHighScore();
    }

    int loadHighScore()
    {
        ifstream inFile(filename);
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
            ofstream outFile(filename);
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

    Mat pipeImgBottom;
    Mat pipeImgTop;

    void overlayImage(Mat &bg, const Mat &fg, Point pt)
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
                    Vec4b fgPixel = fg.at<Vec4b>(y, x);
                    unsigned char alpha = fgPixel[3];

                    if (alpha > 0)
                    {
                        Vec3b &bgPixel = bg.at<Vec3b>(bgY, bgX);
                        if (alpha == 255)
                        {
                            bgPixel = Vec3b(fgPixel[0], fgPixel[1], fgPixel[2]);
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
                    bg.at<Vec3b>(bgY, bgX) = fg.at<Vec3b>(y, x);
                }
            }
        }
    }

public:
    PipeManager() : pipeWidth(80), gapHeight(180), speed(5.0f), passed(false) {}

    bool loadTexture()
    {
        pipeImgBottom = imread("pipe.png", IMREAD_UNCHANGED);
        if (pipeImgBottom.empty())
            return false;

        if (pipeImgBottom.channels() == 3)
        {
            cvtColor(pipeImgBottom, pipeImgBottom, COLOR_BGR2BGRA);
        }

        flip(pipeImgBottom, pipeImgTop, 0);
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

    void draw(Mat &frame)
    {
        int x = static_cast<int>(pipeX);

        if (!pipeImgBottom.empty())
        {
            int topHeight = gapY;
            if (topHeight > 0)
            {
                Mat resizedTop;
                resize(pipeImgTop, resizedTop, Size(pipeWidth, topHeight));
                overlayImage(frame, resizedTop, Point(x, 0));
            }

            int bottomY = gapY + gapHeight;
            int bottomHeight = frame.rows - bottomY;
            if (bottomHeight > 0)
            {
                Mat resizedBottom;
                resize(pipeImgBottom, resizedBottom, Size(pipeWidth, bottomHeight));
                overlayImage(frame, resizedBottom, Point(x, bottomY));
            }
        }
        else
        {
            rectangle(frame, Point(x, 0), Point(x + pipeWidth, gapY), Scalar(0, 200, 0), -1);
            rectangle(frame, Point(x, gapY + gapHeight), Point(x + pipeWidth, frame.rows), Scalar(0, 200, 0), -1);
        }
    }

    bool checkCollision(const Rect &birdBox)
    {
        Rect topPipe(static_cast<int>(pipeX), 0, pipeWidth, gapY);
        Rect bottomPipe(static_cast<int>(pipeX), gapY + gapHeight, pipeWidth, 1000);
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
    CascadeClassifier faceCascade;
    ScoreManager scoreMgr;
    PipeManager pipeMgr;

    Ptr<freetype::FreeType2> ft2;
    bool hasCustomFont;

    Mat bgImage;

    vector<Mat> birdFrames;
    int animFrameIndex;
    int animTimer;

    Point2f smoothBirdPos;
    float alphaSmooth;

    int currentScore;
    GameState currentState;

    void overlayImage(Mat &bg, const Mat &fg, Point pt)
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

                Vec4b fgPixel = fg.at<Vec4b>(y, x);
                unsigned char alpha = fgPixel[3];

                if (alpha > 0)
                {
                    Vec3b &bgPixel = bg.at<Vec3b>(bgY, bgX);
                    if (alpha == 255)
                    {
                        bgPixel = Vec3b(fgPixel[0], fgPixel[1], fgPixel[2]);
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

    void drawText(Mat &frame, const string &text, Point pos, int fontSize, Scalar color)
    {
        if (hasCustomFont)
        {
            ft2->putText(frame, text, pos, fontSize, color, -1, LINE_AA, true);
        }
        else
        {
            putText(frame, text, pos, FONT_HERSHEY_SIMPLEX, fontSize / 28.0, color, 2);
        }
    }

    void drawCenteredText(Mat &frame, const string &text, int y, int fontSize, Scalar color)
    {
        int textWidth = 0;

        if (hasCustomFont)
        {
            int baseLine = 0;
            Size textSize = ft2->getTextSize(text, fontSize, -1, &baseLine);
            textWidth = textSize.width;
        }
        else
        {
            int baseLine = 0;
            Size textSize = getTextSize(text, FONT_HERSHEY_SIMPLEX, fontSize / 28.0, 2, &baseLine);
            textWidth = textSize.width;
        }

        int x = (frame.cols - textWidth) / 2;
        drawText(frame, text, Point(x, y), fontSize, color);
    }

public:
    GameEngine() : currentScore(0), currentState(MENU),
                   animFrameIndex(0), animTimer(0), alphaSmooth(0.25f), hasCustomFont(false) {}

    bool loadResources()
    {
        faceCascade.load("haarcascade_frontalface_default.xml");
        pipeMgr.loadTexture();

        bgImage = imread("background.png");

        try
        {
            ft2 = freetype::createFreeType2();
            ft2->loadFontData("04b_19.ttf", 0);
            hasCustomFont = true;
            cout << "[INFO] Fonte 04b_19 carregada com sucesso!" << endl;
        }
        catch (...)
        {
            cout << "[AVISO] '04b_19.ttf' nao encontrada. Usando fonte padrao..." << endl;
            hasCustomFont = false;
        }

        Mat spriteSheet = imread("bird.png", IMREAD_UNCHANGED);
        if (spriteSheet.empty())
        {
            for (int i = 0; i < 3; ++i)
            {
                Mat frameImg(45, 45, CV_8UC4, Scalar(0, 0, 0, 0));
                circle(frameImg, Point(22, 22), 20 - (i * 2), Scalar(0, 255, 255, 255), -1);
                birdFrames.push_back(frameImg);
            }
        }
        else
        {
            if (spriteSheet.channels() == 3)
            {
                cvtColor(spriteSheet, spriteSheet, COLOR_BGR2BGRA);
            }

            int singleWidth = spriteSheet.cols / 3;
            int singleHeight = spriteSheet.rows;

            for (int i = 0; i < 3; ++i)
            {
                Rect cropRegion(i * singleWidth, 0, singleWidth, singleHeight);
                Mat frameCrop = spriteSheet(cropRegion).clone();
                resize(frameCrop, frameCrop, Size(48, 36));
                birdFrames.push_back(frameCrop);
            }
        }
        return true;
    }

    void drawBgTexture(Mat &frame, int screenWidth, int screenHeight)
    {
        if (!bgImage.empty())
        {
            resize(bgImage, frame, Size(screenWidth, screenHeight));
        }
        else
        {
            frame = Mat(screenHeight, screenWidth, CV_8UC3, Scalar(40, 40, 40));
        }
    }

    void run()
    {
        VideoCapture cap(0);
        bool useRealWebcam = cap.isOpened();

        int screenWidth = 640;
        int screenHeight = 480;
        pipeMgr.reset(screenWidth, screenHeight);

        smoothBirdPos = Point2f(screenWidth / 4.0f, screenHeight / 2.0f);

        string winName = "Flappy Bird - Visao Computacional";
        namedWindow(winName, WINDOW_AUTOSIZE);
        setMouseCallback(winName, onMouse, NULL);

        while (true)
        {
            Mat frame;

            if (currentState == PLAYING && useRealWebcam)
            {
                cap >> frame;
                if (!frame.empty())
                {
                    flip(frame, frame, 1);
                    resize(frame, frame, Size(screenWidth, screenHeight));
                }
            }

            if (frame.empty())
            {
                frame = Mat(screenHeight, screenWidth, CV_8UC3, Scalar(40, 40, 40));
            }

            int key = waitKey(30);
            if (key == 27)
                break;

            // ==========================================
            // TELA 1: MENU INICIAL
            // ==========================================
            if (currentState == MENU)
            {
                drawBgTexture(frame, screenWidth, screenHeight);
                scoreMgr.loadHighScore();

                drawCenteredText(frame, "Flappy Bird", 120, 42, Scalar(0, 200, 255));

                int btnW = 160;
                int btnH = 60;
                int btnX = (screenWidth - btnW) / 2;
                int btnY = 210;
                Rect playBtn(btnX, btnY, btnW, btnH);

                bool isHovered = playBtn.contains(mousePos);
                Scalar btnColor = isHovered ? Scalar(0, 230, 0) : Scalar(0, 180, 0);

                rectangle(frame, playBtn, btnColor, -1);
                rectangle(frame, playBtn, Scalar(0, 100, 0), 3);
                drawCenteredText(frame, "PLAY", btnY + 42, 28, Scalar(255, 255, 255));

                drawCenteredText(frame, "Maior Pontuacao: " + to_string(scoreMgr.getHighScore()), 330, 20, Scalar(255, 255, 255));

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
                    Mat grayFrame;
                    cvtColor(frame, grayFrame, COLOR_BGR2GRAY);
                    vector<Rect> faces;
                    faceCascade.detectMultiScale(grayFrame, faces, 1.1, 4, 0, Size(60, 60));

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

                Mat currentBirdImg = birdFrames[animFrameIndex];
                Rect birdBox(static_cast<int>(smoothBirdPos.x) - currentBirdImg.cols / 2,
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
                    playDieSound();
                    scoreMgr.saveHighScore(currentScore);
                }

                pipeMgr.draw(frame);
                overlayImage(frame, currentBirdImg, Point(birdBox.x, birdBox.y));

                drawText(frame, "Pontos: " + to_string(currentScore), Point(20, 45), 22, Scalar(255, 255, 255));
                drawText(frame, "Recorde: " + to_string(scoreMgr.getHighScore()), Point(20, 75), 22, Scalar(0, 255, 255));
            }
            // ==========================================
            // TELA 3: GAME OVER
            // ==========================================
            else if (currentState == GAME_OVER)
            {
                drawBgTexture(frame, screenWidth, screenHeight);

                drawCenteredText(frame, "GAME OVER", screenHeight / 2 - 20, 32, Scalar(0, 0, 255));
                drawCenteredText(frame, "Pressione 'R' para Reiniciar ou 'M' para o Menu", screenHeight / 2 + 25, 14, Scalar(255, 255, 255));

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
            imshow(winName, frame);
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
