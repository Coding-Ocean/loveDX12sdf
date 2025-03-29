#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"winmm.lib")

#include<Windows.h>
#include<dxgi1_6.h>
#include<cassert>
#include<fstream>
#include<sstream>
#include"graphic.h"

//�O���[�o���ϐ�-----------------------------------------------------------------
// �E�B���h�E
LPCWSTR	WindowTitle;
int ClientWidth;
int ClientHeight;
int ClientPosX;
int ClientPosY;
float Aspect;
DWORD WindowStyle;
HWND HWnd;
// �f�o�C�X
ComPtr<ID3D12Device> Device;
// �R�}���h
ComPtr<ID3D12CommandAllocator> CommandAllocator;
ComPtr<ID3D12GraphicsCommandList> CommandList;
ComPtr<ID3D12CommandQueue> CommandQueue;
// �t�F���X
ComPtr<ID3D12Fence> Fence;
HANDLE FenceEvent;
UINT64 FenceValue;
// �f�o�b�O
HRESULT Hr;
// �o�b�N�o�b�t�@
ComPtr<IDXGISwapChain4> SwapChain;
ComPtr<ID3D12Resource> BackBuffers[2];
UINT BackBufIdx;
ComPtr<ID3D12DescriptorHeap> BbvHeap;//"Bbv"��"BackBufView"�̗�
UINT BbvIncSize;
//�o�b�N�o�b�t�@�N���A�J���[
float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
// �f�v�X�X�e���V���o�b�t�@
ComPtr<ID3D12Resource> DepthStencilBuffer;
ComPtr<ID3D12DescriptorHeap> DsvHeap;//"Dsv"��"DepthStencilBufferView"�̗�
// �p�C�v���C��
ComPtr<ID3D12RootSignature> RootSignature;
ComPtr<ID3D12PipelineState> PipelineState;
D3D12_VIEWPORT Viewport;
D3D12_RECT ScissorRect;
//�@�R���X�^���g�o�b�t�@�n�f�B�X�N���v�^�q�[�v
ComPtr<ID3D12DescriptorHeap> CbvTbvHeap = nullptr;
UINT CbvTbvIncSize = 0;
UINT CbvTbvCurrentIdx = 0;
//�@�o���A�i�O���[�o���ɂ���̂͊댯�����j
D3D12_RESOURCE_BARRIER Barrier = {};

