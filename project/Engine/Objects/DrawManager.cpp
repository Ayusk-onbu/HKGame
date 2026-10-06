#include "DrawManager.h"

void DrawManager::Initialize(Fngine* engine) {
	CreateAndRegister(box_, engine, 1500);
	CreateAndRegister(ring_, engine, 1000);
	CreateAndRegister(sphere_, engine, 5000);
	CreateAndRegister(cylinder_, engine, 5000);
	CreateAndRegister(line_, engine, 10000);
	CreateAndRegister(magicCircle_, engine, 100);
	CreateAndRegister(grass_, engine, 5500);
	CreateAndRegister(column_, engine, 100);
	CreateAndRegister(bone_, engine, 100);
	CreateAndRegister(character_, engine, 10);
	CreateAndRegister(normalMesh_, engine, 5000);
}

void DrawManager::Draw(RenderPass pass) {
	if (pass == RenderPass::DepthPrePass) {
		for (auto* model : models_) {
			model->DrawInstancedPrePass();
		}
	}
	else if (pass == RenderPass::MainColor) {
		for (auto* model : models_) {
			model->DrawInstanced();
		}
	}
}