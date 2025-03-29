#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"winmm.lib")

#include<Windows.h>
#include<dxgi1_6.h>
#include<cassert>
#include<fstream>
#include<sstream>
#include"graphic.h"

//グローバル変数-----------------------------------------------------------------
// ウィンドウ
LPCWSTR	WindowTitle;
int ClientWidth;
int ClientHeight;
int ClientPosX;
int ClientPosY;
float Aspect;
DWORD WindowStyle;
HWND HWnd;
// デバイス
ComPtr<ID3D12Device> Device;
// コマンド
ComPtr<ID3D12CommandAllocator> CommandAllocator;
ComPtr<ID3D12GraphicsCommandList> CommandList;
ComPtr<ID3D12CommandQueue> CommandQueue;
// フェンス
ComPtr<ID3D12Fence> Fence;
HANDLE FenceEvent;
UINT64 FenceValue;
// デバッグ
HRESULT Hr;
// バックバッファ
ComPtr<IDXGISwapChain4> SwapChain;
ComPtr<ID3D12Resource> BackBuffers[2];
UINT BackBufIdx;
ComPtr<ID3D12DescriptorHeap> BbvHeap;//"Bbv"は"BackBufView"の略
UINT BbvIncSize;
//バックバッファクリアカラー
float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
// デプスステンシルバッファ
ComPtr<ID3D12Resource> DepthStencilBuffer;
ComPtr<ID3D12DescriptorHeap> DsvHeap;//"Dsv"は"DepthStencilBufferView"の略
// パイプライン
ComPtr<ID3D12RootSignature> RootSignature;
ComPtr<ID3D12PipelineState> PipelineState;
D3D12_VIEWPORT Viewport;
D3D12_RECT ScissorRect;
//　コンスタントバッファ系ディスクリプタヒープ
ComPtr<ID3D12DescriptorHeap> CbvTbvHeap = nullptr;
UINT CbvTbvIncSize = 0;
UINT CbvTbvCurrentIdx = 0;
//　バリア（グローバルにするのは危険感も）
D3D12_RESOURCE_BARRIER Barrier = {};

