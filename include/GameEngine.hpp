#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/freetype.hpp>
#include <vector>
#include <string>
#include <iostream>

#include "Audio.hpp"
#include "ScoreManager.hpp"
#include "PipeManager.hpp"

using namespace std;
using namespace cv;

enum GameState
{
    MENU,
    PLAYING,
    GAME_OVER
};

static bool mouseClicked = false;
static Point mousePos(-1, -1);

inline void onMouse(int event, int x, int y, int flags, void *userdata)
{
    mousePos = Point(x, y);
    if (event == EVENT_LBUTTONDOWN)
    {
        mouseClicked = true;
    }
}

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
        int baseLine = 0;

        if (hasCustomFont)
        {
            Size textSize = ft2->getTextSize(text, fontSize, -1, &baseLine);
            textWidth = textSize.width;
        }
        else
        {
            Size textSize = getTextSize(text, FONT_HERSHEY_SIMPLEX, fontSize / 28.0, 2, &baseLine);
            textWidth = textSize.width;
        }

        int x = (frame.cols - textWidth) / 2;
        drawText(frame, text, Point(x, y), fontSize, color);
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

public:
    GameEngine() : currentScore(0), currentState(MENU),
                   animFrameIndex(0), animTimer(0), alphaSmooth(0.25f), hasCustomFont(false) {}

    bool loadResources()
    {
        faceCascade.load("assets/models/haarcascade_frontalface_default.xml");
        pipeMgr.loadTexture();

        bgImage = imread("assets/images/background.png");

        try
        {
            ft2 = freetype::createFreeType2();
            ft2->loadFontData("assets/fonts/04b_19.ttf", 0);
            hasCustomFont = true;
        }
        catch (...)
        {
            hasCustomFont = false;
        }

        Mat spriteSheet = imread("assets/images/bird.png", IMREAD_UNCHANGED);
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

    void run()
    {
        VideoCapture cap(0);
        bool useRealWebcam = cap.isOpened();

        int screenWidth = 640;
        int screenHeight = 480;
        pipeMgr.reset(screenWidth, screenHeight);

        smoothBirdPos = Point2f(screenWidth / 4.0f, screenHeight / 2.0f);

        string winName = "Flappy Bird - Visão Computacional";
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

            // MENU
            if (currentState == MENU)
            {
                drawBgTexture(frame, screenWidth, screenHeight);
                scoreMgr.loadHighScore();

                drawCenteredText(frame, "Flappy Bird", 120, 42, Scalar(0, 200, 255));

                int btnW = 160, btnH = 60;
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
            // PLAYING
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

                if (pipeMgr.isPassed(static_cast<int>(smoothBirdPos.x)))
                {
                    currentScore++;
                    pipeMgr.increaseSpeed();
                    playPointSound();
                    scoreMgr.saveHighScore(currentScore);
                }

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
            // GAME OVER
            else if (currentState == GAME_OVER)
            {
                drawBgTexture(frame, screenWidth, screenHeight);

                drawCenteredText(frame, "GAME OVER", screenHeight / 2 - 20, 32, Scalar(0, 0, 255));
                drawCenteredText(frame, "Pressione 'M' para o Menu", screenHeight / 2 + 25, 14, Scalar(255, 255, 255));

                if (key == 'm' || key == 'M')
                {
                    currentState = MENU;
                }
            }

            mouseClicked = false;
            imshow(winName, frame);
        }
    }
};
