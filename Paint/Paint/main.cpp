#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>
#include <list>
#include <map>
#include <memory>
#include <gdiplus.h>

#define IDM_CLEAR      10000
#define IDM_RECTPRESS  10001
#define IDM_ELLIPPRESS 10002
#define IDM_PEN        10003
#define IDM_COLOR      10010
#define IDM_WTHBUTTON  10011
#define IDM_CLEANER    10012
#define IDM_FILL       10013

#pragma comment(lib, "d2d1")


using namespace std;

enum Width
{
	WIDTH_1 = 1,
	WIDTH_2,
	WIDTH_3,
	WIDTH_4,
	WIDTH_5,
	WIDTH_6
};

template <class DERIVED_TYPE>
class BaseWindow
{
public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		DERIVED_TYPE* pThis = NULL;

		if (uMsg == WM_NCCREATE)
		{
			CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
			pThis = (DERIVED_TYPE*)pCreate->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);

			pThis->m_hwnd = hwnd;
		}
		else
		{
			pThis = (DERIVED_TYPE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		}
		if (pThis)
		{
			return pThis->HandleMessage(uMsg, wParam, lParam);
		}
		else
		{
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

	BaseWindow() : m_hwnd(NULL) { }

	BOOL Create(
		PCWSTR lpWindowName,
		DWORD dwStyle,
		HWND hWndParent = 0,
		DWORD dwExStyle = 0,
		int x = CW_USEDEFAULT,
		int y = 0,
		int nWidth = CW_USEDEFAULT,
		int nHeight = 0,
		HMENU hMenu = 0
	)
	{
		WNDCLASS wc = {};

		wc.lpfnWndProc = DERIVED_TYPE::WindowProc;
		wc.hInstance = GetModuleHandle(NULL);//hInstance;
		wc.lpszClassName = ClassName();
		wc.style = CS_HREDRAW | CS_VREDRAW;

		RegisterClass(&wc);

		m_hParent = hWndParent;

		m_hwnd = CreateWindowEx(
			dwExStyle, ClassName(), lpWindowName, dwStyle, x, y,
			nWidth, nHeight, m_hParent, hMenu, GetModuleHandle(NULL), this
		);

		return (m_hwnd ? TRUE : FALSE);
	}

	HWND Window() const { return m_hwnd; }

protected:

	virtual PCWSTR  ClassName() const = 0;
	virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

	HWND m_hwnd;
	HWND m_hParent;
};
template <class T> void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

class DPIScale
{
	static float scaleX;
	static float scaleY;

public:
	static void Initialize(ID2D1Factory* pFactory)
	{
		FLOAT dpiX, dpiY;
#pragma warning(suppress: 4996)
		pFactory->GetDesktopDpi(&dpiX, &dpiY);
		scaleX = dpiX / 96.0f;
		scaleY = dpiY / 96.0f;
	}

	template <typename T>
	static float PixelsToDipsX(T x)
	{
		return static_cast<float>(x) / scaleX;
	}

	template <typename T>
	static float PixelsToDipsY(T y)
	{
		return static_cast<float>(y) / scaleY;
	}
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct MyEllipse
{
public:
	D2D1_COLOR_F    color;
	D2D1_ELLIPSE    ellipse;
	FLOAT           strokeWidth;

	bool            grad;

	void SetColor(D2D1_COLOR_F& col)
	{
		this->color = col;
	}

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush)
	{
		D2D1_COLOR_F temp = pBrush->GetColor();
		pBrush->SetColor(color);
		pRT->FillEllipse(ellipse, pBrush);
		pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		pRT->DrawEllipse(ellipse, pBrush, strokeWidth);
		pBrush->SetColor(temp);
	}

	BOOL HitTest(float x, float y)
	{
		const float a = ellipse.radiusX;
		const float b = ellipse.radiusY;
		const float x1 = x - ellipse.point.x;
		const float y1 = y - ellipse.point.y;
		const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
		return d <= 1.0f;
	}
};
struct MyRectangle 
{
public:
	D2D1_COLOR_F    color;
	D2D1_RECT_F    rect;
	FLOAT strokeWidth;

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush)
	{
		D2D1_COLOR_F temp = pBrush->GetColor();
		pBrush->SetColor(color);
		pRT->FillRectangle(rect, pBrush);
		pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		pRT->DrawRectangle(rect, pBrush, strokeWidth);
		pBrush->SetColor(temp);
	}

	void SetColor(D2D1_COLOR_F& col)
	{
		this->color = col;
	}

	BOOL HitTest(float x, float y)
	{
		if (x <= rect.right && x >= rect.left)
			if (y <= rect.bottom && y >= rect.top)
				return TRUE;
		return FALSE;
	}
};
struct MyPen
{
public:
	D2D1_COLOR_F    color;
	FLOAT strokeWidth;
	list<D2D1_POINT_2F> points;

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush)
	{
		D2D1_COLOR_F temp = pBrush->GetColor();
		pBrush->SetColor(color);
		auto s = points.begin();
		auto next = s++;
		int i = 1;
		while (i!=points.size())
		{
			pRT->DrawLine((*s), (*next), pBrush, strokeWidth);
			s++;
			next++;
			i++;
		}
		pBrush->SetColor(temp);
	}
};
class Clearer
{
public:
	FLOAT strokeWidth;
	list<D2D1_POINT_2F> points;

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush)
	{
		D2D1_COLOR_F temp = pBrush->GetColor();
		pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
		auto s = points.begin();
		auto next = s++;
		int i = 1;
		while (i != points.size())
		{
			pRT->DrawLine((*s), (*next), pBrush, strokeWidth);
			s++;
			next++;
			i++;
		}
		pBrush->SetColor(temp);
	}
};
class MainWindow : public BaseWindow<MainWindow>
{
	enum Shape
	{
		Ellipse,
		Rectangle,
		Pen
	};
	
