#include "stdafx.h"
#include "CaptureView.h"
#include "Direct3DHelper.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

using namespace ::DirectX;

namespace {


auto CreateCaptureItemForWindow(HWND hwnd)
{
	namespace abi = ABI::Windows::Graphics::Capture;

	auto factory = get_activation_factory<GraphicsCaptureItem>();
	auto interop = factory.as<IGraphicsCaptureItemInterop>();
	GraphicsCaptureItem item{ nullptr };
	check_hresult(interop->CreateForWindow(hwnd, guid_of<abi::IGraphicsCaptureItem>(), reinterpret_cast<void**>(put_abi(item))));
	return item;
}

auto FitInBox(Size const& source, Size const& destination)
{
	// �A�X�y�N�g���ێ������܂܃{�b�N�X�Ɏ��܂��`���v�Z
	Rect box;

	box.Width = destination.Width;
	box.Height = destination.Height;
	float aspect = source.Width / source.Height;
	if (box.Width >= box.Height * aspect) {
		box.Width = box.Height * aspect;
	}
	aspect = source.Height / source.Width;
	if (box.Height >= box.Width * aspect) {
		box.Height = box.Width * aspect;
	}
	box.X = (destination.Width - box.Width) * 0.5f;
	box.Y = (destination.Height - box.Height) * 0.5f;

	return CRect(
		static_cast<int>(box.X), 
		static_cast<int>(box.Y), 
		static_cast<int>(box.X + box.Width), 
		static_cast<int>(box.Y + box.Height));
}

} 

HRESULT CaptureView::CreateDevice()
{
	WINRT_VERIFY(IsWindow());
	
	_device = CreateDirect3DDevice();
	_d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(_device);

	auto dxgiDevice = _d3dDevice.as<IDXGIDevice2>();
	com_ptr<IDXGIAdapter> dxgiAdapter;
	check_hresult(dxgiDevice->GetParent(guid_of<IDXGIAdapter>(), dxgiAdapter.put_void()));
	com_ptr<IDXGIFactory2> dxgiFactory;
	check_hresult(dxgiAdapter->GetParent(guid_of<IDXGIFactory2>(), dxgiFactory.put_void()));

	CRect clientRect;
	GetClientRect(&clientRect);
	DXGI_SWAP_CHAIN_DESC1 scd = {};
	scd.Width = clientRect.Width();
	scd.Height = clientRect.Height();
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.SampleDesc.Count = 1;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	check_hresult(dxgiFactory->CreateSwapChainForHwnd(
		_d3dDevice.get(),
		*this,
		&scd,
		nullptr,
		nullptr,
		_dxgiSwapChain.put()));

	com_ptr<ID3D11Texture2D> chainedBuffer;
	check_hresult(_dxgiSwapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), chainedBuffer.put_void()));
	check_hresult(_d3dDevice->CreateRenderTargetView(chainedBuffer.get(), nullptr, _chainedBufferRTV.put()));

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());
	_spriteBatch = std::make_unique<SpriteBatch>(context.get());
	
	return S_OK;
}

bool CaptureView::StartCapture(winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item)
{
	// Direct3D11CaptureFramePool �͎g���܂킹�Ă��ǂ������ȋC�����邯��
	// GraphicsCaptureSession �ƈꏏ�ɔj�����Ȃ��ƃ_������
	StopCapture();

	auto framePool = Direct3D11CaptureFramePool::Create(
		_device,
		DirectXPixelFormat::B8G8R8A8UIntNormalized,
		2,
		item.Size());
	auto session = framePool.CreateCaptureSession(item);
	if (!session.IsSupported()) {
		return false;
	}

	_captureItem = item;
	_framePool = framePool;
	_frameArrived = _framePool.FrameArrived(auto_revoke, { this, &CaptureView::OnFrameArrived });
	_captureSession = session;

	_captureSession.StartCapture();

	return true;
}

bool CaptureView::StartCaptureForHwnd(HWND hwndItem)
{
	return StartCapture(CreateCaptureItemForWindow(hwndItem));
}

void CaptureView::StopCapture()
{
	if (IsCapturing()) {		
		_frameArrived.revoke();

		// �����͖����I�� Close ���Ăяo���Ȃ��ƎQ�ƃJ�E���^���c���Ă�Ƃ������|�[�g���f�����H
		_captureSession.Close();
		_framePool.Close();

		_captureSession = nullptr;
		_captureItem = nullptr;
		_framePool = nullptr;
	}
}

