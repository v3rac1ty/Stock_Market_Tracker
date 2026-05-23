#include "stock_tracker.hpp"
#include "stock_data.hpp"
#include "ring_buffer.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------------------
// Global application state
// ---------------------------------------------------------------------------

static StockTracker             g_tracker;
static RingBuffer<StockInfo, 8> g_ring;

static StockInfo    g_display;
static bool         g_loading  = false;
static bool         g_hasData  = false;

static int  g_rangeIdx          = 2;
static char g_symbolBuf[32]     = "AAPL";
static std::string g_selected;

static bool g_showSMA20     = true;
static bool g_showSMA50     = true;
static bool g_showEMA12     = false;
static bool g_showEMA26     = false;
static bool g_showBollinger = false;
static bool g_showRSI       = false;

// ---------------------------------------------------------------------------
// Fetch helper
// ---------------------------------------------------------------------------

static void triggerFetch(const std::string& symbol, int rangeIdx) {
    g_loading = true;
    g_hasData = false;
    g_tracker.fetchAsync(symbol, rangeIdx, [](StockInfo info) {
        g_ring.push(std::move(info));
    });
}

// ---------------------------------------------------------------------------
// Candlestick chart
// ---------------------------------------------------------------------------

static void DrawCandlestickChart(const StockInfo& info, float chartHeight) {
    const auto& candles = info.candles;
    if (candles.empty()) return;

    double tFirst = candles.front().timestamp;
    double tLast  = candles.back().timestamp;
    double tSpan  = tLast - tFirst;
    double barW   = (tSpan / std::max<int>((int)candles.size(), 1)) * 0.65;

    double yMin = candles[0].low;
    double yMax = candles[0].high;
    for (const auto& c : candles) {
        yMin = std::min(yMin, c.low);
        yMax = std::max(yMax, c.high);
    }
    double pad = (yMax - yMin) * 0.05;
    yMin -= pad;
    yMax += pad;

    ImPlotFlags plotFlags = ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText;
    if (!ImPlot::BeginPlot("##chart", {-1, chartHeight}, plotFlags))
        return;

    ImPlot::SetupAxis(ImAxis_X1, nullptr,
        ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks);
    ImPlot::SetupAxis(ImAxis_Y1, "Price (USD)", ImPlotAxisFlags_None);
    ImPlot::SetupAxisLimits(ImAxis_X1, tFirst - barW, tLast + barW, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, yMin, yMax, ImGuiCond_Always);
    ImPlot::SetupAxisFormat(ImAxis_X1, [](double val, char* buf, int sz, void*) -> int {
        time_t t = (time_t)val;
        struct tm* tm_ptr = localtime(&t);
        return (int)strftime(buf, (size_t)sz, "%m/%d", tm_ptr);
    }, nullptr);

    // Custom candlestick drawing
    ImPlot::PushPlotClipRect();
    ImDrawList* dl = ImPlot::GetPlotDrawList();
    for (const auto& c : candles) {
        bool  bull   = c.close >= c.open;
        ImU32 col    = bull ? IM_COL32(0, 197, 105, 255) : IM_COL32(220, 50, 50, 255);
        ImU32 colDim = bull ? IM_COL32(0, 140,  75, 255) : IM_COL32(160, 30, 30, 255);

        ImVec2 bodyL = ImPlot::PlotToPixels(c.timestamp - barW / 2.0,
                                             std::max(c.open, c.close));
        ImVec2 bodyR = ImPlot::PlotToPixels(c.timestamp + barW / 2.0,
                                             std::min(c.open, c.close));
        ImVec2 wickT = ImPlot::PlotToPixels(c.timestamp, c.high);
        ImVec2 wickB = ImPlot::PlotToPixels(c.timestamp, c.low);
        float  midX  = (bodyL.x + bodyR.x) * 0.5f;

        if (std::abs(bodyR.y - bodyL.y) < 1.0f)
            bodyR.y = bodyL.y + 1.0f;

        dl->AddRectFilled(bodyL, bodyR, col);
        dl->AddRect      (bodyL, bodyR, colDim);
        dl->AddLine({midX, wickT.y}, {midX, bodyL.y}, col, 1.5f);
        dl->AddLine({midX, bodyR.y}, {midX, wickB.y}, col, 1.5f);
    }
    ImPlot::PopPlotClipRect();

    // Indicator overlays
    std::vector<double> xs(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) xs[i] = candles[i].timestamp;

    auto plotIndicator = [&](const char* label, const std::vector<double>& ys, ImVec4 color) {
        int start = 0;
        while (start < (int)ys.size() && std::isnan(ys[start])) ++start;
        if (start >= (int)ys.size()) return;
        ImPlot::SetNextLineStyle(color, 1.5f);
        ImPlot::PlotLine(label, xs.data() + start, ys.data() + start,
                         (int)ys.size() - start);
    };

    if (g_showSMA20)
        plotIndicator("SMA20", StockTracker::computeSMA(candles, 20),
                      {1.0f, 0.8f, 0.0f, 1.0f});
    if (g_showSMA50)
        plotIndicator("SMA50", StockTracker::computeSMA(candles, 50),
                      {0.4f, 0.6f, 1.0f, 1.0f});
    if (g_showEMA12)
        plotIndicator("EMA12", StockTracker::computeEMA(candles, 12),
                      {1.0f, 0.4f, 0.0f, 1.0f});
    if (g_showEMA26)
        plotIndicator("EMA26", StockTracker::computeEMA(candles, 26),
                      {0.8f, 0.2f, 0.8f, 1.0f});
    if (g_showBollinger) {
        auto bb = StockTracker::computeBollinger(candles);
        plotIndicator("BB Upper", bb.upper,  {0.6f, 0.9f, 0.9f, 0.7f});
        plotIndicator("BB Mid",   bb.middle, {0.9f, 0.9f, 0.6f, 0.7f});
        plotIndicator("BB Lower", bb.lower,  {0.6f, 0.9f, 0.9f, 0.7f});
    }

    ImPlot::EndPlot();
}

