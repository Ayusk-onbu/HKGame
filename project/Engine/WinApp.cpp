#include "WinApp.h"
#include <Windows.h>

bool WinApp::ProcessMessage() {
	MSG msg{};
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

		if (msg.message == WM_QUIT) {
			return true;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return false;
}