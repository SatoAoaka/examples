#include "stdafx.h"
#include "WindowsInAltTab.h"
#include "CaptureView.h"

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

namespace {

// Windows::Graphics::Capture を使うだけなら別に要らん模様
//auto CreateDispatcherQueueController() 
//{
//	namespace abi = ABI::Windows::System;
//
//	DispatcherQueueOptions options {
//		sizeof(DispatcherQueueOptions),
//		DQTYPE_THREAD_CURRENT,
//		DQTAT_COM_STA
//	};
//
//	DispatcherQueueController controller{ nullptr };
//	check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
//	return controller;
//}

}

class MainWindow : public CWindowImpl<MainWindow> {
public:
	DECLARE_WND_CLASS(nullptr)

	void Launch() {
		Create(nullptr, CWindow::rcDefault, L"CapturePreview", WS_OVERLAPPEDWINDOW);

		CenterWindow();
		ShowWindow(SW_SHOWNORMAL);
		Invalidate();

		_windowsInAltTab = GetWindowsInAltTab();
		for (auto& window : _windowsInAltTab) {
			_comboBox.AddString(window.windowText.c_str());
		}

		_captureView.CreateDevice();
	}

private:
	BEGIN_MSG_MAP(MainWindow)
		COMMAND_CODE_HANDLER_EX(BN_CLICKED, OnButtonClicked)
		COMMAND_CODE_HANDLER_EX(CBN_SELCHANGE, OnComboSelectChange)
		MSG_WM_SIZE(OnSize)
		MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestory)
	END_MSG_MAP()

	fire_and_forget OnButtonClicked(UINT uNotifyCode, int, CWindow) {
		GraphicsCapturePicker picker;
		auto interop = picker.as<::IInitializeWithWindow>();
		interop->Initialize(*this);
		apartment_context context;
		auto item = co_await picker.PickSingleItemAsync();
		co_await context;
		if (!!item)
			_captureView.StartCaptureForItem(item);
	}

	void OnComboSelectChange(UINT uNotifyCode, int, CWindow) {
		auto index = _comboBox.GetCurSel();
		auto window = _windowsInAltTab[index];

		_captureView.StartCaptureForHwnd(window.hwnd);
	}

	void OnSize(UINT nType, CSize const& size) {
		CRect clientRect;
		GetClientRect(&clientRect);
		auto captionHeight = GetSystemMetrics(SM_CYCAPTION);

		CRect buttonRect;
		buttonRect.left = clientRect.left + 10;
		buttonRect.right = buttonRect.left + 100;
		buttonRect.top = clientRect.top + 10;
		buttonRect.bottom = buttonRect.top + captionHeight;
		_pickerButton.MoveWindow(&buttonRect);

		CRect comboRect;
		comboRect.left = buttonRect.right + 10;
		comboRect.right = clientRect.right - 10;
		comboRect.top = clientRect.top + 10;
		comboRect.bottom = comboRect.top + captionHeight;
		_comboBox.MoveWindow(&comboRect);

		CRect contentRect;
		contentRect.left = clientRect.left + 10;
		contentRect.right = clientRect.right - 10;
		contentRect.top = buttonRect.bottom + 10;
		contentRect.bottom = clientRect.bottom - 10;
		_captureView.MoveWindow(&contentRect);
	}

	void OnGetMinMaxInfo(MINMAXINFO* mmi) {
		mmi->ptMinTrackSize.x = 300;
		mmi->ptMinTrackSize.y = 300;
	}

	LRESULT OnCreate(LPCREATESTRUCT) {
		LOGFONT lf;
		SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
		_iconTitleFont.Attach(CreateFontIndirect(&lf));

		auto captionHeight = GetSystemMetrics(SM_CYCAPTION);

		CRect clientRect;
		GetClientRect(&clientRect);

		CRect buttonRect;
		buttonRect.left = clientRect.left + 10;
		buttonRect.right = buttonRect.left + 100;
		buttonRect.top = clientRect.top + 10;
		buttonRect.bottom = buttonRect.top + captionHeight;
		_pickerButton.Create(
			*this, 
			buttonRect, 
			L"Use Picker",
			BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE);
		_pickerButton.SetFont(_iconTitleFont);

		CRect comboRect;
		comboRect.left = buttonRect.right + 10;
		comboRect.right = clientRect.right - 10;
		comboRect.top = clientRect.top + 10;
		comboRect.bottom = comboRect.top + captionHeight;
		_comboBox.Create(
			*this,
			comboRect,
			nullptr,
			CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_CHILD | WS_VISIBLE);
		_comboBox.SetFont(_iconTitleFont);

		CRect contentRect;
		contentRect.left = clientRect.left + 10;
		contentRect.right = clientRect.right - 10;
		contentRect.top = buttonRect.bottom + 10;
		contentRect.bottom = clientRect.bottom - 10;
		_captureView.Create(*this, contentRect);

		return 0;
	}

	void OnDestory() {
		PostQuitMessage(0);
	}

private:
	CFont _iconTitleFont;
	CButton _pickerButton;
	CComboBox _comboBox;
	CaptureView _captureView;
	std::vector<WindowInfo> _windowsInAltTab;
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, WCHAR*, int) 
{
	init_apartment(apartment_type::single_threaded);
	//init_apartment();

	//auto controller = CreateDispatcherQueueController();
	//auto queue = controller.DispatcherQueue();

	_Module.Init(nullptr, hInstance);

	CMessageLoop kicker;
	_Module.AddMessageLoop(&kicker);

	MainWindow app;
	app.Launch();

	auto wParam = kicker.Run();

	_Module.RemoveMessageLoop();
	_Module.Term();

	return wParam;
}