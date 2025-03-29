#include"graphic.h"

//���_�o�b�t�@
ComPtr<ID3D12Resource>   VertexBuffer;
D3D12_VERTEX_BUFFER_VIEW Vbv;
//�R���X�^���g�o�b�t�@�O
ComPtr<ID3D12Resource> ConstBuffer0;
UINT CbvIdx;
//�R���X�^���g�o�b�t�@�O�\���́BHeader.hlsli�Ɠ������тɂ��Ă���
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

	//���\�[�X������
	{
		//���_�o�b�t�@
		{
			//texcoord�́Ax��-aspect~aspect,�@y��1~-1�Ƃ���
			float vertices[] = {
				//position            texcoord
				-1.0f,  1.0f,  0.0f,  0,		height(), //����
				-1.0f, -1.0f,  0.0f,  0,		0, //����
				 1.0f,  1.0f,  0.0f,  width(),  height(), //�E��
				 1.0f, -1.0f,  0.0f,  width(),  0, //�E��
			};
			unsigned numVertexElements = 5;
			//�f�[�^�T�C�Y�����߂Ă���
			UINT sizeInBytes = sizeof(vertices);
			UINT strideInBytes = sizeof(float) * numVertexElements;
			//�o�b�t�@������
			createBuffer(sizeInBytes, VertexBuffer);
			//�o�b�t�@�Ƀf�[�^������
			updateBuffer(vertices, sizeInBytes, VertexBuffer);
			//�o�b�t�@�r���[������
			createVertexBufferView(VertexBuffer, sizeInBytes, strideInBytes, Vbv);
		}
		//�R���X�^���g�o�b�t�@�O
		{
			//�o�b�t�@������
			createBuffer(alignedSize(sizeof(CONST_BUF0)), ConstBuffer0);
			//�}�b�v���Ă���
			mapBuffer(ConstBuffer0, (void**)&CB0);
			//�P�̃f�B�X�N���v�^�̃q�[�v������
			createDescriptorHeap(1);
			//�r���[�������ăC���f�b�N�X��������Ă���
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
