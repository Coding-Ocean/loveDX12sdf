#include"graphic.h"

//頂点バッファ
ComPtr<ID3D12Resource>   VertexBuffer;
D3D12_VERTEX_BUFFER_VIEW Vbv;
//コンスタントバッファ０
ComPtr<ID3D12Resource> ConstBuffer0;
UINT CbvIdx;
//コンスタントバッファ０構造体。Header.hlsliと同じ並びにしておく
struct CONST_BUF0 
{
	float time;
    float width;
	float height;
};
struct CONST_BUF0* CB0;

//Entry point
INT WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ INT)
{
	window(L"RayMarching", 1920, 1080, NO_WINDOW);
	createPipeline("PixelShader2");

	//リソース初期化
	{
		//頂点バッファ
		{
			//texcoordは、xを-aspect~aspect,　yを1~-1とする
			float vertices[] = {
				//position            texcoord
				-1.0f,  1.0f,  0.0f,  0,		height(), //左上
				-1.0f, -1.0f,  0.0f,  0,		0, //左下
				 1.0f,  1.0f,  0.0f,  width(),  height(), //右上
				 1.0f, -1.0f,  0.0f,  width(),  0, //右下
			};
			unsigned numVertexElements = 5;
			//データサイズを求めておく
			UINT sizeInBytes = sizeof(vertices);
			UINT strideInBytes = sizeof(float) * numVertexElements;
			//バッファをつくる
			createBuffer(sizeInBytes, VertexBuffer);
			//バッファにデータを入れる
			updateBuffer(vertices, sizeInBytes, VertexBuffer);
			//バッファビューをつくる
			createVertexBufferView(VertexBuffer, sizeInBytes, strideInBytes, Vbv);
		}
		//コンスタントバッファ０
		{
			//バッファをつくる
			createBuffer(alignedSize(sizeof(CONST_BUF0)), ConstBuffer0);
			//マップしておく
			mapBuffer(ConstBuffer0, (void**)&CB0);
			//１つのディスクリプタのヒープをつくる
			createDescriptorHeap(1);
			//ビューをつくってインデックスをもらっておく
			CbvIdx = createConstantBufferView(ConstBuffer0);
            CB0->width = width();
            CB0->height = height();
		}
	}

	timeBeginPeriod(1);
	DWORD startTime = timeGetTime();
	ShowCursor(false);

	while (!quit())
	{
		CB0->time = (timeGetTime() - startTime) / 1000.0f;
		beginRender();
		drawMesh(Vbv, CbvIdx);
		endRender();
	}

	waitGPU();
	closeEventHandle();
	unmapBuffer(ConstBuffer0);
	ShowCursor(true);
	timeEndPeriod(1);
}