	enum Mode
	{
		Draw,
		Fill,
		Clear
	};

	map<int, MyEllipse>           elipses;
	map<int, MyRectangle>         rects;
	map<int, MyPen>               pens;
	map<int, Clearer>             clears;
	HCURSOR                   hCursor;

	int id;

	Mode                      mode;
	Shape                     shape;
	ID2D1Factory*             pFactory;
	ID2D1HwndRenderTarget*    pRenderTarget;
	ID2D1SolidColorBrush*     pBrush;
	HPEN                      hPen;
	D2D1_POINT_2F             ptMouse;
	D2D1_COLOR_F              screenColor;
	CHOOSECOLOR               cc;
	DWORD                     cColor[16];
	ID2D1LinearGradientBrush* gradBrush;
	FLOAT                     strokeWidth;

	HDC                       hdc;

	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gradPros;

	HWND                      rectButton;
	HWND                      ellipButton;
	HWND                      clearButton;
	HWND                      colorButton;
	HWND                      penButton;
	HWND                      wdtButton;
	HMENU                     penWidthMenu;
	HWND                      clrButton;
	HWND                      flButton;

public:
	MainWindow() : id(0), pFactory(NULL), pRenderTarget(NULL), pBrush(NULL),
		ptMouse(D2D1::Point2F()), strokeWidth(1.0f)
	{
	}

	PCWSTR ClassName() const { return L"Main Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	HRESULT CreateGraphicsResources();
	void DiscardGraphicsResources();
	void OnPaint();
	void Resize();
	void OnLButtonDown(int pixelX, int pixelY, DWORD flags);
	void OnMouseMove(int pixelX, int pixelY, DWORD flags);
	void OnLButtonUp();

	HRESULT InsertEllipse(float x, float y);
	HRESULT InsertRectangle(float x, float y);
	HRESULT InsertPen(float x, float y);
	HRESULT InsertClear(float x, float y);

	void CleanUp();
	void ChangeColor();
	void ChangeWidth(int width);
	void FillArea(float x, float y);

	void SetMode(Mode mode) { this->mode = mode; }
	void SetShape(Shape shape) { this->shape = shape; }
	void IntitializeComponents();
};

HRESULT MainWindow::CreateGraphicsResources()
{
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			&pRenderTarget);

		//Rectangle(hdc, rc.left + 10, rc.top + 10, rc.right - 400, rc.bottom - 10);