void CaptureView::OnPaint(CDCHandle)
{
	CPaintDC dc(*this);

	if (!_dxgiSwapChain || IsCapturing()) {
		return;
	}

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());

	ID3D11RenderTargetView* pRTVs[1] = { _chainedBufferRTV.get() };
	context->OMSetRenderTargets(1, pRTVs, nullptr);

	auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
	context->ClearRenderTargetView(_chainedBufferRTV.get(), &clearColor.r);

	DXGI_PRESENT_PARAMETERS pp = { 0 };
	_dxgiSwapChain->Present1(1, 0, &pp);
}

void CaptureView::OnSize(UINT nType, CSize const& size)
{
	if (_dxgiSwapChain == nullptr) {
		return;
	}

	if ((nType == SIZE_RESTORED || nType == SIZE_MAX) && size.cx > 0 && size.cy) {
		_chainedBufferRTV = nullptr;
		_dxgiSwapChain->ResizeBuffers(2, size.cx, size.cy, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		
		com_ptr<ID3D11Texture2D> chainedBuffer;
		check_hresult(_dxgiSwapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), chainedBuffer.put_void()));
		check_hresult(_d3dDevice->CreateRenderTargetView(chainedBuffer.get(), nullptr, _chainedBufferRTV.put()));

		Invalidate();
	}
}

void CaptureView::OnDestroy()
{
	StopCapture();

	_spriteBatch = nullptr;
	_chainedBufferRTV = nullptr;
	_dxgiSwapChain = nullptr;
	_d3dDevice = nullptr;
	_device = nullptr;
}

void CaptureView::OnFrameArrived(
	Direct3D11CaptureFramePool const& sender, 
	winrt::Windows::Foundation::IInspectable const& args)
{
	auto frame = sender.TryGetNextFrame();
	auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

	// �Ȃ�ł��m��� CaptureItem::Size �� CaptureFrame::ContentSize ���قȂ�ꍇ������
	// �����_�����O�� ContentSize ����ɂ���ׂ�
	auto contentSize = frame.ContentSize();

	com_ptr<ID3D11ShaderResourceView> frameSurfaceSRV;
	check_hresult(_d3dDevice->CreateShaderResourceView(frameSurface.get(), nullptr, frameSurfaceSRV.put()));

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());

	ID3D11RenderTargetView* pRTVs[1];
	pRTVs[0] = _chainedBufferRTV.get();
	context->OMSetRenderTargets(1, pRTVs, nullptr);

	D3D11_VIEWPORT vp = { 0 };
	DXGI_SWAP_CHAIN_DESC1 scd;
	_dxgiSwapChain->GetDesc1(&scd);
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(scd.Width);
	vp.Height = static_cast<float>(scd.Height);
	context->RSSetViewports(1, &vp);

	auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
	context->ClearRenderTargetView(_chainedBufferRTV.get(), &clearColor.r);

	_spriteBatch->Begin();

	CRect sourceRect, destinationRect;

	sourceRect.left = 0;
	sourceRect.top = 0;
	sourceRect.right = contentSize.Width;
	sourceRect.bottom = contentSize.Height;

	destinationRect = FitInBox(
		{ static_cast<float>(contentSize.Width), static_cast<float>(contentSize.Height) }, 
		{ static_cast<float>(scd.Width), static_cast<float>(scd.Height) });

	_spriteBatch->Draw(frameSurfaceSRV.get(), destinationRect, &sourceRect);

	_spriteBatch->End();

	DXGI_PRESENT_PARAMETERS pp = { 0 };
	_dxgiSwapChain->Present1(1, 0, &pp);

	// Surface �̃T�C�Y�͂����炭 FramePool �쐬���̃o�b�t�@�T�C�Y�B
	// CaptureItem �̃T�C�Y�̓E�B���h�E�̃T�C�Y�ɒǏ]����̂�
	// ���ق�����Ȃ� FramePool ���č쐬����
	D3D11_TEXTURE2D_DESC surfaceDesc;
	frameSurface->GetDesc(&surfaceDesc);
	auto itemSize = _captureItem.Size();
	if (itemSize.Width != surfaceDesc.Width || itemSize.Height != surfaceDesc.Height) {
		// GraphicsCaptureItem::Closed �͎Q�Ƃ��Ă���E�B���h�E���j�����ꂽ��
		// ���s�����C�x���g���Ǝv�������ǂ��������킯�ł����������c
		// Size �� 0 �Ȃ�j�����ꂽ�ƌ��􂷂����Ȃ����ȁH
		itemSize.Width = std::max(itemSize.Width, 1);
		itemSize.Height = std::max(itemSize.Height, 1);
		_framePool.Recreate(_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, itemSize);
	}
}
