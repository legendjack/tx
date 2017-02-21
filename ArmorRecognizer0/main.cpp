#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include "functions.h"
#include "getConfig.h"
#include "serialsom.h"

using namespace std;
using namespace cv;

// 宏定义
#define WINNAME	"Armor Recognition"
#define WINNAME1 "Binary Image"
#define MaxContourArea 450		// 面积大于该值的轮廓不是装甲的灯条
#define MinContourArea 15		// 面积小于该值的轮廓不是装甲的灯条
#define RED 0					// 代表红色
#define BLUE 1					// 代表蓝色
#define DEBUG

// 全局变量
map<string, string> config;
Mat frame, gray;				// 视频帧及其灰度图
Mat binaryImage, hsvImage;		// 二值图及HSV图，使用cvtColor得到
int m_threshold;				// 阈值
bool showBinaryImage = false;	// 是否显示二值图
Mat element, element1;			// 开运算参数
int Width, Height;				// 视频宽高
int detectColor;				// 敌军装甲的颜色：0-红色，1-蓝色
string fileName;				// 视频的文件名
Point centerOfArmor;
int disX, disY, disZ;
float tmpAngle0 ;
float tmpAngle1;
bool findArmor;
//int yawOut = 50;
//int pitchOut = 50;
uint8_t yawOut = 50;
uint8_t pitchOut = 50;
int frameCount = 150;
Point targetPoint(340, 226);

// 计算直方图需要的参数
Mat hMat, sMat, vMat;			// HSV单通道图
int channels = 0;				// 计算第0个通道的直方图，calcHist参数
int sizeHist = 180;				// 180个色度，calcHist参数
MatND dstHist;					// calcHist结果

