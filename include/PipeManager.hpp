#ifndef PIPE_MANAGER_HPP
#define PIPE_MANAGER_HPP

#include <opencv2/opencv.hpp>
#include <cstdlib>

using namespace std;
using namespace cv;

class PipeManager {
private:
    float pipeX;
    int gapY;
    int pipeWidth;
    int gapHeight;
    float speed;
    bool passed;

    Mat pipeImgBottom;
    Mat pipeImgTop;

    void overlayImage(Mat& bg, const Mat& fg, Point pt) {
        for (int y = 0; y < fg.rows; ++y) {
            int bgY = pt.y + y;
            if (bgY < 0 || bgY >= bg.rows) continue;

            for (int x = 0; x < fg.cols; ++x) {
                int bgX = pt.x + x;
                if (bgX < 0 || bgX >= bg.cols) continue;

                if (fg.channels() == 4) {
                    Vec4b fgPixel = fg.at<Vec4b>(y, x);
                    unsigned char alpha = fgPixel[3];

                    if (alpha > 0) {
                        Vec3b& bgPixel = bg.at<Vec3b>(bgY, bgX);
                        if (alpha == 255) {
                            bgPixel = Vec3b(fgPixel[0], fgPixel[1], fgPixel[2]);
                        } else {
                            float a = alpha / 255.0f;
                            bgPixel[0] = static_cast<uchar>(fgPixel[0] * a + bgPixel[0] * (1.0f - a));
                            bgPixel[1] = static_cast<uchar>(fgPixel[1] * a + bgPixel[1] * (1.0f - a));
                            bgPixel[2] = static_cast<uchar>(fgPixel[2] * a + bgPixel[2] * (1.0f - a));
                        }
                    }
                } else {
                    bg.at<Vec3b>(bgY, bgX) = fg.at<Vec3b>(y, x);
                }
            }
        }
    }

public:
    PipeManager() : pipeWidth(80), gapHeight(180), speed(5.0f), passed(false) {}

    bool loadTexture() {
        pipeImgBottom = imread("assets/images/pipe.png", IMREAD_UNCHANGED);
        if (pipeImgBottom.empty()) return false;

        if (pipeImgBottom.channels() == 3) {
            cvtColor(pipeImgBottom, pipeImgBottom, COLOR_BGR2BGRA);
        }

        flip(pipeImgBottom, pipeImgTop, 0);
        return true;
    }

    void reset(int screenWidth, int screenHeight) {
        pipeX = screenWidth;
        int minGapY = 60;
        int maxGapY = screenHeight - gapHeight - 60;
        if (maxGapY <= minGapY) maxGapY = minGapY + 1;
        gapY = rand() % (maxGapY - minGapY + 1) + minGapY;
        passed = false;
    }

    void update(int screenWidth, int screenHeight) {
        pipeX -= speed;
        if (pipeX + pipeWidth < 0) {
            reset(screenWidth, screenHeight);
        }
    }

    void increaseSpeed() { speed += 0.3f; }

    void draw(Mat& frame) {
        int x = static_cast<int>(pipeX);

        if (!pipeImgBottom.empty()) {
            int topHeight = gapY;
            if (topHeight > 0) {
                Mat resizedTop;
                resize(pipeImgTop, resizedTop, Size(pipeWidth, topHeight));
                overlayImage(frame, resizedTop, Point(x, 0));
            }

            int bottomY = gapY + gapHeight;
            int bottomHeight = frame.rows - bottomY;
            if (bottomHeight > 0) {
                Mat resizedBottom;
                resize(pipeImgBottom, resizedBottom, Size(pipeWidth, bottomHeight));
                overlayImage(frame, resizedBottom, Point(x, bottomY));
            }
        } else {
            rectangle(frame, Point(x, 0), Point(x + pipeWidth, gapY), Scalar(0, 200, 0), -1);
            rectangle(frame, Point(x, gapY + gapHeight), Point(x + pipeWidth, frame.rows), Scalar(0, 200, 0), -1);
        }
    }

    bool checkCollision(const Rect& birdBox) {
        Rect topPipe(static_cast<int>(pipeX), 0, pipeWidth, gapY);
        Rect bottomPipe(static_cast<int>(pipeX), gapY + gapHeight, pipeWidth, 1000);
        return (birdBox & topPipe).area() > 0 || (birdBox & bottomPipe).area() > 0;
    }

    bool isPassed(int birdX) {
        if (!passed && pipeX + pipeWidth < birdX) {
            passed = true;
            return true;
        }
        return false;
    }
};

#endif