		screenColor = D2D1::ColorF(D2D1::ColorF::White);

		if (SUCCEEDED(hr))
		{
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
		}
	}
	return hr;
}

void MainWindow::IntitializeComponents()
{
	rectButton = CreateWindow(L"BUTTON", L"Rectangle", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 10, 10, 70, 35, m_hwnd, (HMENU)IDM_RECTPRESS, NULL, NULL);
	ellipButton = CreateWindow(L"BUTTON", L"Ellipse", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 100, 10, 75, 35, m_hwnd, (HMENU)IDM_ELLIPPRESS, NULL, NULL);
	clearButton = CreateWindow(L"BUTTON", L"Clean", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 190, 10, 75, 35, m_hwnd, (HMENU)IDM_CLEAR, NULL, NULL);
	colorButton = CreateWindow(L"BUTTON", L"Color", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 370, 10, 75, 35, m_hwnd, (HMENU)IDM_COLOR, NULL, NULL);
	penButton = CreateWindow(L"BUTTON", L"Pen", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 280, 10, 75, 35, m_hwnd, (HMENU)IDM_PEN, NULL, NULL);
	clrButton = CreateWindow(L"BUTTON", L"Eraser", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 460, 10, 75, 35, m_hwnd, (HMENU)IDM_CLEANER, NULL, NULL);
	flButton = CreateWindow(L"BUTTON", L"Fill", WS_CHILD | WS_BORDER | BS_PUSHBUTTON | WS_VISIBLE, 550, 10, 75, 35, m_hwnd, (HMENU)IDM_FILL, NULL, NULL);
	{
		cc.Flags = CC_RGBINIT | CC_FULLOPEN;
		cc.hInstance = NULL;
		cc.hwndOwner = m_hwnd;
		cc.lCustData = 0L;
		cc.lpCustColors = cColor;
		cc.lpfnHook = NULL;
		cc.lpTemplateName = (LPCWSTR)NULL;
		cc.lStructSize = sizeof(cc);
		cc.rgbResult = RGB(255, 0, 0);
	}
	{
		HMENU hWidthMenu = CreateMenu();
		penWidthMenu = CreateMenu();
		AppendMenu(penWidthMenu, MF_CHECKED, WIDTH_1, L"1");
		AppendMenu(penWidthMenu, MF_UNCHECKED, WIDTH_2, L"2");
		AppendMenu(penWidthMenu, MF_UNCHECKED, WIDTH_3, L"3");
		AppendMenu(penWidthMenu, MF_UNCHECKED, WIDTH_4, L"4");
		AppendMenu(penWidthMenu, MF_UNCHECKED, WIDTH_5, L"5");
		AppendMenu(penWidthMenu, MF_UNCHECKED, WIDTH_6, L"6");
		AppendMenu(hWidthMenu, MF_POPUP, (UINT)penWidthMenu, L"Width");
		SetMenu(m_hwnd, hWidthMenu);
	}
}

void MainWindow::DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

