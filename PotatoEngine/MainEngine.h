#pragma once
#include "EngineBase.h" 
#include "Mesh.h"
#include "Model.h"

namespace Engine {
	class MainEngine : public EngineBase
	{
	public: 
		MainEngine();

		virtual bool Initialize() override;
		virtual void Render() override;
		virtual void Update() override;
		virtual void UpdateGUI() override;

	private: 
		std::vector<std::shared_ptr<Model>> m_objectList; 

		float diff = 0, spec = 0;

	};
}