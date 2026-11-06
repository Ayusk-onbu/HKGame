#include "Game.h"

// windowsアプリでのエントリーポイント（main関数）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
# if defined USE_IMGUI
	D3D12ResourceLeakChecker leakCheck;
#endif
	
	{
		std::unique_ptr<Game>game = std::make_unique<Game>();
		game->Initialize();

		while (true) {
			if (game->IsEnd()) {
				break;
			}
			game->Run();
		}

		game->Finalize();
	}
	//COMの初期化を解除
	CoUninitialize();

	return 0;
}

//   決まり事
//   1. 長さはメートル(m)
//   2. 重さはキログラム(kg)
//   3. 時間は秒(s)