void MainWindow::CleanUp()
{
	screenColor = D2D1::ColorF(D2D1::ColorF::White);
	pBrush->SetColor(screenColor);
	elipses.clear();
	rects.clear();
	pens.clear();
	InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::FillArea(float x, float y)
{
	bool stop = false;
		for (auto ellipse = elipses.begin(); ellipse != elipses.end(); ellipse++)
		{
			if ((*ellipse).second.HitTest(x, y))
			{
				stop = true;
				D2D1_COLOR_F color = pBrush->GetColor();
				(*ellipse).second.SetColor(color);
			}
		}
		for (auto rect = rects.begin(); rect != rects.end(); rect++)
		{
			if ((*rect).second.HitTest(x, y))
			{
				stop = true;
				D2D1_COLOR_F color = pBrush->GetColor();
				(*rect).second.SetColor(color);
			}
		}
		if (stop) return;
		screenColor = pBrush->GetColor();
}

void MainWindow::ChangeColor()
{
	if (ChooseColor(&cc))
	{
		D2D1_COLOR_F color = D2D1::ColorF(cc.rgbResult);
		D2D1_COLOR_F sec;
		sec.r = color.b;
		sec.g = color.g;
		sec.b = color.r;
		sec.a = color.a;
		pBrush->SetColor(sec);
	}
}

void MainWindow::ChangeWidth(int width)
{
	static int previousWidth = WIDTH_1;
	CheckMenuItem(penWidthMenu, width, MF_CHECKED);
	CheckMenuItem(penWidthMenu, previousWidth, MF_UNCHECKED);
	previousWidth = width;
}

void MainWindow::OnPaint()
{
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr))
	{
		PAINTSTRUCT ps;
		hdc = BeginPaint(m_hwnd, &ps);

		pRenderTarget->BeginDraw();
		pRenderTarget->Clear(screenColor);

		for (int i = 0; i <= id; i++)
		{
			bool cont = false;
			for (auto eraser : clears)
			{
				if (eraser.first == i)
				{
					eraser.second.Draw(pRenderTarget, pBrush);
					cont = true;
				}
			}
			if (cont) continue;
			for (auto ellipse : elipses)
			{
				if (ellipse.first == i)
				{
					ellipse.second.Draw(pRenderTarget, pBrush);
					cont = true;
				}
			}
			if (cont) continue;
			for (auto rect : rects)
			{
				if (rect.first == i)
				{
					rect.second.Draw(pRenderTarget, pBrush);
					cont = true;
				}
			}
			if (cont) continue;
			for (auto pen : pens)
			{
				if (pen.first == i)
				{
					pen.second.Draw(pRenderTarget, pBrush);
					cont = true;
				}
			}
		}
		hr = pRenderTarget->EndDraw();
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			DiscardGraphicsResources();
		}
		EndPaint(m_hwnd, &ps);
	}
}

void MainWindow::Resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

		pRenderTarget->Resize(size);

		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

HRESULT MainWindow::InsertEllipse(float x, float y)
{
	MyEllipse* myEllipse = new MyEllipse();
	myEllipse->ellipse.point = ptMouse = D2D1::Point2F(x, y);
	myEllipse->ellipse.radiusX = myEllipse->ellipse.radiusY = 2.0f;
	myEllipse->color = pBrush->GetColor();
	myEllipse->strokeWidth = strokeWidth;
	pair<int, MyEllipse> pair;
	pair.first = id;
	pair.second = *myEllipse;
	elipses.insert(pair);
	id++;
	delete myEllipse;
	return S_OK;
}

HRESULT MainWindow::InsertRectangle(float x, float y)
{
	MyRectangle* myRectangle = new MyRectangle();
	myRectangle->rect.left = x;
	myRectangle->rect.top = y;
	myRectangle->rect.right = x + 2.0f;
	myRectangle->rect.bottom = y + 2.0f;
	myRectangle->color = pBrush->GetColor();
	myRectangle->strokeWidth = strokeWidth;
	pair<int, MyRectangle> pair;
	pair.first = id;
	pair.second = *myRectangle;
	rects.insert(pair);
	id++;
	delete myRectangle;
	return S_OK;
}

HRESULT MainWindow::InsertPen(float x, float y)
{
	MyPen* myPen = new MyPen();
	myPen->color = pBrush->GetColor();
	myPen->points.push_back(D2D1::Point2F(x, y));
	myPen->points.push_back(D2D1::Point2F(x + 1, y + 1));
	myPen->strokeWidth = strokeWidth;
	pair<int, MyPen> pair;
	pair.first = id;
	pair.second = *myPen;
	pens.insert(pair);
	id++;
	delete myPen;
	return S_OK;
}