//�v���C�x�[�g�Ȋ֐�--------------------------------------------------------------
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
	//�E�B���h�E�N���X�o�^
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = GetModuleHandle(0);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	windowClass.lpszClassName = L"GAME_WINDOW";
	RegisterClassEx(&windowClass);
	//�\���ʒu�A�E�B���h�E�̑傫������
	RECT windowRect = { 0, 0, ClientWidth, ClientHeight };
	AdjustWindowRect(&windowRect, WindowStyle, FALSE);
	int windowPosX = ClientPosX + windowRect.left;
	int windowPosY = ClientPosY + windowRect.top;
	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;
	//�E�B���h�E������
	HWnd = CreateWindowEx(
		NULL,
		L"GAME_WINDOW",
		WindowTitle,
		WindowStyle,
		windowPosX,
		windowPosY,
		windowWidth,
		windowHeight,
		NULL,		//�e�E�B���h�E�Ȃ�
		NULL,		//���j���[�Ȃ�
		GetModuleHandle(0),
		NULL);		//�����E�B���h�E�Ȃ�
}
void CreateDevice()
{
#ifdef _DEBUG
	//�f�o�b�O���[�h�ł́A�f�o�b�O���C���[��L��������
	ComPtr<ID3D12Debug> debug;
	Hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	assert(SUCCEEDED(Hr));
	debug->EnableDebugLayer();
#endif
	//�f�o�C�X������(�ȈՃo�[�W����)
	{
		Hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(Device.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�R�}���h
	{
		//�R�}���h�A���P�[�^������
		Hr = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(CommandAllocator.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//�R�}���h���X�g������
		Hr = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
			CommandAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//�R�}���h�L���[������
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;		//GPU�^�C���A�E�g���L��
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;		//���ڃR�}���h�L���[
		Hr = Device->CreateCommandQueue(&desc, IID_PPV_ARGS(CommandQueue.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�t�F���X
	{
		//GPU�̏����������`�F�b�N����t�F���X������
		Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.GetAddressOf()));
		assert(SUCCEEDED(Hr));
		FenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		assert(FenceEvent != nullptr);
		FenceValue = 1;
	}
}
void CreateRenderTarget()
{
	//�X���b�v�`�F�C��������(�����Ƀo�b�N�o�b�t�@���܂܂�Ă���)
	{
		//DXGI�t�@�N�g��������
		ComPtr<IDXGIFactory4> dxgiFactory;
		Hr = CreateDXGIFactory2(0, IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
		assert(SUCCEEDED(Hr));

		//�X���b�v�`�F�C��������
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		desc.BufferCount = 2; //�o�b�N�o�b�t�@2��
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

		//IDXGISwapChain4�C���^�[�t�F�C�X���T�|�[�g���Ă��邩�q�˂�
		Hr = swapChain1->QueryInterface(IID_PPV_ARGS(SwapChain.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�o�b�N�o�b�t�@�u�r���[�v�̓��ꕨ�ł���u�f�B�X�N���v�^�q�[�v�v������
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = 2;//�o�b�N�o�b�t�@�r���[�Q��
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//RenderTargetView
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//�V�F�[�_����A�N�Z�X���Ȃ��̂�NONE��OK
		Hr = Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(BbvHeap.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�o�b�N�o�b�t�@�u�r���[�v���u�f�B�X�N���v�^�q�[�v�v�ɂ���
	{
		D3D12_CPU_DESCRIPTOR_HANDLE hBbvHeap
			= BbvHeap->GetCPUDescriptorHandleForHeapStart();

		BbvIncSize
			= Device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (UINT idx = 0; idx < 2; idx++) {
			//�o�b�N�o�b�t�@�����o��
			Hr = SwapChain->GetBuffer(idx, IID_PPV_ARGS(BackBuffers[idx].GetAddressOf()));
			assert(SUCCEEDED(Hr));
			//�o�b�N�o�b�t�@�̃r���[���q�[�v�ɂ���
			hBbvHeap.ptr += BbvIncSize * idx;
			Device->CreateRenderTargetView(BackBuffers[idx].Get(), nullptr, hBbvHeap);
		}
	}
	//�f�v�X�X�e���V���o�b�t�@������
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;//DEFAULT��������UNKNOWN�ł悵
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2�����̃e�N�X�`���f�[�^�Ƃ���
		desc.Width = ClientWidth;//���ƍ����̓����_�[�^�[�Q�b�g�Ɠ���
		desc.Height = ClientHeight;//��ɓ���
		desc.DepthOrArraySize = 1;//�e�N�X�`���z��ł��Ȃ���3D�e�N�X�`���ł��Ȃ�
		desc.Format = DXGI_FORMAT_D32_FLOAT;//�[�x�l�������ݗp�t�H�[�}�b�g
		desc.SampleDesc.Count = 1;//�T���v����1�s�N�Z��������1��
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//���̃o�b�t�@�͐[�x�X�e���V���Ƃ��Ďg�p���܂�
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.MipLevels = 1;
		//�f�v�X�X�e���V���o�b�t�@���N���A����l
		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.DepthStencil.Depth = 1.0f;//�[���P(�ő�l)�ŃN���A
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;//32bit�[�x�l�Ƃ��ăN���A
		//�f�v�X�X�e���V���o�b�t�@�����
		Hr = Device->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, //�f�v�X�������݂Ɏg�p
			&depthClearValue,
			IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�f�v�X�X�e���V���o�b�t�@�u�r���[�v�̓��ꕨ�ł���u�f�X�N���v�^�q�[�v�v������
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};//�[�x�Ɏg����Ƃ��������킩��΂���
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;//�f�v�X�X�e���V���r���[�Ƃ��Ďg��
		desc.NumDescriptors = 1;//�[�x�r���[1�̂�
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		Hr = Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(DsvHeap.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}
	//�f�v�X�X�e���V���o�b�t�@�u�r���[�v���u�f�B�X�N���v�^�q�[�v�v�ɂ���
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_D32_FLOAT;//�f�v�X�l��32bit�g�p
		desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2D�e�N�X�`��
		desc.Flags = D3D12_DSV_FLAG_NONE;//�t���O�͓��ɂȂ�
		D3D12_CPU_DESCRIPTOR_HANDLE hDsvHeap
			= DsvHeap->GetCPUDescriptorHandleForHeapStart();
		Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &desc, hDsvHeap);
	}
}
void CreatePipeline()
{
	//���[�g�V�O�l�`��
	{
		//�f�B�X�N���v�^�����W�B�f�B�X�N���v�^�q�[�v�ƃV�F�[�_��R�Â�����������B
		D3D12_DESCRIPTOR_RANGE  range[1] = {};
		UINT b0 = 0;
		range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range[0].BaseShaderRegister = b0;
		range[0].NumDescriptors = 1;
		range[0].RegisterSpace = 0;
		range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//���[�g�p�����^���f�B�X�N���v�^�e�[�u���Ƃ��Ďg�p
		D3D12_ROOT_PARAMETER rootParam[1] = {};
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.pDescriptorRanges = range;
		rootParam[0].DescriptorTable.NumDescriptorRanges = _countof(range);
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//�T���v���̋L�q�B���̃T���v�����V�F�[�_�[�� s0 �ɃZ�b�g�����
		D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���J��Ԃ�
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�c�J��Ԃ�
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
		samplerDesc[0].MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�

		//���[�g�V�O�j�`���̋L�q
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.pParameters = rootParam;
		desc.NumParameters = _countof(rootParam);
		//desc.pStaticSamplers = samplerDesc;//�T���v���[�̐擪�A�h���X
		//desc.NumStaticSamplers = _countof(samplerDesc);//�T���v���[��

		//���[�g�V�O�l�`�����V���A���C�Y��blob(��)������B
		ComPtr<ID3DBlob> blob;
		Hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
		assert(SUCCEEDED(Hr));

		//���[�g�V�O�l�`��������
		Hr = Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(RootSignature.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}

	//�R���p�C���ς݃V�F�[�_��ǂݍ��ރt�@�C���o�b�t�@
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
	//�V�F�[�_�ǂݍ���
	BIN_FILE12 vs("assets\\VertexShader.cso");
	assert(vs.succeeded());
	BIN_FILE12 ps("assets\\PixelShader.cso");
	assert(ps.succeeded());

	//�ȉ��A�e��L�q

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
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//�S�ď�������
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;//�����������̗p
	depthStencilDesc.StencilEnable = false;//�X�e���V���o�b�t�@�͎g��Ȃ�


	//�����܂ł̋L�q���܂Ƃ߂ăp�C�v���C���X�e�[�g�I�u�W�F�N�g������
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

	//�o�͗̈��ݒ�
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = (float)ClientWidth;
	Viewport.Height = (float)ClientHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	//�؂����`��ݒ�
	ScissorRect.left = 0;
	ScissorRect.top = 0;
	ScissorRect.right = ClientWidth;
	ScissorRect.bottom = ClientHeight;
}
void createPipeline(const char* pixelShader)
{
	//���[�g�V�O�l�`��
	{
		//�f�B�X�N���v�^�����W�B�f�B�X�N���v�^�q�[�v�ƃV�F�[�_��R�Â�����������B
		D3D12_DESCRIPTOR_RANGE  range[1] = {};
		UINT b0 = 0;
		range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range[0].BaseShaderRegister = b0;
		range[0].NumDescriptors = 1;
		range[0].RegisterSpace = 0;
		range[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		//���[�g�p�����^���f�B�X�N���v�^�e�[�u���Ƃ��Ďg�p
		D3D12_ROOT_PARAMETER rootParam[1] = {};
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.pDescriptorRanges = range;
		rootParam[0].DescriptorTable.NumDescriptorRanges = _countof(range);
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		//�T���v���̋L�q�B���̃T���v�����V�F�[�_�[�� s0 �ɃZ�b�g�����
		D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;//��Ԃ��Ȃ�(�j�A���X�g�l�C�o�[)
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���J��Ԃ�
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//�c�J��Ԃ�
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//���s�J��Ԃ�
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;//�{�[�_�[�̎��͍�
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;//�~�b�v�}�b�v�ő�l
		samplerDesc[0].MinLOD = 0.0f;//�~�b�v�}�b�v�ŏ��l
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//�I�[�o�[�T���v�����O�̍ۃ��T���v�����O���Ȃ��H
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//�s�N�Z���V�F�[�_����̂݉�

		//���[�g�V�O�j�`���̋L�q
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		desc.pParameters = rootParam;
		desc.NumParameters = _countof(rootParam);
		//desc.pStaticSamplers = samplerDesc;//�T���v���[�̐擪�A�h���X
		//desc.NumStaticSamplers = _countof(samplerDesc);//�T���v���[��

		//���[�g�V�O�l�`�����V���A���C�Y��blob(��)������B
		ComPtr<ID3DBlob> blob;
		Hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
		assert(SUCCEEDED(Hr));

		//���[�g�V�O�l�`��������
		Hr = Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
			IID_PPV_ARGS(RootSignature.GetAddressOf()));
		assert(SUCCEEDED(Hr));
	}

	//�R���p�C���ς݃V�F�[�_��ǂݍ��ރt�@�C���o�b�t�@
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
	//�V�F�[�_�ǂݍ���
	BIN_FILE12 vs("assets\\VertexShader.cso");
	assert(vs.succeeded());
    std::stringstream ss;
    ss << "assets\\" << pixelShader << ".cso";
	BIN_FILE12 ps(ss.str().c_str());
	assert(ps.succeeded());

	//�ȉ��A�e��L�q

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
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//�S�ď�������
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;//�����������̗p
	depthStencilDesc.StencilEnable = false;//�X�e���V���o�b�t�@�͎g��Ȃ�


	//�����܂ł̋L�q���܂Ƃ߂ăp�C�v���C���X�e�[�g�I�u�W�F�N�g������
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

	//�o�͗̈��ݒ�
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = (float)ClientWidth;
	Viewport.Height = (float)ClientHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	//�؂����`��ݒ�
	ScissorRect.left = 0;
	ScissorRect.top = 0;
	ScissorRect.right = ClientWidth;
	ScissorRect.bottom = ClientHeight;
}

//�p�u���b�N�Ȋ֐�---------------------------------------------------------------
//�V�X�e���n
void window(LPCWSTR windowTitle, int clientWidth, int clientHeight, bool windowed, int clientPosX, int clientPosY)
{
	WindowTitle = windowTitle;
	ClientWidth = clientWidth;
	ClientHeight = clientHeight;
	ClientPosX = (GetSystemMetrics(SM_CXSCREEN) - ClientWidth) / 2;//�����\��
	if (clientPosX >= 0)ClientPosX = clientPosX;
	ClientPosY = (GetSystemMetrics(SM_CYSCREEN) - ClientHeight) / 2;//�����\��
	if (clientPosY >= 0)ClientPosY = clientPosY;
	Aspect = static_cast<float>(ClientWidth) / ClientHeight;
	WindowStyle = WS_POPUP;//Alt + F4�ŕ���
	if (windowed) WindowStyle = WS_OVERLAPPEDWINDOW;

	CreateWindows();
	CreateDevice();
	CreateRenderTarget();
	//CreatePipeline();

	//�E�B���h�E�\��
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
	//���݂�Fence�l���R�}���h�I�����Fence�ɏ������܂��悤�ɂ���
	UINT64 fvalue = FenceValue;
	CommandQueue->Signal(Fence.Get(), fvalue);
	FenceValue++;

	//�܂��R�}���h�L���[���I�����Ă��Ȃ����Ƃ��m�F����
	if (Fence->GetCompletedValue() < fvalue)
	{
		//����Fence�ɂ����āAfvalue �̒l�ɂȂ�����C�x���g�𔭐�������
		Fence->SetEventOnCompletion(fvalue, FenceEvent);
		//�C�x���g����������܂ő҂�
		WaitForSingleObject(FenceEvent, INFINITE);
	}
}
void closeEventHandle()
{
	CloseHandle(FenceEvent);
}
//�R���X�^���g�o�b�t�@�A�e�N�X�`���o�b�t�@�̃f�B�X�N���v�^�q�[�v
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
//�o�b�t�@�n
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
	//�t�@�C����ǂݍ��݁A���f�[�^�����o��
	unsigned char* pixels = nullptr;
	int width = 0, height = 0, bytePerPixel = 4;
	//pixels = stbi_load(filename, &width, &height, nullptr, bytePerPixel);
	if (pixels == nullptr)
	{
		MessageBoxA(0, filename, "�t�@�C�����Ȃ�����", 0);
		exit(0);
	}

	//�P�s�̃s�b�`��256�̔{���ɂ��Ă���(�o�b�t�@�T�C�Y��256�̔{���łȂ���΂����Ȃ�)
	const UINT64 alignedRowPitch = (width * bytePerPixel + 0xff) & ~0xff;

	//�A�b�v���[�h�p���ԃo�b�t�@������A���f�[�^���R�s�[���Ă���
	ComPtr<ID3D12Resource> uploadBuf;
	{
		//�e�N�X�`���ł͂Ȃ��t�c�[�̃o�b�t�@�Ƃ��Ă���
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

		//���f�[�^��uploadbuff�Ɉ�U�R�s�[���܂�
		uint8_t* mapBuf = nullptr;
		Hr = uploadBuf->Map(0, nullptr, (void**)&mapBuf);//�}�b�v
		auto srcAddress = pixels;
		auto originalRowPitch = width * bytePerPixel;
		for (int y = 0; y < height; ++y) {
			memcpy(mapBuf, srcAddress, originalRowPitch);
			//1�s���Ƃ̒�������킹�Ă��
			mapBuf += alignedRowPitch;
			srcAddress += originalRowPitch;
		}
		uploadBuf->Unmap(0, nullptr);//�A���}�b�v
	}

	//�����āA�ŏI�R�s�[��ł���e�N�X�`���o�b�t�@�����
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;//CPU����A�N�Z�X���Ȃ��B�����������B
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//���̃o�b�t�@�ƈႤ
		desc.Alignment = 0;
		desc.Width = width;
		desc.Height = height;//���̃o�b�t�@�ƈႤ
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//���̃o�b�t�@�ƈႤ
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//���̃o�b�t�@�ƈႤ
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

	//GPU��uploadBuf����textureBuf�փR�s�[���钷�����̂肪�n�܂�܂�

	//�܂��R�s�[�����P�[�V�����̏����E�t�b�g�v�����g�w��
	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = uploadBuf.Get();
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Footprint.Width = static_cast<UINT>(width);
	src.PlacedFootprint.Footprint.Height = static_cast<UINT>(height);
	src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(1);
	src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(alignedRowPitch);
	src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//�R�s�[�惍�P�[�V�����̏����E�T�u���\�[�X�C���f�b�N�X�w��
	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = textureBuf.Get();
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	//�R�}���h���X�g�ŃR�s�[��\�񂵂܂���I�I�I
	CommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	//���Ă��Ƃ̓o���A������̂ł�
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureBuf.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);
	//uploadBuf�A�����[�h
	CommandList->DiscardResource(uploadBuf.Get(), nullptr);
	//�R�}���h���X�g�����
	CommandList->Close();
	//���s
	ID3D12CommandList* commandLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	//���\�[�X��GPU�ɓ]�������܂őҋ@����
	waitGPU();

	//�R�}���h�A���P�[�^�����Z�b�g
	HRESULT Hr = CommandAllocator->Reset();
	assert(SUCCEEDED(Hr));
	//�R�}���h���X�g�����Z�b�g
	Hr = CommandList->Reset(CommandAllocator.Get(), nullptr);
	assert(SUCCEEDED(Hr));

	//�J��
	//stbi_image_free(pixels);

	return S_OK;
}
//�r���[�i�f�B�X�N���v�^�j�n
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
	desc.Texture2D.MipLevels = 1;//�~�b�v�}�b�v�͎g�p���Ȃ��̂�1
	auto hCbvTbvHeap = CbvTbvHeap->GetCPUDescriptorHandleForHeapStart();
	hCbvTbvHeap.ptr += CbvTbvIncSize * CbvTbvCurrentIdx;
	Device->CreateShaderResourceView(textureBuffer.Get(), &desc, hCbvTbvHeap);
	return CbvTbvCurrentIdx++;
}
//�`��n
void setClearColor(float r, float g, float b)
{
	ClearColor[0] = r;	ClearColor[1] = g;	ClearColor[2] = b;
}
void beginRender()
{
	//���݂̃o�b�N�o�b�t�@�̃C���f�b�N�X���擾�B���̃v���O�����̏ꍇ0 or 1�ɂȂ�B
	BackBufIdx = SwapChain->GetCurrentBackBufferIndex();

	//�o���A�Ńo�b�N�o�b�t�@��`��^�[�Q�b�g�ɐ؂�ւ���
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//���̃o���A�͏�ԑJ�ڃ^�C�v
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = BackBuffers[BackBufIdx].Get();//���\�[�X�̓o�b�N�o�b�t�@
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;//�J�ڑO��Present
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;//�J�ڌ�͕`��^�[�Q�b�g
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);

	//�o�b�N�o�b�t�@�̏ꏊ���w���f�B�X�N���v�^�q�[�v�n���h����p�ӂ���
	auto hBbvHeap = BbvHeap->GetCPUDescriptorHandleForHeapStart();
	hBbvHeap.ptr += BackBufIdx * BbvIncSize;
	//�f�v�X�X�e���V���o�b�t�@�̃f�B�X�N���v�^�n���h����p�ӂ���
	auto hDsvHeap = DsvHeap->GetCPUDescriptorHandleForHeapStart();
	//�o�b�N�o�b�t�@�ƃf�v�X�X�e���V���o�b�t�@��`��^�[�Q�b�g�Ƃ��Đݒ肷��
	CommandList->OMSetRenderTargets(1, &hBbvHeap, false, &hDsvHeap);

	//�`��^�[�Q�b�g���N���A����
	CommandList->ClearRenderTargetView(hBbvHeap, ClearColor, 0, nullptr);

	//�f�v�X�X�e���V���o�b�t�@���N���A����
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
	//�o���A�Ńo�b�N�o�b�t�@��\���p�ɐ؂�ւ���
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;//���̃o���A�͏�ԑJ�ڃ^�C�v
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = BackBuffers[BackBufIdx].Get();//���\�[�X�̓o�b�N�o�b�t�@
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;//�J�ڑO�͕`��^�[�Q�b�g
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;//�J�ڌ��Present
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	CommandList->ResourceBarrier(1, &barrier);

	//�R�}���h���X�g���N���[�Y����
	CommandList->Close();
	//�R�}���h���X�g�����s����
	ID3D12CommandList* commandLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	//�`�抮����҂�
	waitGPU();

	//�o�b�N�o�b�t�@��\��
	SwapChain->Present(0, 0);

	//�R�}���h�A���P�[�^�����Z�b�g
	Hr = CommandAllocator->Reset();
	assert(SUCCEEDED(Hr));
	//�R�}���h���X�g�����Z�b�g
	Hr = CommandList->Reset(CommandAllocator.Get(), nullptr);
	assert(SUCCEEDED(Hr));
}
//Get�n
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