// ---------------------------------------------------------------------------
// RSI sub-chart
// ---------------------------------------------------------------------------

static void DrawRSIChart(const StockInfo& info, float height) {
    const auto& candles = info.candles;
    if (candles.empty()) return;
    auto rsi = StockTracker::computeRSI(candles);

    std::vector<double> xs(candles.size());
    for (size_t i = 0; i < candles.size(); ++i) xs[i] = candles[i].timestamp;

    if (!ImPlot::BeginPlot("##rsi", {-1, height},
                           ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText))
        return;

    ImPlot::SetupAxes("Date", "RSI");
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImGuiCond_Always);

    ImPlot::PushPlotClipRect();
    ImDrawList* dl = ImPlot::GetPlotDrawList();
    {
        double xMin = candles.front().timestamp;
        double xMax = candles.back().timestamp;
        ImVec2 ob1 = ImPlot::PlotToPixels(xMin, 70);
        ImVec2 ob2 = ImPlot::PlotToPixels(xMax, 100);
        ImVec2 os1 = ImPlot::PlotToPixels(xMin, 0);
        ImVec2 os2 = ImPlot::PlotToPixels(xMax, 30);
        dl->AddRectFilled(ob1, ob2, IM_COL32(220, 50,   50,  40));
        dl->AddRectFilled(os1, os2, IM_COL32(  0, 197, 105,  40));
    }
    ImPlot::PopPlotClipRect();

    int start = 0;
    while (start < (int)rsi.size() && std::isnan(rsi[start])) ++start;
    if (start < (int)rsi.size()) {
        ImPlot::SetNextLineStyle({0.8f, 0.6f, 1.0f, 1.0f}, 1.5f);
        ImPlot::PlotLine("RSI", xs.data() + start, rsi.data() + start,
                         (int)rsi.size() - start);
    }

    ImPlot::EndPlot();
}

// ---------------------------------------------------------------------------
// Per-frame render
// ---------------------------------------------------------------------------

