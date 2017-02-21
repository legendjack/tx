#pragma once
#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include <opencv2/opencv.hpp>
#include <iostream>

#define RED 0					// 代表红色
#define BLUE 1					// 代表蓝色

using namespace std;
using namespace cv;


void showText()
{
	cout << "操作提示：" << endl;
	cout << "\t按【0】键显示阈值分割的结果" << endl;
	cout << "\t按【1】键暂停视频" << endl;
}

void choise(int *a, int *b, int n)
{
	int i, j, k, temp;

	for (i = 0; i < n - 1; i++)
	{
		k = i;	// 给记号赋值

		for (j = i + 1; j < n; j++)
			if (a[k] < a[j]) k = j;	// 是k总是指向最小元素

		// 当k!=i是才交换，否则a[i] 即为最小
		if (i != k)
		{
			temp = a[i];
			a[i] = a[k];
			a[k] = temp;

			temp = b[i];
			b[i] = b[k];
			b[k] = temp;
		}
	}
}

// 计算直方图中分布最多的颜色（白色除外），选取前20个分布最多对的颜色值，看哪个区域最多
int JudgeColor(MatND hist)
{
	// 声明那个数组，color表示颜色，count直方图中第i个颜色对应的值
	int color[180];
	for (int i = 0; i < 180; i++)
		color[i] = i;

	int value[180];
	for (int i = 0; i < 180; i++)
		value[i] = static_cast<int>(hist.at<float>(i));

	choise(value, color, 180);

	int maxValue = color[0];

	int red = 0;
	int blue = 0;
	int otherColor = 0;

	for (int i = 0; i < 15; i++)
	{
		int binValue = color[i];
		if (binValue > 99 && binValue < 125)
		{
			blue++;
		}
		else if ((binValue > -1 && binValue < 11) || (binValue > 155 && binValue < 181))
		{
			red++;
		}
		else
			otherColor++;
	}

	if ((red > blue && red > otherColor) || (maxValue > 155 && maxValue < 181))
	{
		return RED;
	}
	else if ((red < blue && blue > otherColor) || (maxValue > 99 && maxValue < 125))
	{
		return BLUE;
	}
	else
		return -1;
}

Point centerOf2Points(Point p1, Point p2) {
	Point p;
	p.x = (p1.x + p2.x) / 2;
	p.y = (p1.y + p2.y) / 2;
	return p;
}

void exchange(float &a, float &b) {
    float temp = a;
    a = b;
    b = temp;
}

#endif