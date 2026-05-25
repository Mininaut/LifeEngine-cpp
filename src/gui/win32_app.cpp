#include "lifeengine/gui/application.hpp"

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

namespace lifeengine::gui {
namespace {

constexpr int kTimerId = 1;
constexpr int kTimerMs = 16;
constexpr int kPanelWidth = 330;
constexpr int kMargin = 10;

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
};

COLORREF colorRef(std::uint32_t rgb) {
    return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
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

class Win32GuiApp {
public:
    Win32GuiApp() : model_(160, 100, 5, 1) {
        lastTick_ = std::chrono::steady_clock::now();
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
        if (canvasBrush_ != nullptr) {
            DeleteObject(canvasBrush_);
        }
        if (eyeSlitBrush_ != nullptr) {
            DeleteObject(eyeSlitBrush_);
        }
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
            WS_OVERLAPPEDWINDOW,
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
        case WM_HSCROLL:
            onScroll(reinterpret_cast<HWND>(lParam));
            return 0;
        case WM_LBUTTONDOWN:
            onPointer(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false, true);
            return 0;
        case WM_RBUTTONDOWN:
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
        case WM_MBUTTONUP:
            mouseDown_ = false;
            panning_ = false;
            ReleaseCapture();
            return 0;
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

        speedSlider_ = trackbar(IdSpeedSlider, 1, 300, 60);
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

        CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdFoodTool);
    }

    HWND button(const wchar_t* text, int id) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND radio(const wchar_t* text, int id) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
        setFont(control);
        return control;
    }

    HWND checkbox(const wchar_t* text, int id, bool checked) {
        HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 1, 1, hwnd_, reinterpret_cast<HMENU>(id), nullptr, nullptr);
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

    void layoutControls() {
        RECT client{};
        GetClientRect(hwnd_, &client);
        canvasRect_ = client;
        canvasRect_.right = std::max(canvasRect_.left, client.right - kPanelWidth);
        panelRect_ = client;
        panelRect_.left = canvasRect_.right;

        int x = panelRect_.left + kMargin;
        int y = kMargin;
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
        updateStats();
        if (model_.settings().renderEnabled) {
            InvalidateRect(hwnd_, &canvasRect_, FALSE);
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
            model_.setTool(ToolMode::Food);
            break;
        case IdWallTool:
            model_.setTool(ToolMode::Wall);
            break;
        case IdKillTool:
            model_.setTool(ToolMode::Kill);
            break;
        case IdInspectTool:
            model_.setTool(ToolMode::Inspect);
            break;
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
        default:
            break;
        }
    }

    void onPointer(int x, int y, bool secondary, bool begin) {
        if (!pointInCanvas(x, y)) {
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
            SetCapture(hwnd_);
        }
        auto [col, row] = screenToGrid(x, y);
        model_.applyToolAt(col, row, secondary);
        updateStats();
        InvalidateRect(hwnd_, &canvasRect_, FALSE);
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
        InvalidateRect(hwnd_, &canvasRect_, FALSE);
    }