static void renderFrame(ImGuiIO& io) {
    while (auto item = g_ring.pop()) {
        g_display = std::move(*item);
        g_loading = false;
        g_hasData = true;
    }

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoTitleBar      |
        ImGuiWindowFlags_NoResize        |
        ImGuiWindowFlags_NoScrollbar     |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ---- Left sidebar -------------------------------------------------------
    ImGui::BeginChild("##sidebar", {260.0f, 0.0f}, true);
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Watchlist");
    ImGui::Separator();

    for (const auto& sym : g_tracker.watchlist()) {
        bool selected = (sym == g_selected);
        ImVec4 col = {0.8f, 0.8f, 0.8f, 1.0f};
        std::string label = sym;
        if (sym == g_selected && g_hasData) {
            float pct = (float)g_display.changePercent;
            col = pct >= 0.0f ? ImVec4{0.2f, 0.9f, 0.4f, 1.0f}
                              : ImVec4{0.9f, 0.3f, 0.3f, 1.0f};
            char buf[64];
            snprintf(buf, sizeof(buf), "%s  %+.2f%%", sym.c_str(), pct);
            label = buf;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        if (ImGui::Selectable(label.c_str(), selected)) {
            g_selected = sym;
            triggerFetch(sym, g_rangeIdx);
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##sym", g_symbolBuf, sizeof(g_symbolBuf));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string s(g_symbolBuf);
        if (!s.empty()) {
            g_tracker.addSymbol(s);
            g_selected = s;
            triggerFetch(s, g_rangeIdx);
        }
    }
    if (!g_selected.empty()) {
        if (ImGui::Button("Remove selected")) {
            g_tracker.removeSymbol(g_selected);
            if (!g_tracker.watchlist().empty()) {
                g_selected = g_tracker.watchlist().front();
                triggerFetch(g_selected, g_rangeIdx);
            } else {
                g_selected.clear();
                g_hasData = false;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Main content area --------------------------------------------------
    ImGui::BeginChild("##content", {0.0f, 0.0f}, false);

    // Time range buttons
    for (int i = 0; i < NUM_TIME_RANGES; ++i) {
        if (i > 0) ImGui::SameLine();
        bool active = (i == g_rangeIdx);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.5f, 0.9f, 1.0f});
        if (ImGui::Button(TIME_RANGES[i].label)) {
            g_rangeIdx = i;
            if (!g_selected.empty()) triggerFetch(g_selected, g_rangeIdx);
        }
        if (active)
            ImGui::PopStyleColor();
    }

    // Indicator checkboxes
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("SMA20",     &g_showSMA20);     ImGui::SameLine();
    ImGui::Checkbox("SMA50",     &g_showSMA50);     ImGui::SameLine();
    ImGui::Checkbox("EMA12",     &g_showEMA12);     ImGui::SameLine();
    ImGui::Checkbox("EMA26",     &g_showEMA26);     ImGui::SameLine();
    ImGui::Checkbox("Bollinger", &g_showBollinger); ImGui::SameLine();
    ImGui::Checkbox("RSI",       &g_showRSI);

    ImGui::Separator();

    if (g_selected.empty()) {
        ImGui::TextDisabled("Select a symbol from the watchlist.");
    } else if (g_loading) {
        ImGui::TextDisabled("Loading %s ...", g_selected.c_str());
    } else if (!g_hasData) {
        ImGui::TextColored({0.9f, 0.4f, 0.4f, 1.0f}, "No data.");
    } else {
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "%s",
                           g_display.symbol.c_str());
        ImGui::SameLine();
        ImVec4 pCol = g_display.changePercent >= 0.0
                      ? ImVec4{0.2f, 0.9f, 0.4f, 1.0f}
                      : ImVec4{0.9f, 0.3f, 0.3f, 1.0f};
        ImGui::TextColored(pCol, "  $%.2f  %+.2f (%.2f%%)",
                           g_display.currentPrice,
                           g_display.change,
                           g_display.changePercent);

        float avail       = ImGui::GetContentRegionAvail().y;
        float rsiHeight   = g_showRSI ? 160.0f : 0.0f;
        float chartHeight = avail - rsiHeight - (g_showRSI ? 6.0f : 0.0f);

        DrawCandlestickChart(g_display, chartHeight);
        if (g_showRSI)
            DrawRSIChart(g_display, rsiHeight);
    }

    ImGui::EndChild();
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1400, 900, "Stock Market Tracker", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().FramePadding = {4, 3};
    ImGui::GetStyle().ItemSpacing  = {6, 4};

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Populate initial watchlist and trigger first fetch
    for (const char* s : {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"})
        g_tracker.addSymbol(s);
    g_selected = "AAPL";
    triggerFetch("AAPL", g_rangeIdx);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderFrame(io);

        ImGui::Render();

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
