#include "MainEngine.h"
#include <vector> 
#include <directXtk/SimpleMath.h>
#include <iostream>
#include "Vertex.h"
#include "D3D11Utils.h"
#include "DefaultObjectGenerator.h"
#include "DefineGraphicsPSO.h"
#include "ResourceLoader.h"
#include <vector> 

namespace Engine {

	MainEngine::MainEngine() : EngineBase() 
	{
	}

	bool MainEngine::Initialize()
	{
		if (!EngineBase::Initialize()) return false; 

		//std::vector<Vector4> billboardPoints; 
		//Vector4 p = Vector4(-20.0f, 1.0f, 20.0f, 1.0f);
		//for (int i = 0; i < 20; i++) {
		//	billboardPoints.push_back(p);
		//	p.x += 2.0f; 
		//}
		//billboards.Initialize(m_device, m_context, billboardPoints, 1.8f);

		auto tq = DefaultObjectGenerator::MakeTessellationQuad();
		tessellationQuad.Initialize(m_device, m_context, tq); 

		//CreateCubeMap(L"../Resources/Texture/CubeMap/", L"SampleEnvMDR.dds", L"SampleDiffuseMDR.dds", L"SampleSpecularMDR.dds");
		CreateCubeMap(L"../Resources/Texture/CubeMap/", L"SampleEnvHDR.dds", L"SampleDiffuseHDR.dds", L"SampleSpecularHDR.dds");
		
		MeshData envMapMeshData = DefaultObjectGenerator::MakeBox(40.0f); 
		std::reverse(envMapMeshData.indices.begin(), envMapMeshData.indices.end()); 
		m_envMap = std::make_shared<Model>(m_device, m_context, std::vector{ envMapMeshData }); 

		{
			Vector3 pos = Vector3(0.0f, 0.0f, 3.0f);
			//auto meshData = DefaultObjectGenerator::MakeSquareGrid(1.0f, 3, 2); 
			//auto meshData = DefaultObjectGenerator::MakeBox(1.0f);
			//auto meshData = DefaultObjectGenerator::MakeCylinder(1.0f, 1.5f, 2.0f, 20, 5); 
			auto meshData = DefaultObjectGenerator::MakeSphere(2.0f, 50, 50);
			//auto meshData = DefaultObjectGenerator::ReadFromFile("../Resources/3D_Model/", "stanford_dragon.stl", false);
			meshData = DefaultObjectGenerator::SubdivideToSphere(1.5f, meshData);
			//meshData = DefaultObjectGenerator::SubdivideToSphere(1.5f, meshData);
			//meshData.albedoTextureFile = "../Resources/Texture/hanbyeol.png";
			meshData.albedoTextureFile = "../Resources/Texture/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_diffuse.jpg"; 
			meshData.normalMapTextureFile = "../Resources/Texture/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_normal.jpg"; 
			//meshData.heightMapTextureFile = "../Resources/Texture/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_height.jpg"; 
			//meshData.AOTextureFile = "../Resources/Texture/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_ao.jpg"; 
			auto model = std::make_shared<Model>(m_device, m_context, std::vector{ meshData }); 
			//auto model = std::make_shared<Model>(m_device, m_context, meshData); 
			//model->isVisible = false; 
			model->modelConstantCPU.world = Matrix::CreateTranslation(pos).Transpose(); 
			//model->modelConstantCPU.world = (Matrix::CreateRotationX(90 * DirectX::XM_PI / 180) * Matrix::CreateTranslation(pos)).Transpose();
			//model->modelConstantCPU.invTranspose = Matrix::CreateRotationX(90 * DirectX::XM_PI / 180).Invert(); 
			model->materialConstantCPU.texture.invertNormalMapY = true; 
			model->UpdateConstantBuffer(m_context); 

			m_objectList.push_back(model); 

			m_objectBS = DirectX::BoundingSphere(pos, 0.0f); 
		}

		{
			auto meshData = DefaultObjectGenerator::MakeSphere(0.03f, 10, 10); 
			m_cursorSphere = std::make_shared<Model>(m_device, m_context, std::vector{ meshData }); 
			m_cursorSphere->isVisible = false; 
			m_cursorSphere->materialConstantCPU.texture.useAmbient = true;
			m_cursorSphere->materialConstantCPU.mat.ambient = Vector3(1.0f, 1.0f, 0.0f); 
			m_cursorSphere->UpdateConstantBuffer(m_context); 

			m_objectList.push_back(m_cursorSphere); 
		}

		globalConstantCPU.light.pos = Vector3(0.0f, 0.0f, -1.0f);
		globalConstantCPU.light.dir = Vector3(0.0f, 0.0f, 1.0f);
		globalConstantCPU.light.strength = Vector3(1.0f); 

		return true;
	}

