#pragma once
#include "Fngine.h"

class SceneDirector;
class Scene
{
public:
	Scene() = default;
	virtual ~Scene() = default;
public:
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void DrawDepthPrePass() {};
	virtual void Draw() = 0;
	virtual void PauseUpdate() {};
	virtual void PauseDraw() {};
	virtual void GetSceneDirector(SceneDirector* sceneDirector) { p_sceneDirector_ = sceneDirector; }
	virtual void FngineSetUp(Fngine& fngine) { p_fngine_ = &fngine; }
	virtual bool CanPause() const { return false; }
public:
	Fngine* p_fngine_;
	SceneDirector* p_sceneDirector_;
	bool hasRequestedNextScene_ = false;
};