    void onMouseWheel(int screenX, int screenY, int delta) {
        POINT point{screenX, screenY};
        ScreenToClient(hwnd_, &point);
        if (!pointInCanvas(point.x, point.y)) {
            return;
        }
        double factor = delta > 0 ? 1.2 : (1.0 / 1.2);
        viewport_.zoomAt(point.x - canvasRect_.left, point.y - canvasRect_.top, factor);
        InvalidateRect(hwnd_, &canvasRect_, FALSE);
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
            CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdFoodTool);
            model_.setTool(ToolMode::Food);
            break;
        case 'D':
            CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdWallTool);
            model_.setTool(ToolMode::Wall);
            break;
        case 'G':
            CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdKillTool);
            model_.setTool(ToolMode::Kill);
            break;
        case 'S':
            CheckRadioButton(hwnd_, IdFoodTool, IdInspectTool, IdInspectTool);
            model_.setTool(ToolMode::Inspect);
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
        return model_.settings().targetFps >= 1000 ? L"MAX" : std::to_wstring(model_.settings().targetFps);
    }

    void refreshControlsFromModel() {
        setText(speedLabel_, L"Speed: " + speedText());
        setText(brushLabel_, L"Brush: " + std::to_wstring(model_.settings().brushSize));
        updateStats();
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
        FillRect(hdc, &panelRect_, panelBrush_);
        paintCanvasBuffered(hdc);
        EndPaint(hwnd_, &ps);
    }

    void paintCanvasBuffered(HDC targetDc) {
        int width = canvasWidth();
        int height = canvasHeight();
        HDC memoryDc = CreateCompatibleDC(targetDc);
        HBITMAP bitmap = CreateCompatibleBitmap(targetDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

        RECT localCanvas{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        FillRect(memoryDc, &localCanvas, canvasBrush_);
        SetViewportOrgEx(memoryDc, -canvasRect_.left, -canvasRect_.top, nullptr);
        if (model_.settings().renderEnabled) {
            paintGrid(memoryDc);
        } else {
            SetBkMode(memoryDc, TRANSPARENT);
            SetTextColor(memoryDc, RGB(230, 230, 230));
            DrawTextW(memoryDc, L"Rendering disabled", -1, &canvasRect_, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SetViewportOrgEx(memoryDc, 0, 0, nullptr);
        BitBlt(targetDc, canvasRect_.left, canvasRect_.top, width, height, memoryDc, 0, 0, SRCCOPY);

        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
    }

    void paintGrid(HDC hdc) {
        const WorldEnvironment& world = model_.world();
        int size = static_cast<int>(std::round(scaledCellSize()));
        for (int col = 0; col < world.gridMap.cols; ++col) {
            ScreenPoint point = viewport_.gridToScreen(col, 0, model_.settings().cellSize);
            int x = static_cast<int>(std::round(canvasRect_.left + point.x));
            if (x >= canvasRect_.right) {
                break;
            }
            if (x + size < canvasRect_.left) {
                continue;
            }
            for (int row = 0; row < world.gridMap.rows; ++row) {
                ScreenPoint rowPoint = viewport_.gridToScreen(col, row, model_.settings().cellSize);
                int y = static_cast<int>(std::round(canvasRect_.top + rowPoint.y));
                if (y >= canvasRect_.bottom) {
                    break;
                }
                if (y + size < canvasRect_.top) {
                    continue;
                }
                const GridCell* cell = world.gridMap.cellAt(col, row);
                RECT rect{
                    static_cast<LONG>(x),
                    static_cast<LONG>(y),
                    static_cast<LONG>(std::min(x + size, static_cast<int>(canvasRect_.right))),
                    static_cast<LONG>(std::min(y + size, static_cast<int>(canvasRect_.bottom)))};
                FillRect(hdc, &rect, cellBrushes_[stateIndex(cell->state)]);
                if (cell->state == CellState::Eye && cell->cellOwner != nullptr && size > 2) {
                    paintEyeSlit(hdc, rect, cell->cellOwner->absoluteDirection());
                }
            }
        }
    }

    void paintEyeSlit(HDC hdc, const RECT& cellRect, Direction direction) {
        int width = cellRect.right - cellRect.left;
        int height = cellRect.bottom - cellRect.top;
        int thin = std::max(1, width / 4);
        int longSide = std::max(1, (height * 3) / 4);
        RECT slit = cellRect;
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
        FillRect(hdc, &slit, eyeSlitBrush_);
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
        panelBrush_ = CreateSolidBrush(RGB(225, 227, 236));
        canvasBrush_ = CreateSolidBrush(colorRef(palette.empty));
    }

    SimulationGuiModel model_;
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    RECT canvasRect_{};
    RECT panelRect_{};
    bool mouseDown_ = false;
    bool panning_ = false;
    int lastPanX_ = 0;
    int lastPanY_ = 0;
    Viewport viewport_;
    std::chrono::steady_clock::time_point lastTick_;

    std::array<HBRUSH, CellStateCount> cellBrushes_{};
    HBRUSH eyeSlitBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HBRUSH canvasBrush_ = nullptr;

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