	void MainEngine::Update()
	{ 
		InputProcess(); 
		EulerCalc(); 

		Matrix view = camera.GetView();
		Matrix proj = camera.GetProj();
		Vector3 eyePos = camera.GetPos(); 

		UpdateGlobalConstant(view, proj, eyePos); 

		Vector3 pickPoint; 
		Quaternion q; 
		Vector3 dragTranslation;
		if (MousePicking(m_objectBS, q, dragTranslation, pickPoint)) {  
			Matrix& world = m_objectList[0]->modelConstantCPU.world; 
			Matrix& inv = m_objectList[0]->modelConstantCPU.invTranspose; 
			world = world.Transpose(); 

			Vector3 translation = world.Translation();
			world.Translation(Vector3(0.0f)); 

			Matrix wr = world * Matrix::CreateFromQuaternion(q); 

			world = wr * Matrix::CreateTranslation(dragTranslation + translation); 
			inv = wr.Invert(); 

			world = world.Transpose();
			m_objectBS.Center = dragTranslation + translation; 

			m_cursorSphere->modelConstantCPU.world = Matrix::CreateTranslation(pickPoint).Transpose(); 
			m_cursorSphere->isVisible = true; 
		}
		else {
			m_cursorSphere->isVisible = false; 
		}

		for (auto& a : m_objectList) {
			a->UpdateConstantBuffer(m_context); 
		}

		postProcess.samplingFilter.UpdateConstantBuffer(m_context);
		postProcess.combineFilter.UpdateConstantBuffer(m_context);
	}

	void MainEngine::Render() 
	{
		SetDefaultViewport(); 

		m_context->VSSetSamplers(0, PSO::samplerStates.size(), PSO::samplerStates.data());
		m_context->PSSetSamplers(0, PSO::samplerStates.size(), PSO::samplerStates.data());
		
		std::vector<ID3D11ShaderResourceView*> srvs = {
			m_envSRV.Get(), m_diffuseSRV.Get(), m_specularSRV.Get() 
		}; 
		m_context->PSSetShaderResources(10, srvs.size(), srvs.data());

		SetGlobalConstant(); 

		m_context->OMSetRenderTargets(1, postProcessRTV.GetAddressOf(), m_DSV.Get());
		float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_context->ClearRenderTargetView(postProcessRTV.Get(), color);  
		m_context->ClearDepthStencilView(m_DSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0.0f);
		
		//SetGraphicsPSO(PSO::billboardPSO); 
		
		//billboards.Render(m_context); 

		//SetGraphicsPSO(useWire ? PSO::tessellationQuadWirePSO : PSO::tessellationQuadSolidPSO); 

		//tessellationQuad.Render(m_context); 

		SetGraphicsPSO(useWire ? PSO::cubeMapWirePSO : PSO::cubeMapSolidPSO); 
		
		m_envMap->Render(m_context); 

		if (useBackFaceCull) SetGraphicsPSO(useWire ? PSO::defaultWirePSO : PSO::defaultSolidPSO); 
		else SetGraphicsPSO(useWire ? PSO::wireNoneCullPSO : PSO::solidNoneCullPSO); 

		for (auto& a : m_objectList) {
			a->Render(m_context);
		}

		if (useNormal) { 
			SetGraphicsPSO(PSO::normalPSO); 

			for (auto& a : m_objectList) {
				a->NormalRender(m_context);
			}
		} 

		SetGraphicsPSO(PSO::postProcessPSO); 
		postProcess.Render(m_context); 
	}

