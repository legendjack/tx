#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include "functions.h"
#include "getConfig.h"
#include "serialsom.h"

using namespace std;
using namespace cv;

// �궨��
#define WINNAME	"Armor Recognition"
#define WINNAME1 "Binary Image"
#define MaxContourArea 450		// ������ڸ�ֵ����������װ�׵ĵ���
#define MinContourArea 15		// ���С�ڸ�ֵ����������װ�׵ĵ���
#define RED 0					// �����ɫ
#define BLUE 1					// ������ɫ
#define DEBUG

// ȫ�ֱ���
map<string, string> config;
Mat frame, gray;				// ��Ƶ֡����Ҷ�ͼ
Mat binaryImage, hsvImage;		// ��ֵͼ��HSVͼ��ʹ��cvtColor�õ�
int m_threshold;				// ��ֵ
bool showBinaryImage = false;	// �Ƿ���ʾ��ֵͼ
Mat element, element1;			// ���������
int Width, Height;				// ��Ƶ���
int detectColor;				// �о�װ�׵���ɫ��0-��ɫ��1-��ɫ
string fileName;				// ��Ƶ���ļ���
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

// ����ֱ��ͼ��Ҫ�Ĳ���
Mat hMat, sMat, vMat;			// HSV��ͨ��ͼ
int channels = 0;				// �����0��ͨ����ֱ��ͼ��calcHist����
int sizeHist = 180;				// 180��ɫ�ȣ�calcHist����
MatND dstHist;					// calcHist���

int main()
{
	//showText();

	// ��ʼ��������
	Serialport Serialport1("/dev/ttyUSB0");
	int fd = Serialport1.open_port("/dev/ttyUSB0");
	if (fd >= 0)
		Serialport1.set_opt(115200, 8, 'N', 1);
 	else
		cout << "δ�򿪴���..." << endl;

	/***************************************
			��ȡ video.cfg ����ļ�ֵ��
	****************************************/
	bool read = ReadConfig("video.cfg", config);	// ��video.cfg����config��ֵ�ԣ��ɹ�����true
	if (!read) {
		cout << "�޷���ȡ video.cfg" << endl;
		return -1;
	}

	/***************************************
				ȫ�ֱ�����ʼ��
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

	vector<RotatedRect> rotatedRectsOfLights;	// ��ɫ/��ɫ������RotatedRect
	vector<RotatedRect> rotatedRects;			// �������ָ����Χ�ڵ����������Բ���õ���Ӧ����ת����

	namedWindow(WINNAME, WINDOW_AUTOSIZE);
	createTrackbar("Threshold", WINNAME, &m_threshold, 255, 0);

	//VideoCapture cap(fileName);
	VideoCapture cap(1);

	if (!cap.isOpened()) {
		cout << "δ����Ƶ�ļ�" << endl;
		return -1;
	}

	/***************************************
				��ʼ����ÿһ֡
	****************************************/
	while (true)
	{
		cap >> frame;
		if (frame.empty())
			break;

#ifdef DEBUG
		Mat frame_ = frame.clone();		// ֡ͼ�񱸷ݣ�������
#endif // DEBUG

		cvtColor(frame, gray, COLOR_BGR2GRAY);

		cvtColor(frame, hsvImage, COLOR_BGR2HSV);

		threshold(gray, binaryImage, m_threshold, 255, THRESH_BINARY);

		morphologyEx(binaryImage, binaryImage, MORPH_OPEN, element);	// �����㣬�ȸ�ʴ�������͡�ȥ��С�İ�ɫ����

		Mat binaryImage_ = binaryImage.clone();  // ��ֵͼ�񱸷ݣ�������

		vector<vector<Point> > contours;		// ����������findContours�����Ľ��
		findContours(binaryImage, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);	// Ѱ������

		vector<vector<Point> > contoursInAreaRange;	// �����(MinContourArea, MaxContourArea)��Χ�ڵ�����
		for (int i = 0; i < contours.size(); i++) {
			double areaTemp = contourArea(contours[i]);
			if (areaTemp > MinContourArea && areaTemp < MaxContourArea)
				contoursInAreaRange.push_back(contours[i]);
		}

		// ��������ָ����Χ�ڵ���������С��2���������һ��ѭ��
		if (contoursInAreaRange.size() < 2)
			goto HERE;

		// �������ָ����Χ�ڵ����������Բ���õ���Ӧ����ת����
		rotatedRects.clear();
		for (int i = 0; i < contoursInAreaRange.size(); i++)
			rotatedRects.push_back(fitEllipse(contoursInAreaRange[i]));

		/* Ϊÿһ������������RotatedRect����һ����ģ��Ȼ�������ģ�����ֱ��ͼ��
		 * ͨ��ֱ��ͼ�жϸ�RotatedRect����Ҫ��ɫ�Ǻ�ɫ������ɫ
		 */
		rotatedRectsOfLights.clear();
		split(hsvImage, chan);						// ��HSVͼ���Ϊ3��ͨ�������ڼ���ֱ��ͼ
		for (int i = 0; i < rotatedRects.size(); i++) {
			Point2f pointTemp[4];
			rotatedRects[i].points(pointTemp);		// �õ���ת���ε�4���ǵ�
			vector<Point> corners;
			for (int j = 0; j < 4; j++)
				corners.push_back(pointTemp[j]);

			vector<vector<Point> > corners_;
			corners_.push_back(corners);
			Mat mask(Height, Width, CV_8UC1, Scalar::all(0));
			drawContours(mask, corners_, -1, Scalar(255), -1, LINE_AA);	// ������ģ
			dilate(mask, mask, element1);								// ���ʹ���
			calcHist(&hMat, 1, &channels, mask, dstHist, 1, &sizeHist, ranges);	// ������ģ��ֱ��ͼ

			/* ����ֱ��ͼ */
// 			Mat dstImage(180, 180, CV_8U, Scalar(0));
// 			double maxValue = 0;
// 			minMaxLoc(dstHist, 0, &maxValue, 0, 0); // ������ֵ�����ڹ�һ������

// 			for (int j = 0; j < 180; j++)
// 			{
// 				// ע��hist����float����
// 				float binValue = dstHist.at<float>(j);
// 				int realValue = cvRound(binValue / maxValue * 256); // ���ͼƬ�ߴ���󣬿��ܳ���ĳһ�Ҷȵ�����̫�࣬binValueֵ�ر�������������һ����0~255
// 				rectangle(dstImage, Point(j, 256 - realValue), Point(j + 1, 256), Scalar(255), -1);
// 			}
// 			imshow("һάֱ��ͼ", dstImage);
// 			waitKey(0);

			if (JudgeColor(dstHist) == detectColor && !(rotatedRects[i].angle > 30 && rotatedRects[i].angle < 150)) {
				rotatedRectsOfLights.push_back(rotatedRects[i]);
			}
		}

		// �����⵽����������С��2��������´�ѭ��
		if (rotatedRectsOfLights.size() < 2)
			goto HERE;

//		for (int i = 0; i < rotatedRectsOfLights.size(); i++)
//			cout << rotatedRectsOfLights[i].angle << ", ";

		// ����ÿ�������������Բ
		for (int i = 0; i < rotatedRectsOfLights.size(); i++)
			ellipse(frame_, rotatedRectsOfLights[i], Scalar(0, 255, 0), 2, LINE_AA);

        tmpAngle0 = 10;
		tmpAngle1 = 170;
		findArmor = false;
		for (int i = 0; i < rotatedRectsOfLights.size() - 1; i++) {
			for (int j = i + 1; j < rotatedRectsOfLights.size(); j++) {
				float angleDifference = abs(rotatedRectsOfLights[i].angle - rotatedRectsOfLights[j].angle);		// �ƴ��ĽǶȲ�
				float yDifference = abs(rotatedRectsOfLights[i].center.y - rotatedRectsOfLights[j].center.y);	// �ƴ���Y���
				float xDifference = abs(rotatedRectsOfLights[i].center.x - rotatedRectsOfLights[j].center.x);	// �ƴ���X���
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
				Serialport1.usart3_send(pitchOut, yawOut);	// ������ֱ�����ˮƽ�����ƶ��ٶ�
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
			if (showBinaryImage)	// �����ʱ�򴰿�����ʾ�ģ���ر���
				destroyWindow(WINNAME1);
			showBinaryImage = !showBinaryImage;
		} else if (key == int('1')) {
			waitKey(0);
		}
	}

	return 0;
}