int main()
{
	//showText();

	// 初始化串口类
	Serialport Serialport1("/dev/ttyUSB0");
	int fd = Serialport1.open_port("/dev/ttyUSB0");
	if (fd >= 0)
		Serialport1.set_opt(115200, 8, 'N', 1);
 	else
		cout << "未打开串口..." << endl;

	/***************************************
			读取 video.cfg 里面的键值对
	****************************************/
	bool read = ReadConfig("video.cfg", config);	// 把video.cfg读入config键值对，成功返回true
	if (!read) {
		cout << "无法读取 video.cfg" << endl;
		return -1;
	}

	/***************************************
				全局变量初始化
	****************************************/
	m_threshold = atoi(config["THRESHOLD"].c_str());
	Width = atoi(config["WIDTH"].c_str());
	Height = atoi(config["HEIGHT"].c_str());
	detectColor = atoi(config["DETECTCOLOR"].c_str());
	fileName = config["FILENAME"].c_str();

	element = getStructuringElement(MORPH_ELLIPSE, Size(2, 2));
	element1 = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
	hMat.create(Size(Width, Height), CV_8UC1);
	sMat.create(Size(Width, Height), CV_8UC1);
	vMat.create(Size(Width, Height), CV_8UC1);
	Mat chan[3] = { hMat, sMat, vMat };
	float hranges[] = { 0, 180 };
	const float *ranges[] = { hranges };

	vector<RotatedRect> rotatedRectsOfLights;	// 蓝色/红色灯条的RotatedRect
	vector<RotatedRect> rotatedRects;			// 对面积在指定范围内的轮廓拟合椭圆，得到相应的旋转矩形

	namedWindow(WINNAME, WINDOW_AUTOSIZE);
	createTrackbar("Threshold", WINNAME, &m_threshold, 255, 0);

	//VideoCapture cap(fileName);
	VideoCapture cap(1);

	if (!cap.isOpened()) {
		cout << "未打开视频文件" << endl;
		return -1;
	}

	/***************************************
				开始处理每一帧
	****************************************/
	while (true)
	{
		cap >> frame;
		if (frame.empty())
			break;

#ifdef DEBUG
		Mat frame_ = frame.clone();		// 帧图像备份，调试用
#endif // DEBUG

		cvtColor(frame, gray, COLOR_BGR2GRAY);

		cvtColor(frame, hsvImage, COLOR_BGR2HSV);

		threshold(gray, binaryImage, m_threshold, 255, THRESH_BINARY);

		morphologyEx(binaryImage, binaryImage, MORPH_OPEN, element);	// 开运算，先腐蚀，后膨胀。去掉小的白色轮廓

		Mat binaryImage_ = binaryImage.clone();  // 二值图像备份，调试用

		vector<vector<Point> > contours;		// 所有轮廓，findContours函数的结果
		findContours(binaryImage, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);	// 寻找轮廓

		vector<vector<Point> > contoursInAreaRange;	// 面积在(MinContourArea, MaxContourArea)范围内的轮廓
		for (int i = 0; i < contours.size(); i++) {
			double areaTemp = contourArea(contours[i]);
			if (areaTemp > MinContourArea && areaTemp < MaxContourArea)
				contoursInAreaRange.push_back(contours[i]);
		}

		// 如果面积在指定范围内的轮廓数量小于2，则进入下一次循环
		if (contoursInAreaRange.size() < 2)
			goto HERE;

		// 对面积在指定范围内的轮廓拟合椭圆，得到相应的旋转矩形
		rotatedRects.clear();
		for (int i = 0; i < contoursInAreaRange.size(); i++)
			rotatedRects.push_back(fitEllipse(contoursInAreaRange[i]));

		/* 为每一个符合条件的RotatedRect制作一个掩模，然后计算掩模区域的直方图，
		 * 通过直方图判断该RotatedRect的主要颜色是红色还是蓝色
		 */
		rotatedRectsOfLights.clear();
		split(hsvImage, chan);						// 把HSV图像分为3个通道，用于计算直方图
		for (int i = 0; i < rotatedRects.size(); i++) {
			Point2f pointTemp[4];
			rotatedRects[i].points(pointTemp);		// 得到旋转矩形的4个角点
			vector<Point> corners;
			for (int j = 0; j < 4; j++)
				corners.push_back(pointTemp[j]);

			vector<vector<Point> > corners_;
			corners_.push_back(corners);
			Mat mask(Height, Width, CV_8UC1, Scalar::all(0));
			drawContours(mask, corners_, -1, Scalar(255), -1, LINE_AA);	// 绘制掩模
			dilate(mask, mask, element1);								// 膨胀处理
			calcHist(&hMat, 1, &channels, mask, dstHist, 1, &sizeHist, ranges);	// 计算掩模的直方图

			/* 绘制直方图 */
// 			Mat dstImage(180, 180, CV_8U, Scalar(0));
// 			double maxValue = 0;
// 			minMaxLoc(dstHist, 0, &maxValue, 0, 0); // 求得最大值，用于归一化处理

// 			for (int j = 0; j < 180; j++)
// 			{
// 				// 注意hist中是float类型
// 				float binValue = dstHist.at<float>(j);
// 				int realValue = cvRound(binValue / maxValue * 256); // 如果图片尺寸过大，可能出现某一灰度的像素太多，binValue值特别大的情况；这里归一化到0~255
// 				rectangle(dstImage, Point(j, 256 - realValue), Point(j + 1, 256), Scalar(255), -1);
// 			}
// 			imshow("一维直方图", dstImage);
// 			waitKey(0);

			if (JudgeColor(dstHist) == detectColor && !(rotatedRects[i].angle > 30 && rotatedRects[i].angle < 150)) {
				rotatedRectsOfLights.push_back(rotatedRects[i]);
			}
		}

		// 如果检测到灯条的数量小于2，则进入下次循环
		if (rotatedRectsOfLights.size() < 2)
			goto HERE;

//		for (int i = 0; i < rotatedRectsOfLights.size(); i++)
//			cout << rotatedRectsOfLights[i].angle << ", ";

		// 绘制每个灯条的拟合椭圆
		for (int i = 0; i < rotatedRectsOfLights.size(); i++)
			ellipse(frame_, rotatedRectsOfLights[i], Scalar(0, 255, 0), 2, LINE_AA);

        tmpAngle0 = 10;
		tmpAngle1 = 170;
		findArmor = false;
		for (int i = 0; i < rotatedRectsOfLights.size() - 1; i++) {
			for (int j = i + 1; j < rotatedRectsOfLights.size(); j++) {
				float angleDifference = abs(rotatedRectsOfLights[i].angle - rotatedRectsOfLights[j].angle);		// 灯带的角度差
				float yDifference = abs(rotatedRectsOfLights[i].center.y - rotatedRectsOfLights[j].center.y);	// 灯带的Y轴差
				float xDifference = abs(rotatedRectsOfLights[i].center.x - rotatedRectsOfLights[j].center.x);	// 灯带的X轴差
				float rotatedRectHeight = rotatedRectsOfLights[i].size.height;
				float rotatedRectWidth = rotatedRectsOfLights[i].size.width;
				if (rotatedRectHeight < rotatedRectWidth)
					exchange(rotatedRectHeight, rotatedRectWidth);
				if (xDifference < rotatedRectHeight)
					continue;
				if ((angleDifference < 10 || angleDifference > 170) &&
					(angleDifference < tmpAngle0 || angleDifference > tmpAngle1) &&
					yDifference < 7) {
					circle(frame_, rotatedRectsOfLights[i].center, 3, Scalar(0, 0, 255), -1, LINE_AA);
					circle(frame_, rotatedRectsOfLights[j].center, 3, Scalar(0, 0, 255), -1, LINE_AA);
					centerOfArmor = centerOf2Points(rotatedRectsOfLights[i].center, rotatedRectsOfLights[j].center);
					targetPoint.x = 17 * rotatedRectHeight / 21 + 311;
					if (angleDifference < 10)
						tmpAngle0 = angleDifference;
					else if (angleDifference > 170)
						tmpAngle1 = angleDifference;
					//cout << endl << i << ", " << j << endl;
					findArmor = true;
				}
			}
		}
		//cout << endl << "---------------" << endl;

		yawOut = 250;
		pitchOut = 250;

		if (findArmor) {
            frameCount = 0;

			circle(frame_, centerOfArmor, 10, Scalar(0, 0, 255), 2, LINE_AA);
			disX = centerOfArmor.x - targetPoint.x;
			disY = centerOfArmor.y - targetPoint.y;

			disX = -disX;
			disX += 100;
			if (disX > 200)
				disX = 200;
			else if (disX < 0)
				disX = 0;

			disY += 100;
			if (disY > 200)
				disY = 200;
			else if (disY < 0)
				disY = 0;
			
			yawOut = static_cast<uint8_t>(disX);
			pitchOut = static_cast<uint8_t>(disY);
			
 			if (fd >= 0)
				Serialport1.usart3_send(pitchOut, yawOut);	// 发送竖直方向和水平方向移动速度
		} else {
            frameCount++;
            if (frameCount >= 150) {
                frameCount--;
                pitchOut = 250;
                Serialport1.usart3_send(pitchOut, yawOut);
            }
		}
		//circle(frame_, Point(366, 238), 5, Scalar(0, 255, 255), -1, LINE_AA);

HERE:
        cout << static_cast<int>(pitchOut) << ", " << static_cast<int>(yawOut) << endl;
		imshow(WINNAME, frame_);

		if (showBinaryImage)
			imshow(WINNAME1, binaryImage_);

		int key = waitKey(1);
		if (key == 27) {
			break;
		}  else if (key == int('0')) {
			if (showBinaryImage)	// 如果这时候窗口是显示的，则关闭它
				destroyWindow(WINNAME1);
			showBinaryImage = !showBinaryImage;
		} else if (key == int('1')) {
			waitKey(0);
		}
	}

	return 0;
}