	namespace GUI {
		float diffuse;
		float specular; 
	}

	void MainEngine::UpdateGUI() { 

		if (ImGui::TreeNode("Resterizer")) {
			ImGui::Checkbox("useWire", &useWire);
			ImGui::Checkbox("useNormal", &useNormal);
			ImGui::Checkbox("useBackFaceCull", &useBackFaceCull); 
			ImGui::SliderFloat("envStrength", &globalConstantCPU.envStrength, 0.0f, 5.0f);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Phong")) {
			ImGui::SliderFloat("diffuse", &GUI::diffuse, 0.0f, 3.0f);
			ImGui::SliderFloat("specular", &GUI::specular, 0.0f, 3.0f);
			ImGui::SliderFloat("shininess", &m_objectList[0]->materialConstantCPU.mat.shininess, 0.0f, 256.0f);
			
			ImGui::TreePop();
		}

		m_objectList[0]->materialConstantCPU.mat.diffuse = Vector3(GUI::diffuse);
		m_objectList[0]->materialConstantCPU.mat.specular = Vector3(GUI::specular);

		if (ImGui::TreeNode("Rim")) {
			ImGui::CheckboxFlags("useRim", &m_objectList[0]->materialConstantCPU.rim.useRim, 1);
			ImGui::SliderFloat3("color", &m_objectList[0]->materialConstantCPU.rim.color.x, 0.0f, 1.0f);
			ImGui::SliderFloat("strength", &m_objectList[0]->materialConstantCPU.rim.strength, 0.0f, 10.0f);
			ImGui::SliderFloat("factor", &m_objectList[0]->materialConstantCPU.rim.factor, 0.0f, 10.0f);
			ImGui::CheckboxFlags("useSmoothStep", &m_objectList[0]->materialConstantCPU.rim.useSmoothStep, 1);
			
			ImGui::TreePop();
		} 

		if (ImGui::TreeNode("Fresnel")) {
			ImGui::CheckboxFlags("useFresnel", &m_objectList[0]->materialConstantCPU.fresnel.useFresnel, 1);
			ImGui::SliderFloat3("fresnelR0", &m_objectList[0]->materialConstantCPU.fresnel.fresnelR0.x, 0.0f, 1.0f);

			ImGui::TreePop();
		}
		
		if (ImGui::TreeNode("PostProcess")) { 
			ImGui::CheckboxFlags("useSampling", &postProcess.samplingFilter.useSamplingFilter, 1); 
			ImGui::SliderFloat("strength", &postProcess.combineFilter.imageFilterConstantCPU.bloomStrength, 0.0, 1.0);
			ImGui::CheckboxFlags("useToneMapping", &postProcess.combineFilter.imageFilterConstantCPU.useToneMapping, 1); 
			ImGui::SliderFloat("exposure", &postProcess.combineFilter.imageFilterConstantCPU.exposure, 0.0, 3.0);
			ImGui::SliderFloat("gamma", &postProcess.combineFilter.imageFilterConstantCPU.gamma, 0.0, 10.0);

			ImGui::TreePop(); 
		} 
		
		if (ImGui::TreeNode("Texture")) {
			ImGui::CheckboxFlags("useTextureLOD", &m_objectList[0]->materialConstantCPU.texture.useTextureLOD, 1); 
			ImGui::SliderFloat("mipLevel", &m_objectList[0]->materialConstantCPU.texture.mipLevel, 0.0f, 10.0f); 
			ImGui::CheckboxFlags("useNormalMap", &m_objectList[0]->materialConstantCPU.texture.useNormalMap, 1); 
			ImGui::CheckboxFlags("useHeightMap", &m_objectList[0]->modelConstantCPU.useHeightMap, 1); 
			ImGui::CheckboxFlags("useAO", &m_objectList[0]->materialConstantCPU.texture.useAO, 1); 

			ImGui::TreePop(); 
		}
	}
}
