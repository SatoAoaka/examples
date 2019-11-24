#include "stdafx.h"

namespace {

    const UINT FrameCount = 2;

}

class MainWindow : public CWindowImpl<MainWindow>, public CIdleHandler {
public:
    DECLARE_WND_CLASS_EX(nullptr, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, -1)

    void Launch() {
        Create(nullptr, CWindow::rcDefault, L"00_Hello", WS_OVERLAPPEDWINDOW);

        CenterWindow();
        ShowWindow(SW_SHOWNORMAL);
        Invalidate();

        UINT factoryFlags = 0;
#if defined(_DEBUG)
        {
            // Direct3D 12 Debug layer �L�����̎葱��
            wil::com_ptr<ID3D12Debug> debugController;
            THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));
            debugController->EnableDebugLayer();
            // ���̃t���O�� Direct3D 11 �܂ł̓f�o�C�X�쐬���̎w�蓙�ɂ���Ă̓����^�C�����ݒ肷�邱�Ƃ������������ȁB
            // Direct3D 12 �� D3D12CreateDevice ���V���v���ɂȂ��Ă�Ԃ񂱂��Ŗ����I�Ɏw�肵�悤�Ƃ������Ƃ�����H
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        // Direct3D 12 �� WARP �h���C�o���g�p����ꍇ�� IDXGIFactory4::EnumWarpAdapter ���K�v�ɂȂ�
        wil::com_ptr<IDXGIFactory4> factory;
        THROW_IF_FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.put())));
        
        // �n�[�h�E�F�A�A�_�v�^�̎擾
        wil::com_ptr<IDXGIAdapter1> adapter;
        for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, adapter.put()); ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }
            // MinimumFeatureLevel �ō쐬�ł��邩����
            if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }

        THROW_IF_FAILED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(_device.put())));

        // ��ނƃI�v�V�������w�肵�ăR�}���h�L���[���쐬
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.put())));

        CRect clientRect;
        GetClientRect(&clientRect);

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        scd.Width = static_cast<UINT>(clientRect.Width());
        scd.Height = static_cast<UINT>(clientRect.Height());
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferCount = FrameCount;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.SampleDesc.Count = 1;

        // Direct3D 12 �̃X���b�v�`�F�C���̍쐬�ɂ̓f�o�C�X�ł͂Ȃ��R�}���h�L���[��n��
        wil::com_ptr<IDXGISwapChain1> swapChain;
        THROW_IF_FAILED(factory->CreateSwapChainForHwnd(_commandQueue.get(), *this, &scd, nullptr, nullptr, swapChain.put()));
        
        // Direct3D 12 �ł� IDXGISwapChain3::GetCurrentBackBufferIndex ���K�v
        _swapChain = swapChain.query<IDXGISwapChain3>();
        
        // Direct3D 12 �ł� GetBuffer ����Ԃ���郊�\�[�X�͉��z������Ă��Ȃ��̂�
        // �����_�����O���ׂ����݂̃o�b�t�@�ւ̏������A�v�����ŋL�^���Ă����K�v������
        _frameIndex = _swapChain->GetCurrentBackBufferIndex();

        // RTV �p�̃f�X�N���v�^�q�[�v���쐬
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = FrameCount;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        THROW_IF_FAILED(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeap.put())));
        {
            // �o�b�N�o�b�t�@���Ƀf�X�N���v�^�q�[�v�� RTV ���쐬
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
            auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            for (UINT i = 0; i < FrameCount; ++i) {
                THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                rtvHandle.ptr += rtvDescriptorSize;
            }
        }

        // ��ނ��w�肵�ăR�}���h�A���P�[�^���쐬
        THROW_IF_FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_commandAllocator.put())));

        // ��ނƃA���P�[�^�� PSO ���w�肵�ăR�}���h���X�g���쐬�B���̃T���v���ł� PSO �͖��g�p
        THROW_IF_FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.get(), nullptr, IID_PPV_ARGS(_commandList.put())));
        
        // �����_�����O���[�v�̐擪�ł̓R�}���h���X�g�����Ă��邱�Ƃ�O��Ƃ��Ă���
        THROW_IF_FAILED(_commandList->Close());

        // GPU �̊�����ҋ@���邽�߂̃t�F���X�Ƒҋ@�C�x���g���쐬�B
        // �t�F���X�� GPU �̐i�s����J�E���^�[�ŊǗ�����B
        // �����ł� 0 �������l�Ƃ��AfenceValue �͎��̃t���[���̊����������l�B
        THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_gpuFence.put())));
        _fenceEvent.create(wil::EventOptions::None);
        _fenceValue = 1;
    }

    virtual BOOL OnIdle() {
        OnRender();
        return TRUE;
    }

