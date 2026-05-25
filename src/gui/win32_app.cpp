#include "lifeengine/gui/application.hpp"
#include "lifeengine/gui/renderer.hpp"

#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#ifdef hyper
#undef hyper
#endif

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <sstream>
#include <string>
#include <unordered_map>

namespace lifeengine::gui {
namespace {

constexpr int kTimerId = 1;
constexpr int kTimerMs = 16;
constexpr int kPanelWidth = 430;
constexpr int kMargin = 10;
constexpr int kEditorGridPixels = 150;
constexpr int kSpeedSliderMax = 2000;
constexpr int kPanPaintIntervalMs = 16;
constexpr int kToolPaintIntervalMs = 16;
constexpr int kToolStatsIntervalMs = 100;
constexpr int kPanelStatsIntervalMs = 250;
constexpr int kRenderOffPaintIntervalMs = 250;
constexpr int kMetricsIntervalMs = 1000;

enum ControlId {
    IdPlay = 1001,
    IdStep,
    IdReset,
    IdClear,
    IdFoodTool,
    IdWallTool,
    IdKillTool,
    IdInspectTool,
    IdRender,
    IdRandomWalls,
    IdApplyGrid,
    IdApplyRules,
    IdResetView,
    IdSpeedSlider = 1101,
    IdBrushSlider,
    IdCellSizeEdit = 1201,
    IdFoodProbEdit,
    IdLifespanEdit,
    IdGlobalMutationEdit,
    IdFoodDropEdit,
    IdMaxOrgEdit,
    IdRotationCheck = 1301,
    IdInstaKillCheck,
    IdMoversProduceCheck,
    IdFoodBlocksCheck,
    IdUseGlobalMutationCheck,
    IdEditorMouth = 1401,
    IdEditorProducer,
    IdEditorMover,
    IdEditorKiller,
    IdEditorArmor,
    IdEditorEye,
    IdEditorReset,
    IdDropEditor,
};

COLORREF colorRef(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

std::uint32_t dibColor(std::uint32_t rgb) {
    return ((rgb & 0x0000ffu) << 16) | (rgb & 0x00ff00u) | ((rgb & 0xff0000u) >> 16);
}

int rectHeight(const RECT& rect) {
    return static_cast<int>(rect.bottom - rect.top);
}

std::wstring widen(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

std::wstring fixedDouble(double value, int precision = 2) {
    std::wostringstream out;
    out.setf(std::ios::fixed);
    out.precision(precision);
    out << value;
    return out.str();
}

} // namespace

class GdiRenderSurface : public RenderSurface {
public:
    GdiRenderSurface(HDC hdc, HBRUSH eyeSlitBrush, std::unordered_map<std::uint32_t, HBRUSH>& brushes)
        : hdc_(hdc), eyeSlitBrush_(eyeSlitBrush), brushes_(brushes) {}

    void fillRect(const Rect& rect, std::uint32_t rgb) override {
        HBRUSH brush = brushFor(rgb);
        RECT nativeRect{
            static_cast<LONG>(rect.left),
            static_cast<LONG>(rect.top),
            static_cast<LONG>(rect.right),
            static_cast<LONG>(rect.bottom)};
        FillRect(hdc_, &nativeRect, brush);
    }

    void drawEyeSlit(const Rect& cellRect, Direction direction, std::uint32_t) override {
        int width = cellRect.width();
        int height = cellRect.height();
        int thin = std::max(1, width / 4);
        int longSide = std::max(1, (height * 3) / 4);
        RECT slit{
            static_cast<LONG>(cellRect.left),
            static_cast<LONG>(cellRect.top),
            static_cast<LONG>(cellRect.right),
            static_cast<LONG>(cellRect.bottom)};
        if (direction == Direction::Up || direction == Direction::Down) {
            slit.left = cellRect.left + (width / 2) - (thin / 2);
            slit.right = slit.left + thin;
            slit.top = cellRect.top + (height / 2) - (longSide / 2);
            slit.bottom = slit.top + longSide;
        } else {
            slit.top = cellRect.top + (height / 2) - (thin / 2);
            slit.bottom = slit.top + thin;
            slit.left = cellRect.left + (width / 2) - (longSide / 2);
            slit.right = slit.left + longSide;
        }
        FillRect(hdc_, &slit, eyeSlitBrush_);
    }

private:
    HBRUSH brushFor(std::uint32_t rgb) {
        auto found = brushes_.find(rgb);
        if (found != brushes_.end()) {
            return found->second;
        }
        HBRUSH brush = CreateSolidBrush(colorRef(rgb));
        brushes_[rgb] = brush;
        return brush;
    }

    HDC hdc_ = nullptr;
    HBRUSH eyeSlitBrush_ = nullptr;
    std::unordered_map<std::uint32_t, HBRUSH>& brushes_;
};

class DibRenderSurface : public RenderSurface {
public:
    DibRenderSurface(std::uint32_t* pixels, int width, int height)
        : pixels_(pixels), width_(width), height_(height) {}

    void fillRect(const Rect& rect, std::uint32_t rgb) override {
        if (pixels_ == nullptr) {
            return;
        }
        int left = std::clamp(rect.left, 0, width_);
        int top = std::clamp(rect.top, 0, height_);
        int right = std::clamp(rect.right, 0, width_);
        int bottom = std::clamp(rect.bottom, 0, height_);
        if (right <= left || bottom <= top) {
            return;
        }
        std::uint32_t color = dibColor(rgb);
        for (int y = top; y < bottom; ++y) {
            std::fill(pixels_ + (y * width_) + left, pixels_ + (y * width_) + right, color);
        }
    }

    void drawEyeSlit(const Rect& cellRect, Direction direction, std::uint32_t rgb) override {
        int width = cellRect.width();
        int height = cellRect.height();
        int thin = std::max(1, width / 4);
        int longSide = std::max(1, (height * 3) / 4);
        Rect slit{cellRect.left, cellRect.top, cellRect.right, cellRect.bottom};
        if (direction == Direction::Up || direction == Direction::Down) {
            slit.left = cellRect.left + (width / 2) - (thin / 2);
            slit.right = slit.left + thin;
            slit.top = cellRect.top + (height / 2) - (longSide / 2);
            slit.bottom = slit.top + longSide;
        } else {
            slit.top = cellRect.top + (height / 2) - (thin / 2);
            slit.bottom = slit.top + thin;
            slit.left = cellRect.left + (width / 2) - (longSide / 2);
            slit.right = slit.left + longSide;
        }
        fillRect(slit, rgb);
    }

private:
    std::uint32_t* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

class Win32GuiApp {
public:
    Win32GuiApp() : model_(160, 100, 5, 1) {
        lastTick_ = std::chrono::steady_clock::now();
        lastMetricsTick_ = lastTick_;
        lastCanvasPaintRequest_ = lastTick_;
        lastStatsUpdate_ = lastTick_;
    }

    ~Win32GuiApp() {
        for (HBRUSH brush : cellBrushes_) {
            if (brush != nullptr) {
                DeleteObject(brush);
            }
        }
        if (panelBrush_ != nullptr) {
            DeleteObject(panelBrush_);
        }
        if (editBrush_ != nullptr) {
            DeleteObject(editBrush_);
        }
        if (canvasBrush_ != nullptr) {
            DeleteObject(canvasBrush_);
        }
        if (fieldBrush_ != nullptr) {
            DeleteObject(fieldBrush_);
        }
        if (fieldBorderBrush_ != nullptr) {
            DeleteObject(fieldBorderBrush_);
        }
        if (eyeSlitBrush_ != nullptr) {
            DeleteObject(eyeSlitBrush_);
        }
        for (auto& entry : renderBrushes_) {
            DeleteObject(entry.second);
        }
        destroyCanvasBuffer();
    }

    int run(int showCommand) {
        INITCOMMONCONTROLSEX controls{};
        controls.dwSize = sizeof(controls);
        controls.dwICC = ICC_BAR_CLASSES;
        InitCommonControlsEx(&controls);

        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Win32GuiApp::windowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"LifeEngineNativeWindow";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = nullptr;
        RegisterClassExW(&wc);

        hwnd_ = CreateWindowExW(
            0,
            wc.lpszClassName,
            L"The Life Engine - C++ Native",
            WS_OVERLAPPEDWINDOW | WS_VSCROLL,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1240,
            760,
            nullptr,
            nullptr,
            instance,
            this);

        if (hwnd_ == nullptr) {
            return 1;
        }

        createBrushes();
        createControls();
        layoutControls();
        refreshControlsFromModel();
        SetTimer(hwnd_, kTimerId, kTimerMs, nullptr);

        ShowWindow(hwnd_, showCommand);
        UpdateWindow(hwnd_);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        Win32GuiApp* app = nullptr;
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<Win32GuiApp*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            app->hwnd_ = hwnd;
        } else {
            app = reinterpret_cast<Win32GuiApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (app == nullptr) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
        return app->handleMessage(message, wParam, lParam);
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_SIZE:
            layoutControls();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            onTimer();
            return 0;
        case WM_COMMAND:
            onCommand(LOWORD(wParam));
            return 0;
        case WM_DRAWITEM:
            return drawOwnerControl(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)) ? TRUE : FALSE;
        case WM_CTLCOLORSTATIC:
            return reinterpret_cast<LRESULT>(brushStaticControl(reinterpret_cast<HDC>(wParam)));
        case WM_CTLCOLOREDIT:
            return reinterpret_cast<LRESULT>(brushEditControl(reinterpret_cast<HDC>(wParam)));
        case WM_CTLCOLORBTN:
            return reinterpret_cast<LRESULT>(panelBrush_);
        case WM_HSCROLL:
            onScroll(reinterpret_cast<HWND>(lParam));
            return 0;
        case WM_VSCROLL:
            onPanelScroll(LOWORD(wParam));
            return 0;
        case WM_LBUTTONDOWN:
            if (onEditorPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false)) {
                return 0;
            }
            onPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false, true);
            return 0;
        case WM_RBUTTONDOWN:
            if (onEditorPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true)) {
                return 0;
            }
            onPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true, true);
            return 0;
        case WM_MBUTTONDOWN:
            beginPan(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE:
            if (panning_) {
                updatePan(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            } else if (mouseDown_) {
                bool secondary = (wParam & MK_RBUTTON) != 0;
                onPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), secondary, false);
            }
            return 0;
        case WM_MOUSEWHEEL:
            onMouseWheel(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_KEYDOWN:
            onKeyDown(wParam);
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            bool finishedToolDrag = mouseDown_ && !panning_;
            mouseDown_ = false;
            panning_ = false;
            resetToolDragCache();
            ReleaseCapture();
            if (finishedToolDrag) {
                auto now = std::chrono::steady_clock::now();
                updateStats();
                requestCanvasPaint(now, 0);
            }
            return 0;
        }
        case WM_PAINT:
            paint();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd_, kTimerId);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }

    void createControls() {
        font_ = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        playButton_ = button(L"Pause", IdPlay);
        stepButton_ = button(L"Step", IdStep);
        resetButton_ = button(L"Reset", IdReset);
        clearButton_ = button(L"Clear", IdClear);
        renderButton_ = button(L"Render On", IdRender);
        resetViewButton_ = button(L"Reset View", IdResetView);
        randomWallsButton_ = button(L"Random Walls", IdRandomWalls);
        applyGridButton_ = button(L"Resize Grid", IdApplyGrid);
        applyRulesButton_ = button(L"Apply Rules", IdApplyRules);

        foodTool_ = radio(L"Food", IdFoodTool);
        wallTool_ = radio(L"Wall", IdWallTool);
        killTool_ = radio(L"Kill", IdKillTool);
        inspectTool_ = radio(L"Pan", IdInspectTool);

        speedSlider_ = trackbar(IdSpeedSlider, 1, kSpeedSliderMax, 60);
        brushSlider_ = trackbar(IdBrushSlider, 0, 15, 2);

        cellSizeEdit_ = edit(L"5", IdCellSizeEdit);
        foodProbEdit_ = edit(L"5", IdFoodProbEdit);
        lifespanEdit_ = edit(L"100", IdLifespanEdit);
        globalMutationEdit_ = edit(L"5", IdGlobalMutationEdit);
        foodDropEdit_ = edit(L"0", IdFoodDropEdit);
        maxOrgEdit_ = edit(L"-1", IdMaxOrgEdit);

        rotationCheck_ = checkbox(L"Rotation", IdRotationCheck, true);
        instaKillCheck_ = checkbox(L"One touch kill", IdInstaKillCheck, false);
        moversProduceCheck_ = checkbox(L"Movers produce", IdMoversProduceCheck, false);
        foodBlocksCheck_ = checkbox(L"Food blocks reproduction", IdFoodBlocksCheck, true);
        useGlobalMutationCheck_ = checkbox(L"Use global mutation", IdUseGlobalMutationCheck, false);

        editorMouthTool_ = radio(L"Mouth", IdEditorMouth);
        editorProducerTool_ = radio(L"Producer", IdEditorProducer);
        editorMoverTool_ = radio(L"Mover", IdEditorMover);
        editorKillerTool_ = radio(L"Killer", IdEditorKiller);
        editorArmorTool_ = radio(L"Armor", IdEditorArmor);
        editorEyeTool_ = radio(L"Eye", IdEditorEye);
        editorResetButton_ = button(L"Reset Editor", IdEditorReset);
        dropEditorButton_ = button(L"Drop", IdDropEditor);

        titleLabel_ = label(L"The Life Engine");
        speedLabel_ = label(L"Speed: 60");
        brushLabel_ = label(L"Brush: 2");
        gridLabel_ = label(L"Grid");
        rulesLabel_ = label(L"Evolution");
        statsLabel_ = label(L"Stats");
        populationLabel_ = label(L"Population: 0");
        speciesLabel_ = label(L"Species: 0");
        tickLabel_ = label(L"Ticks: 0");
        mutationLabel_ = label(L"Avg mutation: 0");
        largestLabel_ = label(L"Largest: 0");
        topSpeciesLabel_ = label(L"Top species: none");
        editorLabel_ = label(L"Organism Editor");
        editorHintLabel_ = label(L"Left: add/rotate eye  Right: remove");

        CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdFoodTool);
        CheckRadioButton(hwnd_, IdEditorMouth, IdEditorEye, IdEditorMouth);
    }

    HWND button(const wchar_t* text, int id) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND radio(const wchar_t* text, int id) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND checkbox(const wchar_t* text, int id, bool checked) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        setFont(control);
        return control;
    }

    HWND edit(const wchar_t* text, int id) {
        HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND label(const wchar_t* text) {
        HWND control = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, hwnd_, nullptr, nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND trackbar(int id, int min, int max, int value) {
        HWND control = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        SendMessageW(control, TBM_SETRANGE, TRUE, MAKELPARAM(min, max));
        SendMessageW(control, TBM_SETPOS, TRUE, value);
        setFont(control);
        return control;
    }

    void setFont(HWND control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }

    void setText(HWND control, const std::wstring& text) {
        wchar_t current[256]{};
        GetWindowTextW(control, current, 256);
        if (text != current) {
            SetWindowTextW(control, text.c_str());
        }
    }

    HBRUSH brushStaticControl(HDC hdc) const {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(218, 224, 232));
        return panelBrush_;
    }

    HBRUSH brushEditControl(HDC hdc) const {
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(26, 31, 39));
        SetTextColor(hdc, RGB(235, 240, 246));
        return editBrush_;
    }

    bool drawOwnerControl(DRAWITEMSTRUCT* item) {
        if (item == nullptr || item->CtlType != ODT_BUTTON) {
            return false;
        }

        wchar_t text[128]{};
        GetWindowTextW(item->hwndItem, text, 128);
        bool pressed = (item->itemState & ODS_SELECTED) != 0;
        bool focused = (item->itemState & ODS_FOCUS) != 0;
        bool checkedState = SendMessageW(item->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int id = static_cast<int>(item->CtlID);

        if (isCheckboxId(id)) {
            drawCheckControl(item->hDC, item->rcItem, text, checkedState, pressed, focused);
        } else {
            bool accent = checkedState || id == IdPlay || id == IdApplyGrid || id == IdApplyRules || id == IdDropEditor;
            drawPillButton(item->hDC, item->rcItem, text, checkedState, pressed, focused, accent);
        }
        return true;
    }

    void drawPillButton(HDC hdc, RECT rect, const wchar_t* text, bool checkedState, bool pressed, bool focused, bool accent) {
        COLORREF fill = checkedState ? RGB(44, 112, 174) : (accent ? RGB(55, 67, 82) : RGB(38, 45, 55));
        COLORREF border = checkedState ? RGB(91, 169, 238) : RGB(74, 86, 102);
        COLORREF textColor = RGB(237, 242, 248);
        if (pressed) {
            fill = checkedState ? RGB(34, 91, 145) : RGB(30, 36, 45);
        }
        if (focused) {
            border = RGB(122, 182, 235);
        }

        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        RECT textRect = rect;
        InflateRect(&textRect, -8, 0);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        DrawTextW(hdc, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    void drawCheckControl(HDC hdc, RECT rect, const wchar_t* text, bool checkedState, bool pressed, bool focused) {
        FillRect(hdc, &rect, panelBrush_);
        RECT box{rect.left + 2, rect.top + 5, rect.left + 18, rect.top + 21};
        COLORREF fill = checkedState ? RGB(44, 112, 174) : RGB(33, 39, 48);
        COLORREF border = focused ? RGB(122, 182, 235) : RGB(79, 91, 107);
        if (pressed) {
            fill = RGB(28, 34, 42);
        }

        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        RoundRect(hdc, box.left, box.top, box.right, box.bottom, 5, 5);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        if (checkedState) {
            HPEN markPen = CreatePen(PS_SOLID, 2, RGB(240, 247, 255));
            HGDIOBJ oldMark = SelectObject(hdc, markPen);
            MoveToEx(hdc, box.left + 4, box.top + 8, nullptr);
            LineTo(hdc, box.left + 7, box.top + 11);
            LineTo(hdc, box.right - 3, box.top + 4);
            SelectObject(hdc, oldMark);
            DeleteObject(markPen);
        }

        RECT textRect{rect.left + 24, rect.top, rect.right, rect.bottom};
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(218, 224, 232));
        DrawTextW(hdc, text, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    bool isCheckboxId(int id) const {
        return id == IdRotationCheck || id == IdInstaKillCheck || id == IdMoversProduceCheck ||
               id == IdFoodBlocksCheck || id == IdUseGlobalMutationCheck;
    }

    bool isToolId(int id) const {
        return id == IdFoodTool || id == IdWallTool || id == IdKillTool || id == IdInspectTool;
    }

    bool isEditorToolId(int id) const {
        return id == IdEditorMouth || id == IdEditorProducer || id == IdEditorMover ||
               id == IdEditorKiller || id == IdEditorArmor || id == IdEditorEye;
    }

    void invalidateControl(HWND control) {
        if (control != nullptr) {
            InvalidateRect(control, nullptr, TRUE);
        }
    }

    void selectTool(ToolMode tool, int controlId) {
        CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, controlId);
        model_.setTool(tool);
        invalidateToolButtons();
    }

    void selectEditorState(CellState state, int controlId) {
        CheckRadioButton(hwnd_, IdEditorMouth, IdEditorEye, controlId);
        selectedEditorState_ = state;
        invalidateEditorToolButtons();
    }

    void invalidateToolButtons() {
        invalidateControl(foodTool_);
        invalidateControl(wallTool_);
        invalidateControl(killTool_);
        invalidateControl(inspectTool_);
    }

    void invalidateEditorToolButtons() {
        invalidateControl(editorMouthTool_);
        invalidateControl(editorProducerTool_);
        invalidateControl(editorMoverTool_);
        invalidateControl(editorKillerTool_);
        invalidateControl(editorArmorTool_);
        invalidateControl(editorEyeTool_);
    }

    void toggleOwnerCheckbox(HWND control) {
        if (control == nullptr) {
            return;
        }
        bool isChecked = SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SendMessageW(control, BM_SETCHECK, isChecked ? BST_UNCHECKED : BST_CHECKED, 0);
        invalidateControl(control);
    }

    void layoutControls() {
        RECT client{};
        GetClientRect(hwnd_, &client);
        canvasRect_ = client;
        canvasRect_.right = std::max(canvasRect_.left, client.right - kPanelWidth);
        panelRect_ = client;
        panelRect_.left = canvasRect_.right;

        int x = panelRect_.left + kMargin;
        int y = kMargin - panelScroll_;
        int w = kPanelWidth - (kMargin * 2);
        int row = 28;
        int gap = 7;

        move(titleLabel_, x, y, w, 28);
        y += 34;

        move(playButton_, x, y, 70, row);
        move(stepButton_, x + 76, y, 70, row);
        move(resetButton_, x + 152, y, 76, row);
        move(clearButton_, x + 234, y, 76, row);
        y += row + gap;

        move(speedLabel_, x, y, w, 20);
        y += 20;
        move(speedSlider_, x, y, w, 30);
        y += 34;

        move(brushLabel_, x, y, w, 20);
        y += 20;
        move(brushSlider_, x, y, w, 30);
        y += 36;

        move(foodTool_, x, y, 72, row);
        move(wallTool_, x + 76, y, 72, row);
        move(killTool_, x + 152, y, 72, row);
        move(inspectTool_, x + 228, y, 82, row);
        y += row + gap;

        move(renderButton_, x, y, 94, row);
        move(resetViewButton_, x + 100, y, 96, row);
        move(randomWallsButton_, x + 202, y, 108, row);
        y += row + 12;

        y = layoutEditorControls(x, y, w);

        move(gridLabel_, x, y, w, 20);
        y += 22;
        moveLabelAndEdit(L"Cell size", cellSizeEdit_, x, y, w);
        y += row + gap;
        move(applyGridButton_, x, y, 120, row);
        y += row + 12;

        move(rulesLabel_, x, y, w, 20);
        y += 22;
        moveLabelAndEdit(L"Food prod %", foodProbEdit_, x, y, w);
        y += row + gap;
        moveLabelAndEdit(L"Lifespan x", lifespanEdit_, x, y, w);
        y += row + gap;
        moveLabelAndEdit(L"Global mut %", globalMutationEdit_, x, y, w);
        y += row + gap;
        moveLabelAndEdit(L"Food drop %", foodDropEdit_, x, y, w);
        y += row + gap;
        moveLabelAndEdit(L"Max organisms", maxOrgEdit_, x, y, w);
        y += row + gap;

        move(rotationCheck_, x, y, 140, row);
        move(instaKillCheck_, x + 150, y, 160, row);
        y += row;
        move(moversProduceCheck_, x, y, 140, row);
        move(foodBlocksCheck_, x + 150, y, 160, row);
        y += row;
        move(useGlobalMutationCheck_, x, y, 190, row);
        y += row + gap;
        move(applyRulesButton_, x, y, 120, row);
        y += row + 12;

        move(statsLabel_, x, y, w, 20);
        y += 24;
        move(tickLabel_, x, y, w, 20);
        y += 22;
        move(populationLabel_, x, y, w, 20);
        y += 22;
        move(speciesLabel_, x, y, w, 20);
        y += 22;
        move(mutationLabel_, x, y, w, 20);
        y += 22;
        move(largestLabel_, x, y, w, 20);
        y += 22;
        move(topSpeciesLabel_, x, y, w, 40);
        y += 50;
        int contentBottom = y + panelScroll_;
        maxPanelScroll_ = std::max(0, contentBottom - rectHeight(panelRect_) + kMargin);
        int previousScroll = panelScroll_;
        panelScroll_ = std::clamp(panelScroll_, 0, maxPanelScroll_);
        syncPanelScrollbar();
        if (panelScroll_ != previousScroll) {
            layoutControls();
        }
    }

    int layoutEditorControls(int x, int y, int w) {
        move(editorLabel_, x, y, w, 20);
        y += 22;
        editorRect_ = {
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            static_cast<LONG>(x + kEditorGridPixels),
            static_cast<LONG>(y + kEditorGridPixels)};

        int paletteX = x + kEditorGridPixels + 12;
        int paletteW = w - kEditorGridPixels - 12;
        move(editorMouthTool_, paletteX, y, paletteW, 22);
        move(editorProducerTool_, paletteX, y + 24, paletteW, 22);
        move(editorMoverTool_, paletteX, y + 48, paletteW, 22);
        move(editorKillerTool_, paletteX, y + 72, paletteW, 22);
        move(editorArmorTool_, paletteX, y + 96, paletteW, 22);
        move(editorEyeTool_, paletteX, y + 120, paletteW, 22);
        move(editorResetButton_, paletteX, y + 146, paletteW, 24);
        move(dropEditorButton_, paletteX, y + 174, paletteW, 24);
        y += 205;
        move(editorHintLabel_, x, y, w, 20);
        return y + 30;
    }

    void moveLabelAndEdit(const wchar_t* labelText, HWND editControl, int x, int y, int width) {
        HWND temp = nullptr;
        if (labelText == std::wstring(L"Cell size")) {
            temp = cellSizeText_;
        } else if (labelText == std::wstring(L"Food prod %")) {
            temp = foodProbText_;
        } else if (labelText == std::wstring(L"Lifespan x")) {
            temp = lifespanText_;
        } else if (labelText == std::wstring(L"Global mut %")) {
            temp = globalMutationText_;
        } else if (labelText == std::wstring(L"Food drop %")) {
            temp = foodDropText_;
        } else if (labelText == std::wstring(L"Max organisms")) {
            temp = maxOrgText_;
        }
        if (temp == nullptr) {
            temp = label(labelText);
            if (labelText == std::wstring(L"Cell size")) {
                cellSizeText_ = temp;
            } else if (labelText == std::wstring(L"Food prod %")) {
                foodProbText_ = temp;
            } else if (labelText == std::wstring(L"Lifespan x")) {
                lifespanText_ = temp;
            } else if (labelText == std::wstring(L"Global mut %")) {
                globalMutationText_ = temp;
            } else if (labelText == std::wstring(L"Food drop %")) {
                foodDropText_ = temp;
            } else if (labelText == std::wstring(L"Max organisms")) {
                maxOrgText_ = temp;
            }
        }
        move(temp, x, y + 4, width - 90, 22);
        move(editControl, x + width - 86, y, 86, 24);
    }

    void move(HWND control, int x, int y, int width, int height) {
        if (control != nullptr) {
            MoveWindow(control, x, y, width, height, FALSE);
        }
    }

    void onTimer() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick_);
        lastTick_ = now;
        model_.advance(elapsed);
        updateCanvasMetrics(now);
        requestStats(now, kPanelStatsIntervalMs);
        if (model_.settings().renderEnabled) {
            requestCanvasPaint(now, 0);
        } else {
            requestCanvasPaint(now, kRenderOffPaintIntervalMs);
        }
    }

    void onScroll(HWND source) {
        if (source == speedSlider_) {
            int position = static_cast<int>(SendMessageW(speedSlider_, TBM_GETPOS, 0, 0));
            model_.setTargetFps(position);
            setText(speedLabel_, L"Speed: " + speedText());
        } else if (source == brushSlider_) {
            int position = static_cast<int>(SendMessageW(brushSlider_, TBM_GETPOS, 0, 0));
            model_.setBrushSize(position);
            setText(brushLabel_, L"Brush: " + std::to_wstring(position));
        }
    }

    void onPanelScroll(int request) {
        int next = panelScroll_;
        const int page = std::max(1, rectHeight(panelRect_) - (kMargin * 2));
        switch (request) {
        case SB_LINEUP:
            next -= 40;
            break;
        case SB_LINEDOWN:
            next += 40;
            break;
        case SB_PAGEUP:
            next -= page;
            break;
        case SB_PAGEDOWN:
            next += page;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd_, SB_VERT, &info);
            next = info.nTrackPos;
            break;
        }
        case SB_TOP:
            next = 0;
            break;
        case SB_BOTTOM:
            next = maxPanelScroll_;
            break;
        default:
            return;
        }
        scrollPanelTo(next);
    }

    void scrollPanelTo(int next) {
        int clamped = std::clamp(next, 0, maxPanelScroll_);
        if (clamped == panelScroll_) {
            return;
        }
        panelScroll_ = clamped;
        layoutControls();
        InvalidateRect(hwnd_, &panelRect_, TRUE);
    }

    void syncPanelScrollbar() {
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
        info.nMin = 0;
        info.nMax = maxPanelScroll_ + std::max(1, rectHeight(panelRect_)) - 1;
        info.nPage = static_cast<UINT>(std::max(1, rectHeight(panelRect_)));
        info.nPos = panelScroll_;
        SetScrollInfo(hwnd_, SB_VERT, &info, TRUE);
    }

    void onCommand(int id) {
        switch (id) {
        case IdPlay:
            model_.toggleRunning();
            setText(playButton_, model_.settings().running ? L"Pause" : L"Play");
            break;
        case IdStep:
            model_.stepOnce();
            updateStats();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdReset:
            model_.resetWorld(true);
            updateStats();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdClear:
            model_.clearWorld();
            updateStats();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdFoodTool:
            selectTool(ToolMode::Food, IdFoodTool);
            break;
        case IdWallTool:
            selectTool(ToolMode::Wall, IdWallTool);
            break;
        case IdKillTool:
            selectTool(ToolMode::Kill, IdKillTool);
            break;
        case IdInspectTool:
            selectTool(ToolMode::Inspect, IdInspectTool);
            break;
        case IdEditorMouth:
            selectEditorState(CellState::Mouth, IdEditorMouth);
            break;
        case IdEditorProducer:
            selectEditorState(CellState::Producer, IdEditorProducer);
            break;
        case IdEditorMover:
            selectEditorState(CellState::Mover, IdEditorMover);
            break;
        case IdEditorKiller:
            selectEditorState(CellState::Killer, IdEditorKiller);
            break;
        case IdEditorArmor:
            selectEditorState(CellState::Armor, IdEditorArmor);
            break;
        case IdEditorEye:
            selectEditorState(CellState::Eye, IdEditorEye);
            break;
        case IdEditorReset:
            model_.editor().setDefaultOrganism();
            InvalidateRect(hwnd_, &editorRect_, FALSE);
            break;
        case IdDropEditor: {
            auto [col, row] = model_.world().gridMap.center();
            model_.dropEditorOrganismAt(col, row);
            updateStats();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        }
        case IdRender:
            model_.toggleRenderEnabled();
            setText(renderButton_, model_.settings().renderEnabled ? L"Render On" : L"Render Off");
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdRandomWalls:
            model_.randomizeWalls();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdResetView:
            resetView();
            break;
        case IdApplyGrid:
            model_.resizeGridToCanvas(canvasWidth(), canvasHeight(), readInt(cellSizeEdit_, model_.settings().cellSize));
            updateStats();
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
            break;
        case IdApplyRules:
            applyRules();
            break;
        case IdRotationCheck:
        case IdInstaKillCheck:
        case IdMoversProduceCheck:
        case IdFoodBlocksCheck:
        case IdUseGlobalMutationCheck:
            toggleOwnerCheckbox(GetDlgItem(hwnd_, id));
            break;
        default:
            break;
        }
    }

    void onPointer(int x, int y, bool secondary, bool begin) {
        if (!pointInCanvas(x, y)) {
            resetToolDragCache();
            return;
        }
        if (model_.settings().tool == ToolMode::Inspect) {
            if (begin && !secondary) {
                beginPan(x, y);
            }
            return;
        }
        if (begin) {
            mouseDown_ = true;
            resetToolDragCache();
            SetCapture(hwnd_);
        }
        auto [col, row] = screenToGrid(x, y);
        if (isDuplicateToolDrag(col, row, secondary)) {
            return;
        }
        rememberToolDrag(col, row, secondary);
        model_.applyToolAt(col, row, secondary);
        auto now = std::chrono::steady_clock::now();
        requestStats(now, begin ? 0 : kToolStatsIntervalMs);
        requestCanvasPaint(now, begin ? 0 : kToolPaintIntervalMs);
    }

    bool isDuplicateToolDrag(int col, int row, bool secondary) const {
        return hasLastToolPoint_ &&
               col == lastToolCol_ &&
               row == lastToolRow_ &&
               secondary == lastToolSecondary_ &&
               model_.settings().tool == lastToolMode_ &&
               model_.settings().brushSize == lastToolBrushSize_;
    }

    void rememberToolDrag(int col, int row, bool secondary) {
        hasLastToolPoint_ = true;
        lastToolCol_ = col;
        lastToolRow_ = row;
        lastToolSecondary_ = secondary;
        lastToolMode_ = model_.settings().tool;
        lastToolBrushSize_ = model_.settings().brushSize;
    }

    void resetToolDragCache() {
        hasLastToolPoint_ = false;
    }

    bool onEditorPointer(int x, int y, bool secondary) {
        if (!pointInRect(editorRect_, x, y)) {
            return false;
        }
        int cellSize = std::max(1, kEditorGridPixels / model_.editor().world().gridMap.cols);
        int col = (x - editorRect_.left) / cellSize;
        int row = (y - editorRect_.top) / cellSize;
        GridCell* cell = model_.editor().world().gridMap.cellAt(col, row);
        if (cell == nullptr) {
            return true;
        }
        if (secondary) {
            model_.editor().removeCellAtGrid(col, row);
        } else if (cell->state == CellState::Eye) {
            model_.editor().rotateEyeAtGrid(col, row);
        } else {
            model_.editor().addCellAtGrid(col, row, selectedEditorState_);
        }
        InvalidateRect(hwnd_, &editorRect_, FALSE);
        return true;
    }

    std::pair<int, int> screenToGrid(int x, int y) const {
        GridPoint point = viewport_.screenToGrid(x - canvasRect_.left, y - canvasRect_.top, model_.settings().cellSize);
        return {point.col, point.row};
    }

    double scaledCellSize() const {
        return viewport_.scaledCellSize(model_.settings().cellSize);
    }

    void beginPan(int x, int y) {
        if (!pointInCanvas(x, y)) {
            return;
        }
        panning_ = true;
        lastPanX_ = x;
        lastPanY_ = y;
        SetCapture(hwnd_);
    }

    void updatePan(int x, int y) {
        viewport_.panBy(x - lastPanX_, y - lastPanY_);
        lastPanX_ = x;
        lastPanY_ = y;
        requestCanvasPaint(std::chrono::steady_clock::now(), kPanPaintIntervalMs);
    }

    void onMouseWheel(int screenX, int screenY, int delta) {
        POINT point{screenX, screenY};
        ScreenToClient(hwnd_, &point);
        if (pointInRect(panelRect_, point.x, point.y)) {
            scrollPanelTo(panelScroll_ - (delta / WHEEL_DELTA) * 80);
            return;
        }
        if (!pointInCanvas(point.x, point.y)) {
            return;
        }
        double factor = delta > 0 ? 1.2 : (1.0 / 1.2);
        viewport_.zoomAt(point.x - canvasRect_.left, point.y - canvasRect_.top, factor);
        requestCanvasPaint(std::chrono::steady_clock::now(), 0);
    }

    void resetView() {
        viewport_.reset();
        InvalidateRect(hwnd_, &canvasRect_, FALSE);
    }

    void onKeyDown(WPARAM key) {
        HWND focus = GetFocus();
        wchar_t className[32]{};
        if (focus != nullptr) {
            GetClassNameW(focus, className, 32);
            if (std::wstring(className) == L"Edit") {
                return;
            }
        }

        switch (key) {
        case VK_SPACE:
            onCommand(IdPlay);
            break;
        case 'A':
            resetView();
            break;
        case 'F':
            selectTool(ToolMode::Food, IdFoodTool);
            break;
        case 'D':
            selectTool(ToolMode::Wall, IdWallTool);
            break;
        case 'G':
            selectTool(ToolMode::Kill, IdKillTool);
            break;
        case 'S':
            selectTool(ToolMode::Inspect, IdInspectTool);
            break;
        case 'H':
            onCommand(IdRender);
            break;
        case 'R':
            onCommand(IdReset);
            break;
        case 'C':
            onCommand(IdClear);
            break;
        default:
            break;
        }
    }

    bool pointInCanvas(int x, int y) const {
        return x >= canvasRect_.left && x < canvasRect_.right && y >= canvasRect_.top && y < canvasRect_.bottom;
    }

    bool pointInRect(const RECT& rect, int x, int y) const {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }

    int canvasWidth() const {
        return std::max(1, static_cast<int>(canvasRect_.right - canvasRect_.left));
    }

    int canvasHeight() const {
        return std::max(1, static_cast<int>(canvasRect_.bottom - canvasRect_.top));
    }

    void applyRules() {
        WorldEnvironment& world = model_.world();
        world.hyper.foodProdProb = readDouble(foodProbEdit_, world.hyper.foodProdProb);
        world.hyper.lifespanMultiplier = readInt(lifespanEdit_, world.hyper.lifespanMultiplier);
        world.hyper.globalMutability = readInt(globalMutationEdit_, world.hyper.globalMutability);
        world.hyper.foodDropProb = readDouble(foodDropEdit_, world.hyper.foodDropProb);
        world.hyper.maxOrganisms = readInt(maxOrgEdit_, world.hyper.maxOrganisms);
        world.hyper.rotationEnabled = checked(rotationCheck_);
        world.hyper.instaKill = checked(instaKillCheck_);
        world.hyper.moversCanProduce = checked(moversProduceCheck_);
        world.hyper.foodBlocksReproduction = checked(foodBlocksCheck_);
        world.hyper.useGlobalMutability = checked(useGlobalMutationCheck_);
    }

    bool checked(HWND control) const {
        return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }

    int readInt(HWND control, int fallback) const {
        wchar_t buffer[64]{};
        GetWindowTextW(control, buffer, 64);
        wchar_t* end = nullptr;
        long value = std::wcstol(buffer, &end, 10);
        return end == buffer ? fallback : static_cast<int>(value);
    }

    double readDouble(HWND control, double fallback) const {
        wchar_t buffer[64]{};
        GetWindowTextW(control, buffer, 64);
        wchar_t* end = nullptr;
        double value = std::wcstod(buffer, &end);
        return end == buffer ? fallback : value;
    }

    std::wstring speedText() const {
        return model_.settings().targetFps >= 5000 ? L"MAX" : std::to_wstring(model_.settings().targetFps);
    }

    void refreshControlsFromModel() {
        setText(speedLabel_, L"Speed: " + speedText());
        setText(brushLabel_, L"Brush: " + std::to_wstring(model_.settings().brushSize));
        updateStats();
    }

    void requestCanvasPaint(std::chrono::steady_clock::time_point now, int minIntervalMs) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCanvasPaintRequest_);
        if (minIntervalMs > 0 && elapsed.count() < minIntervalMs) {
            return;
        }
        lastCanvasPaintRequest_ = now;
        InvalidateRect(hwnd_, &canvasRect_, FALSE);
    }

    void requestStats(std::chrono::steady_clock::time_point now, int minIntervalMs) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatsUpdate_);
        if (minIntervalMs > 0 && elapsed.count() < minIntervalMs) {
            return;
        }
        lastStatsUpdate_ = now;
        updateStats();
    }

    void updateCanvasMetrics(std::chrono::steady_clock::time_point now) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMetricsTick_);
        if (elapsed.count() < kMetricsIntervalMs) {
            return;
        }
        int currentTicks = model_.world().totalTicks;
        int tickDelta = currentTicks >= ticksAtLastMetrics_ ? currentTicks - ticksAtLastMetrics_ : currentTicks;
        double seconds = static_cast<double>(elapsed.count()) / 1000.0;
        measuredFps_ = seconds > 0.0 ? static_cast<double>(framesSinceMetrics_) / seconds : 0.0;
        measuredStepsPerSecond_ = seconds > 0.0 ? static_cast<double>(tickDelta) / seconds : 0.0;
        framesSinceMetrics_ = 0;
        ticksAtLastMetrics_ = currentTicks;
        lastMetricsTick_ = now;
    }

    void updateStats() {
        StatsSnapshot stats = model_.stats();
        setText(tickLabel_, L"Ticks: " + std::to_wstring(stats.ticks));
        setText(populationLabel_, L"Population: " + std::to_wstring(stats.organisms));
        setText(speciesLabel_, L"Species: " + std::to_wstring(stats.species));
        setText(mutationLabel_, L"Avg mutation: " + fixedDouble(stats.averageMutability));
        setText(largestLabel_, L"Largest: " + std::to_wstring(stats.largestCellCount));
        setText(topSpeciesLabel_, L"Top species: " + widen(stats.topSpecies));
    }

    void paint() {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);
        if (rectsIntersect(ps.rcPaint, panelRect_)) {
            FillRect(hdc, &panelRect_, panelBrush_);
        }
        if (rectsIntersect(ps.rcPaint, editorRect_)) {
            paintEditor(hdc);
        }
        if (rectsIntersect(ps.rcPaint, canvasRect_)) {
            paintCanvasBuffered(hdc);
        }
        EndPaint(hwnd_, &ps);
    }

    bool rectsIntersect(const RECT& a, const RECT& b) const {
        return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
    }

    void paintCanvasBuffered(HDC targetDc) {
        int width = canvasWidth();
        int height = canvasHeight();
        if (!ensureCanvasBuffer(targetDc, width, height)) {
            return;
        }

        fillPixels({0, 0, width, height}, 0x12161B);
        if (model_.settings().renderEnabled) {
            paintGrid();
        } else {
            RECT localCanvas{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
            SetBkMode(canvasMemoryDc_, TRANSPARENT);
            SetTextColor(canvasMemoryDc_, RGB(230, 230, 230));
            DrawTextW(canvasMemoryDc_, L"Rendering disabled", -1, &localCanvas, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        paintCanvasOverlay(canvasMemoryDc_);
        BitBlt(targetDc, canvasRect_.left, canvasRect_.top, width, height, canvasMemoryDc_, 0, 0, SRCCOPY);
        ++framesSinceMetrics_;
    }

    bool ensureCanvasBuffer(HDC targetDc, int width, int height) {
        if (canvasMemoryDc_ != nullptr && canvasBitmap_ != nullptr &&
            width == canvasBufferWidth_ && height == canvasBufferHeight_) {
            return true;
        }
        destroyCanvasBuffer();

        canvasMemoryDc_ = CreateCompatibleDC(targetDc);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* pixels = nullptr;
        canvasBitmap_ = CreateDIBSection(targetDc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (canvasMemoryDc_ == nullptr || canvasBitmap_ == nullptr) {
            destroyCanvasBuffer();
            return false;
        }

        canvasOldBitmap_ = SelectObject(canvasMemoryDc_, canvasBitmap_);
        canvasPixels_ = static_cast<std::uint32_t*>(pixels);
        canvasBufferWidth_ = width;
        canvasBufferHeight_ = height;
        return true;
    }

    void destroyCanvasBuffer() {
        if (canvasMemoryDc_ != nullptr) {
            if (canvasOldBitmap_ != nullptr) {
                SelectObject(canvasMemoryDc_, canvasOldBitmap_);
            }
            canvasOldBitmap_ = nullptr;
        }
        if (canvasBitmap_ != nullptr) {
            DeleteObject(canvasBitmap_);
            canvasBitmap_ = nullptr;
        }
        if (canvasMemoryDc_ != nullptr) {
            DeleteDC(canvasMemoryDc_);
            canvasMemoryDc_ = nullptr;
        }
        canvasPixels_ = nullptr;
        canvasBufferWidth_ = 0;
        canvasBufferHeight_ = 0;
    }

    void paintCanvasOverlay(HDC hdc) {
        RECT overlay{
            kMargin,
            kMargin,
            std::max(kMargin + 1, std::min(canvasWidth() - kMargin, 360)),
            kMargin + 24,
        };
        std::wstring text = L"FPS " + fixedDouble(measuredFps_, 0) +
                            L"   SPS " + fixedDouble(measuredStepsPerSecond_, 0) +
                            L"   Target " + speedText();
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(224, 232, 240));
        DrawTextW(hdc, text.c_str(), -1, &overlay, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void paintGrid() {
        fillWorldField();

        DibRenderSurface surface(canvasPixels_, canvasBufferWidth_, canvasBufferHeight_);
        GridRenderOptions options{
            &model_.settings().palette,
            &viewport_,
            model_.settings().cellSize,
            {0, 0, canvasBufferWidth_, canvasBufferHeight_},
            true,
            false,
            0};
        renderGrid(model_.world(), surface, options);
        drawWorldOutline(canvasMemoryDc_);
    }

    void fillWorldField() {
        fillPixels(visibleWorldRect(), model_.settings().palette.empty);
    }

    void drawWorldOutline(HDC hdc) {
        const RECT outline = worldOutlineRect();
        if (outline.right > 0 && outline.left < canvasBufferWidth_ &&
            outline.bottom > 0 && outline.top < canvasBufferHeight_) {
            FrameRect(hdc, &outline, fieldBorderBrush_);
        }
    }

    RECT visibleWorldRect() const {
        RECT outline = worldOutlineRect();
        return {
            (std::max)(0L, outline.left),
            (std::max)(0L, outline.top),
            (std::min)(static_cast<LONG>(canvasBufferWidth_), outline.right),
            (std::min)(static_cast<LONG>(canvasBufferHeight_), outline.bottom),
        };
    }

    RECT worldOutlineRect() const {
        const WorldEnvironment& world = model_.world();
        ScreenPoint topLeft = viewport_.gridToScreen(0, 0, model_.settings().cellSize);
        ScreenPoint bottomRight = viewport_.gridToScreen(world.gridMap.cols, world.gridMap.rows, model_.settings().cellSize);
        return {
            static_cast<LONG>(std::floor(topLeft.x)),
            static_cast<LONG>(std::floor(topLeft.y)),
            static_cast<LONG>(std::ceil(bottomRight.x)),
            static_cast<LONG>(std::ceil(bottomRight.y)),
        };
    }

    void fillPixels(const RECT& rect, std::uint32_t rgb) {
        if (canvasPixels_ == nullptr) {
            return;
        }
        int left = std::clamp(static_cast<int>(rect.left), 0, canvasBufferWidth_);
        int top = std::clamp(static_cast<int>(rect.top), 0, canvasBufferHeight_);
        int right = std::clamp(static_cast<int>(rect.right), 0, canvasBufferWidth_);
        int bottom = std::clamp(static_cast<int>(rect.bottom), 0, canvasBufferHeight_);
        if (right <= left || bottom <= top) {
            return;
        }
        std::uint32_t color = dibColor(rgb);
        for (int y = top; y < bottom; ++y) {
            std::fill(canvasPixels_ + (y * canvasBufferWidth_) + left, canvasPixels_ + (y * canvasBufferWidth_) + right, color);
        }
    }

    void paintEditor(HDC hdc) {
        const WorldEnvironment& editorWorld = model_.editor().world();
        int cellSize = std::max(1, kEditorGridPixels / editorWorld.gridMap.cols);
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 50, 65));
        HGDIOBJ oldPen = SelectObject(hdc, borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, editorRect_.left - 1, editorRect_.top - 1, editorRect_.right + 1, editorRect_.bottom + 1);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        Viewport editorViewport;
        GdiRenderSurface surface(hdc, eyeSlitBrush_, renderBrushes_);
        GridRenderOptions options{
            &model_.settings().palette,
            &editorViewport,
            cellSize,
            {static_cast<int>(editorRect_.left), static_cast<int>(editorRect_.top), static_cast<int>(editorRect_.right), static_cast<int>(editorRect_.bottom)},
            true,
            true,
            0};
        renderGrid(editorWorld, surface, options);
    }

    void createBrushes() {
        const Palette& palette = model_.settings().palette;
        for (CellState state : std::array<CellState, CellStateCount>{
                 CellState::Empty,
                 CellState::Food,
                 CellState::Wall,
                 CellState::Mouth,
                 CellState::Producer,
                 CellState::Mover,
                 CellState::Killer,
                 CellState::Armor,
                 CellState::Eye}) {
            cellBrushes_[stateIndex(state)] = CreateSolidBrush(colorRef(palette.colorFor(state)));
        }
        eyeSlitBrush_ = CreateSolidBrush(colorRef(palette.eyeSlit));
        panelBrush_ = CreateSolidBrush(RGB(29, 34, 42));
        editBrush_ = CreateSolidBrush(RGB(26, 31, 39));
        canvasBrush_ = CreateSolidBrush(RGB(18, 22, 27));
        fieldBrush_ = CreateSolidBrush(colorRef(palette.empty));
        fieldBorderBrush_ = CreateSolidBrush(RGB(82, 94, 110));
    }

    SimulationGuiModel model_;
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    RECT canvasRect_{};
    RECT panelRect_{};
    RECT editorRect_{};
    bool mouseDown_ = false;
    bool panning_ = false;
    int panelScroll_ = 0;
    int maxPanelScroll_ = 0;
    int lastPanX_ = 0;
    int lastPanY_ = 0;
    Viewport viewport_;
    CellState selectedEditorState_ = CellState::Mouth;
    std::chrono::steady_clock::time_point lastTick_;
    std::chrono::steady_clock::time_point lastMetricsTick_;
    std::chrono::steady_clock::time_point lastCanvasPaintRequest_;
    int framesSinceMetrics_ = 0;
    int ticksAtLastMetrics_ = 0;
    double measuredFps_ = 0.0;
    double measuredStepsPerSecond_ = 0.0;
    std::chrono::steady_clock::time_point lastStatsUpdate_;
    bool hasLastToolPoint_ = false;
    int lastToolCol_ = 0;
    int lastToolRow_ = 0;
    bool lastToolSecondary_ = false;
    ToolMode lastToolMode_ = ToolMode::Food;
    int lastToolBrushSize_ = 0;

    std::array<HBRUSH, CellStateCount> cellBrushes_{};
    HBRUSH eyeSlitBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    HBRUSH canvasBrush_ = nullptr;
    HBRUSH fieldBrush_ = nullptr;
    HBRUSH fieldBorderBrush_ = nullptr;
    std::unordered_map<std::uint32_t, HBRUSH> renderBrushes_;
    HDC canvasMemoryDc_ = nullptr;
    HBITMAP canvasBitmap_ = nullptr;
    HGDIOBJ canvasOldBitmap_ = nullptr;
    std::uint32_t* canvasPixels_ = nullptr;
    int canvasBufferWidth_ = 0;
    int canvasBufferHeight_ = 0;

    HWND playButton_ = nullptr;
    HWND stepButton_ = nullptr;
    HWND resetButton_ = nullptr;
    HWND clearButton_ = nullptr;
    HWND renderButton_ = nullptr;
    HWND resetViewButton_ = nullptr;
    HWND randomWallsButton_ = nullptr;
    HWND applyGridButton_ = nullptr;
    HWND applyRulesButton_ = nullptr;
    HWND foodTool_ = nullptr;
    HWND wallTool_ = nullptr;
    HWND killTool_ = nullptr;
    HWND inspectTool_ = nullptr;
    HWND speedSlider_ = nullptr;
    HWND brushSlider_ = nullptr;
    HWND cellSizeEdit_ = nullptr;
    HWND foodProbEdit_ = nullptr;
    HWND lifespanEdit_ = nullptr;
    HWND globalMutationEdit_ = nullptr;
    HWND foodDropEdit_ = nullptr;
    HWND maxOrgEdit_ = nullptr;
    HWND rotationCheck_ = nullptr;
    HWND instaKillCheck_ = nullptr;
    HWND moversProduceCheck_ = nullptr;
    HWND foodBlocksCheck_ = nullptr;
    HWND useGlobalMutationCheck_ = nullptr;
    HWND editorMouthTool_ = nullptr;
    HWND editorProducerTool_ = nullptr;
    HWND editorMoverTool_ = nullptr;
    HWND editorKillerTool_ = nullptr;
    HWND editorArmorTool_ = nullptr;
    HWND editorEyeTool_ = nullptr;
    HWND editorResetButton_ = nullptr;
    HWND dropEditorButton_ = nullptr;
    HWND titleLabel_ = nullptr;
    HWND speedLabel_ = nullptr;
    HWND brushLabel_ = nullptr;
    HWND gridLabel_ = nullptr;
    HWND rulesLabel_ = nullptr;
    HWND statsLabel_ = nullptr;
    HWND cellSizeText_ = nullptr;
    HWND foodProbText_ = nullptr;
    HWND lifespanText_ = nullptr;
    HWND globalMutationText_ = nullptr;
    HWND foodDropText_ = nullptr;
    HWND maxOrgText_ = nullptr;
    HWND populationLabel_ = nullptr;
    HWND speciesLabel_ = nullptr;
    HWND tickLabel_ = nullptr;
    HWND mutationLabel_ = nullptr;
    HWND largestLabel_ = nullptr;
    HWND topSpeciesLabel_ = nullptr;
    HWND editorLabel_ = nullptr;
    HWND editorHintLabel_ = nullptr;
};

int runNativeGui(int showCommand) {
    Win32GuiApp app;
    return app.run(showCommand);
}

} // namespace lifeengine::gui

#else

namespace lifeengine::gui {

int runNativeGui(int) {
    return 1;
}

} // namespace lifeengine::gui

#endif
