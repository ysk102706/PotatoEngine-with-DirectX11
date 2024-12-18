#include "Model.h"
#include <memory>
#include "D3D11Utils.h"
#include <directxtk/SimpleMath.h>
#include "ResourceLoader.h"

namespace Engine
{
	using DirectX::SimpleMath::Matrix;

	Engine::Model::Model()
	{
	}

	Engine::Model::Model(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context, const std::vector<MeshData>& meshData)
	{
		Initialize(device, context, meshData);
	}

	void Engine::Model::Initialize(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context, const std::vector<MeshData>& meshData)
	{
		ZeroMemory(&modelConstantCPU, sizeof(modelConstantCPU)); 
		ZeroMemory(&materialConstantCPU, sizeof(materialConstantCPU));

		modelConstantCPU.world = modelConstantCPU.invTranspose = Matrix(); 

		D3D11Utils::CreateConstantBuffer(device, modelConstantCPU, modelConstantGPU);
		D3D11Utils::CreateConstantBuffer(device, materialConstantCPU, materialConstantGPU);

		for (const auto& a : meshData) {
			auto newMesh = std::make_shared<Mesh>();
			newMesh->stride = sizeof(Vertex);
			newMesh->vertexCount = a.vertices.size();
			newMesh->indexCount = a.indices.size();

			D3D11Utils::CreateVertexBuffer(device, a.vertices, newMesh->vertexBuffer);
			D3D11Utils::CreateIndexBuffer(device, a.indices, newMesh->indexBuffer);

			if (!a.albedoTextureFile.empty()) {
				ResourceLoader::CreateMipMapTexture(device, context, a.albedoTextureFile, newMesh->albedoTexture, newMesh->albedoSRV);
			}

			if (!a.normalMapTextureFile.empty()) {
				ResourceLoader::CreateTexture(device, context, a.normalMapTextureFile, newMesh->normalMapTexture, newMesh->normalMapSRV);
			}

			if (!a.heightMapTextureFile.empty()) {
				ResourceLoader::CreateTexture(device, context, a.heightMapTextureFile, newMesh->heightMapTexture, newMesh->heightMapSRV);
			}

			if (!a.AOTextureFile.empty()) {
				ResourceLoader::CreateTexture(device, context, a.AOTextureFile, newMesh->AOTexture, newMesh->AOSRV);
			}

			newMesh->vertexConstantBuffer = modelConstantGPU;
			newMesh->pixelConstantBuffer = materialConstantGPU;

			meshes.push_back(newMesh);
		}
	}

	void Model::Render(ComPtr<ID3D11DeviceContext>& context)
	{
		if (isVisible) {
			for (const auto& a : meshes) {
				context->VSSetConstantBuffers(0, 1, a->vertexConstantBuffer.GetAddressOf());
				context->PSSetConstantBuffers(0, 1, a->pixelConstantBuffer.GetAddressOf());

				std::vector<ID3D11ShaderResourceView*> VSSRVs = {
					a->heightMapSRV.Get()
				};
				context->VSSetShaderResources(0, VSSRVs.size(), VSSRVs.data());

				std::vector<ID3D11ShaderResourceView*> PSSRVs = {
					a->albedoSRV.Get(), a->normalMapSRV.Get(), a->AOSRV.Get() 
				};
				context->PSSetShaderResources(0, PSSRVs.size(), PSSRVs.data());

				context->IASetVertexBuffers(0, 1, a->vertexBuffer.GetAddressOf(), &a->stride, &a->offset);
				context->IASetIndexBuffer(a->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
				context->DrawIndexed(a->indexCount, 0, 0);
			}
		}
	} 

	void Model::NormalRender(ComPtr<ID3D11DeviceContext>& context)
	{
		for (const auto& a : meshes) {
			context->GSSetConstantBuffers(0, 1, a->vertexConstantBuffer.GetAddressOf());

			context->IASetVertexBuffers(0, 1, a->vertexBuffer.GetAddressOf(), &a->stride, &a->offset);
			context->Draw(a->vertexCount, 0);
		}
	}

	void Model::UpdateConstantBuffer(ComPtr<ID3D11DeviceContext>& context)
	{
		D3D11Utils::UpdateConstantBuffer(context, modelConstantCPU, modelConstantGPU);
		D3D11Utils::UpdateConstantBuffer(context, materialConstantCPU, materialConstantGPU);
	} 
}