private:

    void OnRender() {
        // �R�}���h���X�g�ɂ���Ċm�ۂ��ꂽ�����������Z�b�g���Ďg�p�ł���悤�w���B
        // ���̎��R�}���h�̎��s���������Ă��Ȃ���΂Ȃ�Ȃ�
        THROW_IF_FAILED(_commandAllocator->Reset());

        // �R�}���h���X�g���ēx�R�}���h���L�^�ł���悤�Ɏw���B
        // ���̎��R�}���h���X�g�� closed ��ԂłȂ���΂Ȃ�Ȃ�
        THROW_IF_FAILED(_commandList->Reset(_commandAllocator.get(), nullptr));
        
        {
            // �o�b�N�o�b�t�@�̏�Ԃ� PRESENT (COMMON) ���� RENDER_TARGET �ɑJ�ڂ���܂ő҂��\�[�X�o���A��ݒu
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = _renderTargets[_frameIndex].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _commandList->ResourceBarrier(1, &barrier);
        }
        
        // RTV �p�f�X�N���v�^�q�[�v����e RTV �̃n���h���̈ʒu���Z�o
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        rtvHandle.ptr += _frameIndex * rtvDescriptorSize;

        auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
        _commandList->ClearRenderTargetView(rtvHandle, &clearColor.r, 0, nullptr);

        {
            // �o�b�N�o�b�t�@�̏�Ԃ� RENDER_TARGET ���� PRESENT (COMMON) �ɑJ�ڂ���܂ő҂��\�[�X�o���A��ݒu�B
            // Present �����s����ɂ̓o�b�N�o�b�t�@�� PRESENT (COMMON) ��ԂłȂ���΂Ȃ�Ȃ�
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = _renderTargets[_frameIndex].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _commandList->ResourceBarrier(1, &barrier);
        }

        // �R�}���h�̋L�^���I������B���̎��R�}���h���X�g�� closed ��Ԃ��ƃG���[�ɂȂ�
        THROW_IF_FAILED(_commandList->Close());

        std::array<ID3D12CommandList*, 1> commandLists = {
            _commandList.get(),
        };
        _commandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

        THROW_IF_FAILED(_swapChain->Present(1, 0));

        WaitForGpu();
    }

    void WaitForGpu() {
        // GPU �̊����������t�F���X�̒l�̍X�V�� GPU �Ɏw��
        UINT64 value = _fenceValue;
        ++_fenceValue;
        THROW_IF_FAILED(_commandQueue->Signal(_gpuFence.get(), value));

        // �t�F���X�̌��݂̒l���擾���AGPU ���������Ă��Ȃ���΃X���b�h��ҋ@������
        if (_gpuFence->GetCompletedValue() < value) {
            THROW_IF_FAILED(_gpuFence->SetEventOnCompletion(value, _fenceEvent.get()));
            _fenceEvent.wait();
        }

        // GPU ���������Ă���Ό��݂̃t���[�������𓯊�������
        _frameIndex = _swapChain->GetCurrentBackBufferIndex();
    }

private:
    BEGIN_MSG_MAP(MainWindow)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestory)
    END_MSG_MAP()

    void OnSize(UINT type, CSize const& size) {
        if (!_swapChain || type == SIZE_MINIMIZED) {
            return;
        }

        DXGI_SWAP_CHAIN_DESC1 scd;
        _swapChain->GetDesc1(&scd);

        if (size.cx != scd.Width || size.cy != scd.Height) {
            // Direct3D 11 �܂łƓ��l�X���b�v�`�F�C���̃��T�C�Y�O�Ƀo�b�N�o�b�t�@�ւ̎Q�Ƃ�S�ĉ�������B
            // RTV �� COM �I�u�W�F�N�g�Ŗ����Ȃ����̂Ŗ����I�ȉ�����s�v�ƂȂ�B
            // �t�Ƀ��\�[�X�� GPU �ɂ���Ďg�p����Ă��Ȃ����Ƃ�ۏ؂���̂̓A�v�����̐ӔC�ƂȂ�
            for (int i = 0; i < FrameCount; ++i) {
                _renderTargets[i] = nullptr;
            }

            auto width = std::max(static_cast<UINT>(size.cx), 1u);
            auto height = std::max(static_cast<UINT>(size.cy), 1u);
            THROW_IF_FAILED(_swapChain->ResizeBuffers(scd.BufferCount, width, height, scd.Format, scd.Flags));

            // ���݂̃t���[�������̓��Z�b�g
            _frameIndex = _swapChain->GetCurrentBackBufferIndex();

            {
                // RTV ���č쐬�B�P���Ɋ����̃f�X�N���v�^�q�[�v��ŏ㏑������
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
                auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                for (UINT i = 0; i < FrameCount; ++i) {
                    THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                    _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                    rtvHandle.ptr += rtvDescriptorSize;
                }
            }
        }
    }

    void OnDestory() {
        WaitForGpu();

        for (int i = 0; i < FrameCount; ++i) {
            _renderTargets[i] = nullptr;
        }
        
        _gpuFence = nullptr;
        _commandList = nullptr;
        _commandAllocator = nullptr;
        _rtvHeap = nullptr;
        _swapChain = nullptr;
        _commandQueue = nullptr;
        _device = nullptr;

        PostQuitMessage(0);
    }

private:
    wil::com_ptr<ID3D12Device> _device;
    wil::com_ptr<ID3D12CommandQueue> _commandQueue;
    wil::com_ptr<IDXGISwapChain3> _swapChain;
    wil::com_ptr<ID3D12Resource> _renderTargets[FrameCount];
    wil::com_ptr<ID3D12DescriptorHeap> _rtvHeap;
    UINT _frameIndex;
    wil::com_ptr<ID3D12CommandAllocator> _commandAllocator;
    wil::com_ptr<ID3D12GraphicsCommandList> _commandList;
    wil::com_ptr<ID3D12Fence> _gpuFence;
    wil::unique_event _fenceEvent;
    UINT64 _fenceValue;
};

class MessageSpin : public CMessageLoop {
public:
    virtual BOOL OnIdle(int nIdleCount) {
        CMessageLoop::OnIdle(nIdleCount);
        return TRUE;
    }
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, WCHAR*, int)
{
    //SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    _Module.Init(nullptr, hInstance);

    MessageSpin kicker;
    _Module.AddMessageLoop(&kicker);

    MainWindow app;
    app.Launch();

    kicker.AddIdleHandler(&app);
    auto wParam = kicker.Run();

    _Module.RemoveMessageLoop();
    _Module.Term();

    return wParam;

}