HRESULT MainWindow::InsertClear(float x, float y)
{
	Clearer* myClearer = new Clearer();
	myClearer->points.push_back(D2D1::Point2F(x, y));
	myClearer->points.push_back(D2D1::Point2F(x + 1, y + 1));
	myClearer->strokeWidth = strokeWidth;
	pair<int, Clearer> pair;
	pair.first = id;
	pair.second = *myClearer;
	clears.insert(pair);
	id++;
	delete myClearer;
	return S_OK;
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	POINT pt = { dipX, dipY };
	switch (mode)
	{
	case Mode::Draw:
		if (DragDetect(m_hwnd, pt))
		{
			SetCapture(m_hwnd);
		}
		switch (shape)
		{
		case Shape::Ellipse:
			InsertEllipse(dipX, dipY);
			break;
		case Shape::Rectangle:
			InsertRectangle(dipX, dipY);
			break;
		case Shape::Pen:
			InsertPen(dipX, dipY);
			break;
		}
		break;
	case Mode::Clear:
		InsertClear(dipX, dipY);
		break;
	case Mode::Fill:
		FillArea(dipX, dipY);
		break;
	}
	InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	if (MK_LBUTTON & flags)
	{
		switch (mode)
		{
			case Mode::Draw:
				if (shape == Shape::Ellipse)
				{
					const float width = (dipX - ptMouse.x) / 2;
					const float height = (dipY - ptMouse.y) / 2;
					const float x1 = ptMouse.x + width;
					const float y1 = ptMouse.y + height;
					map<int, MyEllipse>::reverse_iterator lElipse = elipses.rbegin();
					(*lElipse).second.ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
				}
				if (shape == Shape::Rectangle)
				{
					map<int, MyRectangle>::reverse_iterator lRect = rects.rbegin();
					 (*lRect).second.rect = D2D1::RectF((*lRect).second.rect.left, (*lRect).second.rect.top, dipX, dipY);
				}
				if (shape == Shape::Pen)
				{
					map<int, MyPen>::reverse_iterator lPen = pens.rbegin();
					(*lPen).second.points.push_back(D2D1::Point2F(dipX, dipY));
				}
				break;
			case Mode::Clear:
				if (shape == Shape::Pen)
				{
					map<int, Clearer>::reverse_iterator lPen = clears.rbegin();
					(*lPen).second.points.push_back(D2D1::Point2F(dipX, dipY));
				}
				break;
		}
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

void MainWindow::OnLButtonUp()
{
	ReleaseCapture();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	MainWindow win;

	if (!win.Create(L"Paint", WS_OVERLAPPEDWINDOW))
	{
		return 0;
	}

	ShowWindow(win.Window(), nCmdShow);
	UpdateWindow(win.Window());

	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		HWND but;
	case WM_CREATE:

		if (FAILED(D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			return -1;  // Fail CreateWindowEx.
		}
		IntitializeComponents();
		DPIScale::Initialize(pFactory);
		SetMode(Mode::Draw);
		SetShape(Shape::Ellipse);
		return 0;

	case WM_CLOSE:
		if (MessageBox(this->m_hwnd, L"Are you sure you want to exit?", L"Exit", MB_OKCANCEL) == IDOK)
		{
			DestroyWindow(this->m_hwnd);
		}
		return 0;
	case WM_DESTROY:
		DiscardGraphicsResources();
		SafeRelease(&pFactory);
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_LBUTTONDOWN:
		OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), DWORD(wParam));
		return 0;
	case WM_LBUTTONUP:
		OnLButtonUp();
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), DWORD(wParam));
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_RECTPRESS:
			SetMode(Mode::Draw);
			SetShape(Shape::Rectangle);
			return 0;
		case IDM_ELLIPPRESS:
			SetMode(Mode::Draw);
			SetShape(Shape::Ellipse);
			return 0;
		case IDM_PEN:
			SetMode(Mode::Draw);
			SetShape(Shape::Pen);
			return 0;
		case IDM_CLEAR:
			CleanUp();
			return 0;
		case IDM_COLOR:
			ChangeColor();
			return 0;
		case IDM_FILL:
			SetMode(Mode::Fill);
			return 0;
		case IDM_CLEANER:
			SetMode(Mode::Clear);
			SetShape(Shape::Pen);
			return 0;
		case WIDTH_1:
		case WIDTH_2:
		case WIDTH_3:
		case WIDTH_4:
		case WIDTH_5:
		case WIDTH_6:
			if (wParam <= WIDTH_6 && wParam >= WIDTH_1)
			{
				ChangeWidth(wParam);
				strokeWidth = wParam;
			}
			return 0;

		}
		return 0;
	}
	return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}