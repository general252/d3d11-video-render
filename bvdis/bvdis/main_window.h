#pragma once

#include <Windows.h>

class MainWindow
{
public:
	MainWindow();
	virtual ~MainWindow();

	virtual bool Init(int pos_x, int pos_y, int width, int height);
	virtual void Destroy();

	bool IsWindow();
	HWND GetHandle();

	virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	static bool RegisterWindowClass();

	HWND wnd_ = NULL;

	static ATOM wnd_class_;
	static const wchar_t kClassName[];
};

int HandleMessage();