//プライベートな関数--------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_KEYDOWN:
		if (wp == VK_ESCAPE){
			DestroyWindow(hwnd);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wp, lp);
	}
}
void CreateWindows()
{
	//ウィンドウクラス登録
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = GetModuleHandle(0);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	windowClass.lpszClassName = L"GAME_WINDOW";
	RegisterClassEx(&windowClass);
	//表示位置、ウィンドウの大きさ調整
	RECT windowRect = { 0, 0, ClientWidth, ClientHeight };
	AdjustWindowRect(&windowRect, WindowStyle, FALSE);
	int windowPosX = ClientPosX + windowRect.left;
	int windowPosY = ClientPosY + windowRect.top;
	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;
	//ウィンドウをつくる
	HWnd = CreateWindowEx(
		NULL,
		L"GAME_WINDOW",
		WindowTitle,
		WindowStyle,
		windowPosX,
		windowPosY,
		windowWidth,
		windowHeight,
		NULL,		//親ウィンドウなし
		NULL,		//メニューなし
		GetModuleHandle(0),
		NULL);		//複数ウィンドウなし
}
void CreateDevice()
{
#ifdef _DEBUG
	//デバッグモードでは、デバッグレイヤーを有効化する
	ComPtr<ID3D12Debug> debug;
	Hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	assert(SUCCEEDED(Hr));
	debug->EnableDebugLayer();
#endif
	//デバイスをつくる(簡易バージョン)
	{
		Hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(Device.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//コマンド
	{
		//コマンドアロケータをつくる
		Hr = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(CommandAllocator.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//コマンドリストをつくる
		Hr = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			CommandAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//コマンドキューをつくる
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;		//GPUタイムアウトが有効
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;		//直接コマンドキュー
		Hr = Device->CreateCommandQueue(&desc, IID_PPV_ARGS(CommandQueue.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//フェンス
	{
		//GPUの処理完了をチェックするフェンスをつくる
		Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetAddressOf()));
		assert(SUCCEEDED(Hr));
		FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		assert(FenceEvent != nullptr);
		FenceValue = 1;
	}
}
void CreateRenderTarget()
{
	//スワップチェインをつくる(ここにバックバッファが含まれている)
	{
		//DXGIファクトリをつくる
		ComPtr<IDXGIFactory4> dxgiFactory;
		Hr = CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//スワップチェインをつくる
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.BufferCount = 2; //バックバッファ2枚
		desc.Width = ClientWidth;
		desc.Height = ClientHeight;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.SampleDesc.Count = 1;
		ComPtr<IDXGISwapChain1> swapChain1;
		Hr = dxgiFactory->CreateSwapChainForHwnd(
			CommandQueue.Get(), HWnd, &desc, nullptr, nullptr, swapChain1.GetAddressOf());
		assert(SUCCEEDED(Hr));

		//IDXGISwapChain4インターフェイスをサポートしているか尋ねる
		Hr = swapChain1->QueryInterface(IID_PPV_ARGS(SwapChain.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//バックバッファ「ビュー」の入れ物である「ディスクリプタヒープ」をつくる
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 2;//バックバッファビュー２つ
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//RenderTargetView
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//シェーダからアクセスしないのでNONEでOK
		Hr = Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(BbvHeap.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//バックバッファ「ビュー」を「ディスクリプタヒープ」につくる
	{
		D3D12_CPU_DESCRIPTOR_HANDLE hBbvHeap
			= BbvHeap->GetCPUDescriptorHandleForHeapStart();

		BbvIncSize
			= Device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (UINT idx = 0; idx < 2; idx++) {
			//バックバッファを取り出す
			Hr = SwapChain->GetBuffer(idx, IID_PPV_ARGS(BackBuffers[idx].GetAddressOf()));
			assert(SUCCEEDED(Hr));
			//バックバッファのビューをヒープにつくる
			hBbvHeap.ptr += BbvIncSize * idx;
			Device->CreateRenderTargetView(BackBuffers[idx].Get(), nullptr, hBbvHeap);
		}
	}
	//デプスステンシルバッファをつくる
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;//DEFAULTだから後はUNKNOWNでよし
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元のテクスチャデータとして
		desc.Width = ClientWidth;//幅と高さはレンダーターゲットと同じ
		desc.Height = ClientHeight;//上に同じ
		desc.DepthOrArraySize = 1;//テクスチャ配列でもないし3Dテクスチャでもない
		desc.Format = DXGI_FORMAT_D32_FLOAT;//深度値書き込み用フォーマット
		desc.SampleDesc.Count = 1;//サンプルは1ピクセル当たり1つ
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//このバッファは深度ステンシルとして使用します
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		//デプスステンシルバッファをクリアする値
		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.DepthStencil.Depth = 1.0f;//深さ１(最大値)でクリア
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit深度値としてクリア
		//デプスステンシルバッファを作る
		Hr = Device->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
			&depthClearValue,
			IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//デプスステンシルバッファ「ビュー」の入れ物である「デスクリプタヒープ」をつくる
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};//深度に使うよという事がわかればいい
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//デプスステンシルビューとして使う
		desc.NumDescriptors = 1;//深度ビュー1つのみ
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Hr = Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(DsvHeap.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//デプスステンシルバッファ「ビュー」を「ディスクリプタヒープ」につくる
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_D32_FLOAT;//デプス値に32bit使用
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2Dテクスチャ
		desc.Flags = D3D12_DSV_FLAG_NONE;//フラグは特になし
		D3D12_CPU_DESCRIPTOR_HANDLE hDsvHeap
			= DsvHeap->GetCPUDescriptorHandleForHeapStart();
		Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &desc, hDsvHeap);
	}
}
void CreatePipeline()
{
	//ルートシグネチャ
	{
		//ディスクリプタレンジ。ディスクリプタヒープとシェーダを紐づける役割をもつ。
		D3D12_DESCRIPTOR_RANGE  range[1] = {};
		UINT b0 = 0;
		range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range[0].BaseShaderRegister = b0;
		range[0].NumDescriptors = 1;
		range[0].RegisterSpace = 0;
		range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//ルートパラメタをディスクリプタテーブルとして使用
		D3D12_ROOT_PARAMETER rootParam[1] = {};
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.pDescriptorRanges = range;
		rootParam[0].DescriptorTable.NumDescriptorRanges = _countof(range);
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//サンプラの記述。このサンプラがシェーダーの s0 にセットされる
		D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補間しない(ニアレストネイバー)
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//横繰り返し
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//縦繰り返し
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//奥行繰り返し
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//ミップマップ最大値
		samplerDesc[0].MinLOD = 0.0f;//ミップマップ最小値
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//オーバーサンプリングの際リサンプリングしない？
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視

		//ルートシグニチャの記述
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.pParameters = rootParam;
		desc.NumParameters = _countof(rootParam);
		//desc.pStaticSamplers = samplerDesc;//サンプラーの先頭アドレス
		//desc.NumStaticSamplers = _countof(samplerDesc);//サンプラー数

		//ルートシグネチャをシリアライズ⇒blob(塊)をつくる。
		ComPtr<ID3DBlob> blob;
		Hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
		assert(SUCCEEDED(Hr));

		//ルートシグネチャをつくる
		Hr = Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(RootSignature.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}

	//コンパイル済みシェーダを読み込むファイルバッファ
	class BIN_FILE12 {
	public:
		BIN_FILE12(const char* fileName) :Succeeded(false)
		{
			std::ifstream ifs(fileName, std::ios::binary);
			if (ifs.fail()) {
				return;
			}
			Succeeded = true;
			std::istreambuf_iterator<char> first(ifs);
			std::istreambuf_iterator<char> last;
			Buffer.assign(first, last);
			ifs.close();
		}
		bool succeeded() const
		{
			return Succeeded;
		}
		unsigned char* code() const
		{
			char* p = const_cast<char*>(Buffer.data());
			return reinterpret_cast<unsigned char*>(p);
		}
		size_t size() const
		{
			return Buffer.size();
		}
	private:
		std::string Buffer;
		bool Succeeded;
	};
	//シェーダ読み込み
	BIN_FILE12 vs("assets\\VertexShader.cso");
	assert(vs.succeeded());
	BIN_FILE12 ps("assets\\PixelShader.cso");
	assert(ps.succeeded());

	//以下、各種記述

	UINT slot0 = 0;
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    slot0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FrontCounterClockwise = true;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.ForcedSampleCount = 0;
	rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = true;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].LogicOpEnable = false;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[1].BlendEnable = true;
	blendDesc.RenderTarget[1].LogicOpEnable = false;
	blendDesc.RenderTarget[1].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[1].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[1].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[1].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//全て書き込み
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;//小さい方を採用
	depthStencilDesc.StencilEnable = false;//ステンシルバッファは使わない


	//ここまでの記述をまとめてパイプラインステートオブジェクトをつくる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = RootSignature.Get();
	pipelineDesc.VS = { vs.code(), vs.size() };
	pipelineDesc.PS = { ps.code(), ps.size() };
	pipelineDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineDesc.RasterizerState = rasterDesc;
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineDesc.SampleMask = UINT_MAX;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 2;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	Hr = Device->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(PipelineState.GetAddressOf())
	);
	assert(SUCCEEDED(Hr));

	//出力領域を設定
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = (float)ClientWidth;
	Viewport.Height = (float)ClientHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	//切り取り矩形を設定
	ScissorRect.left = 0;
	ScissorRect.top = 0;
	ScissorRect.right = ClientWidth;
	ScissorRect.bottom = ClientHeight;
}
void createPipeline(const char* pixelShader)
{
	//ルートシグネチャ
	{
		//ディスクリプタレンジ。ディスクリプタヒープとシェーダを紐づける役割をもつ。
		D3D12_DESCRIPTOR_RANGE  range[1] = {};
		UINT b0 = 0;
		range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range[0].BaseShaderRegister = b0;
		range[0].NumDescriptors = 1;
		range[0].RegisterSpace = 0;
		range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//ルートパラメタをディスクリプタテーブルとして使用
		D3D12_ROOT_PARAMETER rootParam[1] = {};
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.pDescriptorRanges = range;
		rootParam[0].DescriptorTable.NumDescriptorRanges = _countof(range);
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//サンプラの記述。このサンプラがシェーダーの s0 にセットされる
		D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//補間しない(ニアレストネイバー)
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//横繰り返し
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//縦繰り返し
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//奥行繰り返し
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//ボーダーの時は黒
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//ミップマップ最大値
		samplerDesc[0].MinLOD = 0.0f;//ミップマップ最小値
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//オーバーサンプリングの際リサンプリングしない？
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//ピクセルシェーダからのみ可視

		//ルートシグニチャの記述
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.pParameters = rootParam;
		desc.NumParameters = _countof(rootParam);
		//desc.pStaticSamplers = samplerDesc;//サンプラーの先頭アドレス
		//desc.NumStaticSamplers = _countof(samplerDesc);//サンプラー数

		//ルートシグネチャをシリアライズ⇒blob(塊)をつくる。
		ComPtr<ID3DBlob> blob;
		Hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
		assert(SUCCEEDED(Hr));

		//ルートシグネチャをつくる
		Hr = Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(RootSignature.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}

	//コンパイル済みシェーダを読み込むファイルバッファ
	class BIN_FILE12 {
	public:
		BIN_FILE12(const char* fileName) :Succeeded(false)
		{
			std::ifstream ifs(fileName, std::ios::binary);
			if (ifs.fail()) {
				return;
			}
			Succeeded = true;
			std::istreambuf_iterator<char> first(ifs);
			std::istreambuf_iterator<char> last;
			Buffer.assign(first, last);
			ifs.close();
		}
		bool succeeded() const
		{
			return Succeeded;
		}
		unsigned char* code() const
		{
			char* p = const_cast<char*>(Buffer.data());
			return reinterpret_cast<unsigned char*>(p);
		}
		size_t size() const
		{
			return Buffer.size();
		}
	private:
		std::string Buffer;
		bool Succeeded;
	};
	//シェーダ読み込み
	BIN_FILE12 vs("assets\\VertexShader.cso");
	assert(vs.succeeded());
    std::stringstream ss;
    ss << "assets\\" << pixelShader << ".cso";
	BIN_FILE12 ps(ss.str().c_str());
	assert(ps.succeeded());

	//以下、各種記述

	UINT slot0 = 0;
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    slot0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FrontCounterClockwise = true;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.ForcedSampleCount = 0;
	rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = true;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].LogicOpEnable = false;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[1].BlendEnable = true;
	blendDesc.RenderTarget[1].LogicOpEnable = false;
	blendDesc.RenderTarget[1].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[1].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[1].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[1].LogicOp = D3D12_LOGIC_OP_NOOP;
	blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//全て書き込み
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;//小さい方を採用
	depthStencilDesc.StencilEnable = false;//ステンシルバッファは使わない


	//ここまでの記述をまとめてパイプラインステートオブジェクトをつくる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = RootSignature.Get();
	pipelineDesc.VS = { vs.code(), vs.size() };
	pipelineDesc.PS = { ps.code(), ps.size() };
	pipelineDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineDesc.RasterizerState = rasterDesc;
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineDesc.SampleMask = UINT_MAX;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 2;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	Hr = Device->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(PipelineState.GetAddressOf())
	);
	assert(SUCCEEDED(Hr));

	//出力領域を設定
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = (float)ClientWidth;
	Viewport.Height = (float)ClientHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	//切り取り矩形を設定
	ScissorRect.left = 0;
	ScissorRect.top = 0;
	ScissorRect.right = ClientWidth;
	ScissorRect.bottom = ClientHeight;
}

//パブリックな関数---------------------------------------------------------------
//システム系
void window(LPCWSTR windowTitle, int clientWidth, int clientHeight, bool windowed, int clientPosX, int clientPosY)
{
	WindowTitle = windowTitle;
	ClientWidth = clientWidth;
	ClientHeight = clientHeight;
	ClientPosX = (GetSystemMetrics(SM_CXSCREEN) - ClientWidth) / 2;//中央表示
	if (clientPosX >= 0)ClientPosX = clientPosX;
	ClientPosY = (GetSystemMetrics(SM_CYSCREEN) - ClientHeight) / 2;//中央表示
	if (clientPosY >= 0)ClientPosY = clientPosY;
	Aspect = static_cast<float>(ClientWidth) / ClientHeight;
	WindowStyle = WS_POPUP;//Alt + F4で閉じる
	if (windowed) WindowStyle = WS_OVERLAPPEDWINDOW;

	CreateWindows();
	CreateDevice();
	CreateRenderTarget();
	//CreatePipeline();

	//ウィンドウ表示
	ShowWindow(HWnd, SW_SHOW);
}
bool quit()
{
	MSG msg = { 0 };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT)return true;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return false;
}
void waitGPU()
{
	//現在のFence値がコマンド終了後にFenceに書き込まれるようにする
	UINT64 fvalue = FenceValue;
	CommandQueue->Signal(Fence.Get(), fvalue);
	FenceValue++;

	//まだコマンドキューが終了していないことを確認する
	if (Fence->GetCompletedValue() < fvalue)
	{
		//このFenceにおいて、fvalue の値になったらイベントを発生させる
		Fence->SetEventOnCompletion(fvalue, FenceEvent);
		//イベントが発生するまで待つ
		WaitForSingleObject(FenceEvent, INFINITE);
	}
}
void closeEventHandle()
{
	CloseHandle(FenceEvent);
}
//コンスタントバッファ、テクスチャバッファのディスクリプタヒープ
HRESULT createDescriptorHeap(UINT numDescriptors)
{
	CbvTbvIncSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CbvTbvCurrentIdx = 0;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = numDescriptors;
	desc.NodeMask = 0;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	return Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(CbvTbvHeap.ReleaseAndGetAddressOf()));
}
//バッファ系
HRESULT createBuffer(UINT sizeInBytes, ComPtr<ID3D12Resource>& buffer)
{
	D3D12_HEAP_PROPERTIES prop = {};
	prop.Type = D3D12_HEAP_TYPE_UPLOAD;
	prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	prop.CreationNodeMask = 1;
	prop.VisibleNodeMask = 1;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = sizeInBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	return Device->CreateCommittedResource(
		&prop,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf()));
}
UINT alignedSize(size_t size)
{
	return (size + 0xff) & ~0xff;
}
HRESULT updateBuffer(void* data, UINT sizeInBytes, ComPtr<ID3D12Resource>& buffer)
{
	UINT8* mappedBuffer;
	Hr = buffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedBuffer));
	if (FAILED(Hr))return E_FAIL;
	memcpy(mappedBuffer, data, sizeInBytes);
	buffer->Unmap(0, nullptr);
	return S_OK;
}
HRESULT mapBuffer(ComPtr<ID3D12Resource>& buffer, void** mappedBuffer)
{
	return buffer->Map(0, nullptr, mappedBuffer);
}
void unmapBuffer(ComPtr<ID3D12Resource>& buffer)
{
	buffer->Unmap(0, nullptr);
}
HRESULT createTextureBuffer(const char* filename, ComPtr<ID3D12Resource>& textureBuf)
{
	//ファイルを読み込み、生データを取り出す
	unsigned char* pixels = nullptr;
	int width = 0, height = 0, bytePerPixel = 4;
	//pixels = stbi_load(filename, &width, &height, nullptr, bytePerPixel);
	if (pixels == nullptr)
	{
		MessageBoxA(0, filename, "ファイルがないっす", 0);
		exit(0);
	}

	//１行のピッチを256の倍数にしておく(バッファサイズは256の倍数でなければいけない)
	const UINT64 alignedRowPitch = (width * bytePerPixel + 0xff) & ~0xff;

	//アップロード用中間バッファをつくり、生データをコピーしておく
	ComPtr<ID3D12Resource> uploadBuf;
	{
		//テクスチャではなくフツーのバッファとしてつくる
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = alignedRowPitch * height;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Hr = Device->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuf));
		assert(SUCCEEDED(Hr));

		//生データをuploadbuffに一旦コピーします
		uint8_t* mapBuf = nullptr;
		Hr = uploadBuf->Map(0, nullptr, (void**)&mapBuf);//マップ
		auto srcAddress = pixels;
		auto originalRowPitch = width * bytePerPixel;
		for (int y = 0; y < height; ++y) {
			memcpy(mapBuf, srcAddress, originalRowPitch);
			//1行ごとの辻褄を合わせてやる
			mapBuf += alignedRowPitch;
			srcAddress += originalRowPitch;
		}
		uploadBuf->Unmap(0, nullptr);//アンマップ
	}

	//そして、最終コピー先であるテクスチャバッファを作る
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;//CPUからアクセスしない。処理が速い。
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//他のバッファと違う
		desc.Alignment = 0;
		desc.Width = width;
		desc.Height = height;//他のバッファと違う
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//他のバッファと違う
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//他のバッファと違う
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		Hr = Device->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(textureBuf.ReleaseAndGetAddressOf()));
		assert(SUCCEEDED(Hr));
	}

	//GPUでuploadBufからtextureBufへコピーする長い道のりが始まります

	//まずコピー元ロケーションの準備・フットプリント指定
	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = uploadBuf.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Footprint.Width = static_cast<UINT>(width);
	src.PlacedFootprint.Footprint.Height = static_cast<UINT>(height);
	src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(1);
	src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(alignedRowPitch);
	src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//コピー先ロケーションの準備・サブリソースインデックス指定
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = textureBuf.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	//コマンドリストでコピーを予約しますよ！！！
	CommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	//ってことはバリアがいるのです
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureBuf.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);
	//uploadBufアンロード
	CommandList->DiscardResource(uploadBuf.Get(), nullptr);
	//コマンドリストを閉じて
	CommandList->Close();
	//実行
	ID3D12CommandList* commandLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	//リソースがGPUに転送されるまで待機する
	waitGPU();

	//コマンドアロケータをリセット
	HRESULT Hr = CommandAllocator->Reset();
	assert(SUCCEEDED(Hr));
	//コマンドリストをリセット
	Hr = CommandList->Reset(CommandAllocator.Get(), nullptr);
	assert(SUCCEEDED(Hr));

	//開放
	//stbi_image_free(pixels);

	return S_OK;
}
//ビュー（ディスクリプタ）系
void createVertexBufferView(ComPtr<ID3D12Resource>& vertexBuffer, UINT sizeInBytes, UINT strideInBytes, D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
{
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeInBytes;
	vertexBufferView.StrideInBytes = strideInBytes;
}
void createIndexBufferView(ComPtr<ID3D12Resource>& indexBuffer, UINT sizeInBytes, D3D12_INDEX_BUFFER_VIEW& indexBufferView)
{
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeInBytes;
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
}
UINT createConstantBufferView(ComPtr<ID3D12Resource>& constantBuffer)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
	desc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
	desc.SizeInBytes = static_cast<UINT>(constantBuffer->GetDesc().Width);
	auto hCbvTbvHeap = CbvTbvHeap->GetCPUDescriptorHandleForHeapStart();
	hCbvTbvHeap.ptr += CbvTbvIncSize * CbvTbvCurrentIdx;
	Device->CreateConstantBufferView(&desc, hCbvTbvHeap);
	return CbvTbvCurrentIdx++;
}
UINT createTextureBufferView(ComPtr<ID3D12Resource>& textureBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = textureBuffer->GetDesc().Format;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	auto hCbvTbvHeap = CbvTbvHeap->GetCPUDescriptorHandleForHeapStart();
	hCbvTbvHeap.ptr += CbvTbvIncSize * CbvTbvCurrentIdx;
	Device->CreateShaderResourceView(textureBuffer.Get(), &desc, hCbvTbvHeap);
	return CbvTbvCurrentIdx++;
}
//描画系
void setClearColor(float r, float g, float b)
{
	ClearColor[0] = r;	ClearColor[1] = g;	ClearColor[2] = b;
}
void beginRender()
{
	//現在のバックバッファのインデックスを取得。このプログラムの場合0 or 1になる。
	BackBufIdx = SwapChain->GetCurrentBackBufferIndex();

	//バリアでバックバッファを描画ターゲットに切り替える
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//このバリアは状態遷移タイプ
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = BackBuffers[BackBufIdx].Get();//リソースはバックバッファ
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;//遷移前はPresent
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;//遷移後は描画ターゲット
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);

	//バックバッファの場所を指すディスクリプタヒープハンドルを用意する
	auto hBbvHeap = BbvHeap->GetCPUDescriptorHandleForHeapStart();
	hBbvHeap.ptr += BackBufIdx * BbvIncSize;
	//デプスステンシルバッファのディスクリプタハンドルを用意する
	auto hDsvHeap = DsvHeap->GetCPUDescriptorHandleForHeapStart();
	//バックバッファとデプスステンシルバッファを描画ターゲットとして設定する
	CommandList->OMSetRenderTargets(1, &hBbvHeap, false, &hDsvHeap);

	//描画ターゲットをクリアする
	CommandList->ClearRenderTargetView(hBbvHeap, ClearColor, 0, nullptr);

	//デプスステンシルバッファをクリアする
	CommandList->ClearDepthStencilView(hDsvHeap, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	CommandList->RSSetViewports(1, &Viewport);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	CommandList->SetPipelineState(PipelineState.Get());
	CommandList->SetGraphicsRootSignature(RootSignature.Get());

	CommandList->SetDescriptorHeaps(1, CbvTbvHeap.GetAddressOf());
}
void drawMesh(D3D12_VERTEX_BUFFER_VIEW& vbv, UINT cbvTbvIdx)
{
	CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	CommandList->IASetVertexBuffers(0, 1, &vbv);
	auto hCbvTbvHeap = CbvTbvHeap->GetGPUDescriptorHandleForHeapStart();
	hCbvTbvHeap.ptr += CbvTbvIncSize * cbvTbvIdx;
	CommandList->SetGraphicsRootDescriptorTable(0, hCbvTbvHeap);
	CommandList->DrawInstanced(4, 1, 0, 0);
}
void drawMesh(D3D12_VERTEX_BUFFER_VIEW& vbv, D3D12_INDEX_BUFFER_VIEW& ibv, UINT cbvTbvIdx)
{
	CommandList->IASetVertexBuffers(0, 1, &vbv);
	CommandList->IASetIndexBuffer(&ibv);
	auto hCbvTbvHeap = CbvTbvHeap->GetGPUDescriptorHandleForHeapStart();
	hCbvTbvHeap.ptr += CbvTbvIncSize * cbvTbvIdx;
	CommandList->SetGraphicsRootDescriptorTable(0, hCbvTbvHeap);
	UINT numIndices = ibv.SizeInBytes / sizeof(UINT16);
	CommandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
}
void endRender()
{
	//バリアでバックバッファを表示用に切り替える
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//このバリアは状態遷移タイプ
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = BackBuffers[BackBufIdx].Get();//リソースはバックバッファ
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;//遷移前は描画ターゲット
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;//遷移後はPresent
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);

	//コマンドリストをクローズする
	CommandList->Close();
	//コマンドリストを実行する
	ID3D12CommandList* commandLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	//描画完了を待つ
	waitGPU();

	//バックバッファを表示
	SwapChain->Present(0, 0);

	//コマンドアロケータをリセット
	Hr = CommandAllocator->Reset();
	assert(SUCCEEDED(Hr));
	//コマンドリストをリセット
	Hr = CommandList->Reset(CommandAllocator.Get(), nullptr);
	assert(SUCCEEDED(Hr));
}
//Get系
ComPtr<ID3D12Device>& device()
{
	return Device;
}
ComPtr<ID3D12GraphicsCommandList>& commandList()
{
	return CommandList;
}
UINT cbvTbvIncSize()
{
	return Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
float aspect()
{
	return Aspect;
}
float width()
{
    return static_cast<float>(ClientWidth);
}
float height()
{
    return static_cast<float>(ClientHeight);
}
