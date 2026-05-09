#include "GUI.h"
#include "engine/QueryExecutor.h"
#include "schema/Table.h"
#include "schema/Row.h"
#include "schema/Field.h"
#include "parser/Tokenizer.h"
#include "parser/PostfixConverter.h"
#include "parser/ExpressionEvaluator.h"
#include "memory/BufferPool.h"
#include "memory/LRUCache.h"
#include "index/AVLTree.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <chrono>

static int stringLength(const char* text) {
	if (text == 0) {
		return 0;
	}
	int len = 0;
	while (text[len] != '\0') {
		len += 1;
	}
	return len;
}

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char toUpperChar(char c) {
	if (c >= 'a' && c <= 'z') {
		return static_cast<char>(c - ('a' - 'A'));
	}
	return c;
}

static bool startsWith(const char* text, const char* prefix) {
	if (text == 0 || prefix == 0) {
		return false;
	}

	int i = 0;
	while (prefix[i] != '\0') {
		if (text[i] != prefix[i]) {
			return false;
		}
		i += 1;
	}
	return true;
}

static bool fileExists(const char* path) {
	if (path == 0 || path[0] == '\0') {
		return false;
	}
	FILE* file = std::fopen(path, "rb");
	if (file == 0) {
		return false;
	}
	std::fclose(file);
	return true;
}

static void joinPath(const char* dir, const char* file, char* outPath, int outSize) {
	if (outPath == 0 || outSize <= 0) {
		return;
	}
	if (dir == 0 || dir[0] == '\0') {
		std::snprintf(outPath, static_cast<size_t>(outSize), "%s", file != 0 ? file : "");
		return;
	}

	int len = stringLength(dir);
	bool needsSep = true;
	if (len > 0) {
		char last = dir[len - 1];
		needsSep = !(last == '/' || last == '\\');
	}

	std::snprintf(outPath, static_cast<size_t>(outSize), "%s%s%s", dir, needsSep ? "/" : "", file != 0 ? file : "");
}

static bool hasNonWhitespace(const char* text) {
	if (text == 0) {
		return false;
	}
	for (int i = 0; text[i] != '\0'; ++i) {
		if (!isWhitespace(text[i])) {
			return true;
		}
	}
	return false;
}

static bool isSelectQuery(const char* query) {
	if (query == 0) {
		return false;
	}
	int i = 0;
	while (query[i] != '\0' && isWhitespace(query[i])) {
		i += 1;
	}

	const char* keyword = "SELECT";
	int k = 0;
	while (keyword[k] != '\0') {
		if (toUpperChar(query[i + k]) != keyword[k]) {
			return false;
		}
		k += 1;
	}
	char next = query[i + k];
	return next == '\0' || isWhitespace(next);
}

GUI::GUI() {
	executor = 0;
	queryBuffer[0] = '\0';
	copyString(dataDir, static_cast<int>(sizeof(dataDir)), "data");
	setupStatus[0] = '\0';
	copyString(logPath, static_cast<int>(sizeof(logPath)), "logs/nanodb_execution.log");
	autoScroll = true;
	adminMode = false;
	dataReady = false;
	selectedTable = 0;
	editorRatio = 0.38f;
	baseFont = 0;
	monoFont = 0;

	resultRowCount = 0;
	resultColCount = 0;
	resultPage = 0;
	logLineCount = 0;
	hasBenchmark = false;

	for (int i = 0; i < MAX_RESULT_COLS; ++i) {
		resultColumns[i][0] = '\0';
	}
	for (int i = 0; i < MAX_LOG_LINES; ++i) {
		logLines[i][0] = '\0';
	}
	for (int i = 0; i < 3; ++i) {
		benchSeqMs[i] = 0.0;
		benchAvlMs[i] = 0.0;
	}
}

GUI::~GUI() {
	shutdown();
}

bool GUI::init(GLFWwindow* window, const char* glslVersion) {
	if (window == 0 || glslVersion == 0) {
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	if (fileExists("C:\\Windows\\Fonts\\SegoeUIVariable.ttf")) {
		baseFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\SegoeUIVariable.ttf", 18.0f);
	} else if (fileExists("C:\\Windows\\Fonts\\segoeui.ttf")) {
		baseFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	}
	if (fileExists("C:\\Windows\\Fonts\\CascadiaCode.ttf")) {
		monoFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\CascadiaCode.ttf", 16.0f);
	} else if (fileExists("C:\\Windows\\Fonts\\consola.ttf")) {
		monoFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f);
	}
	if (baseFont == 0) {
		baseFont = io.FontDefault;
	}
	if (monoFont == 0) {
		monoFont = baseFont;
	}
	io.FontDefault = baseFont;

	ImGui::StyleColorsLight();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 8.0f;
	style.FrameRounding = 5.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 5.0f;
	style.WindowPadding = ImVec2(14.0f, 12.0f);
	style.FramePadding = ImVec2(8.0f, 6.0f);
	style.ItemSpacing = ImVec2(10.0f, 8.0f);
	style.TabRounding = 5.0f;
	style.WindowBorderSize = 0.0f;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_WindowBg] = ImVec4(0.93f, 0.94f, 0.95f, 1.0f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.96f, 0.96f, 0.97f, 1.0f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.24f, 0.29f, 0.33f, 1.0f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.28f, 0.33f, 0.38f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.89f, 0.93f, 0.98f, 1.0f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.82f, 0.90f, 0.98f, 1.0f);
	colors[ImGuiCol_Button] = ImVec4(0.16f, 0.45f, 0.64f, 1.0f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.53f, 0.73f, 1.0f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.40f, 0.58f, 1.0f);
	colors[ImGuiCol_Tab] = ImVec4(0.78f, 0.82f, 0.86f, 1.0f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.66f, 0.74f, 0.82f, 1.0f);
	colors[ImGuiCol_TabActive] = ImVec4(0.55f, 0.68f, 0.80f, 1.0f);
	colors[ImGuiCol_Header] = ImVec4(0.78f, 0.86f, 0.93f, 1.0f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.68f, 0.82f, 0.92f, 1.0f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.58f, 0.76f, 0.90f, 1.0f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.82f, 0.85f, 0.88f, 1.0f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.72f, 0.74f, 0.78f, 1.0f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.82f, 0.84f, 0.87f, 1.0f);
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glslVersion);
	return true;
}

void GUI::shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	if (ImGui::GetCurrentContext() != 0) {
		ImGui::DestroyContext();
	}
}

void GUI::beginFrame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void GUI::endFrame() {
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::setExecutor(QueryExecutor* exec) {
	executor = exec;
}

void GUI::setLogPath(const char* path) {
	copyString(logPath, static_cast<int>(sizeof(logPath)), path);
}

void GUI::copyString(char* dest, int destSize, const char* src) const {
	if (dest == 0 || destSize <= 0) {
		return;
	}
	if (src == 0) {
		dest[0] = '\0';
		return;
	}

	int i = 0;
	for (; i < destSize - 1 && src[i] != '\0'; ++i) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
}

void GUI::trimWhitespace(char* text) const {
	if (text == 0) {
		return;
	}

	int len = stringLength(text);
	int start = 0;
	while (start < len && isWhitespace(text[start])) {
		start += 1;
	}

	int end = len - 1;
	while (end >= start && isWhitespace(text[end])) {
		end -= 1;
	}

	int out = 0;
	for (int i = start; i <= end; ++i) {
		text[out] = text[i];
		out += 1;
	}
	text[out] = '\0';
}

bool GUI::equalsIgnoreCase(const char* a, const char* b) const {
	if (a == 0 || b == 0) {
		return false;
	}

	int i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		if (toUpperChar(a[i]) != toUpperChar(b[i])) {
			return false;
		}
		i += 1;
	}
	return a[i] == '\0' && b[i] == '\0';
}

void GUI::clearResults() {
	resultRowCount = 0;
	resultColCount = 0;
	resultPage = 0;
	for (int i = 0; i < MAX_RESULT_COLS; ++i) {
		resultColumns[i][0] = '\0';
	}
}

void GUI::parseTableNames(Token* tokens, int tokenCount, char names[][64], int& count) {
	count = 0;
	if (tokens == 0) {
		return;
	}

	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}

		if (tokens[i].type == TOKEN_KEYWORD &&
			(equalsIgnoreCase(tokens[i].value, "FROM") || equalsIgnoreCase(tokens[i].value, "JOIN"))) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(names[count], 64, tokens[i + 1].value);
				count += 1;
			}
		}
	}
}

void GUI::runSelectAndCache(const char* query) {
	clearResults();
	if (executor == 0 || query == 0) {
		return;
	}

	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableNames[5][64];
	int tableCount = 0;
	parseTableNames(tokens, tokenCount, tableNames, tableCount);

	if (tableCount != 1) {
		return;
	}

	Table* table = executor->getOrLoadTable(tableNames[0]);
	if (table == 0) {
		return;
	}

	int colCount = table->getColumnCount();
	if (colCount > MAX_RESULT_COLS) {
		colCount = MAX_RESULT_COLS;
	}

	resultColCount = colCount;
	for (int i = 0; i < colCount; ++i) {
		const char* name = table->getColumnName(i);
		copyString(resultColumns[i], static_cast<int>(sizeof(resultColumns[i])), name != 0 ? name : "");
	}

	int whereIndex = -1;
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			whereIndex = i + 1;
			break;
		}
	}

	Token* exprTokens = 0;
	int exprCount = 0;
	if (whereIndex >= 0) {
		for (int i = whereIndex; i < tokenCount; ++i) {
			exprCount += 1;
			if (tokens[i].type == TOKEN_END) {
				break;
			}
		}

		exprTokens = new Token[exprCount + 1];
		int writeIndex = 0;
		for (int i = whereIndex; i < tokenCount && writeIndex < exprCount; ++i) {
			exprTokens[writeIndex] = tokens[i];
			writeIndex += 1;
			if (tokens[i].type == TOKEN_END) {
				break;
			}
		}
		exprTokens[writeIndex].type = TOKEN_END;
		exprTokens[writeIndex].value[0] = '\0';
		exprCount = writeIndex;
	}

	PostfixConverter converter;
	int postfixCount = 0;
	Token* postfix = 0;
	if (exprTokens != 0) {
		postfix = converter.convert(exprTokens, exprCount, postfixCount);
	}

	ExpressionEvaluator evaluator;
	int rowCount = table->getRowCount();
	for (int i = 0; i < rowCount && resultRowCount < MAX_RESULT_ROWS; ++i) {
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}

		bool match = true;
		if (postfix != 0) {
			match = evaluator.evaluate(postfix, postfixCount, row, table);
		}

		if (match) {
			for (int c = 0; c < colCount; ++c) {
				Field* field = row->getField(c);
				const char* value = field != 0 ? field->toString() : "";
				copyString(resultCells[resultRowCount][c], 128, value);
			}
			resultRowCount += 1;
		}
	}

	delete[] exprTokens;
}

void GUI::refreshLogLines() {
	logLineCount = 0;
	if (logPath[0] == '\0') {
		return;
	}

	FILE* file = std::fopen(logPath, "r");
	if (file == 0) {
		return;
	}

	char line[512];
	while (std::fgets(line, static_cast<int>(sizeof(line)), file) != 0) {
		if (logLineCount >= MAX_LOG_LINES) {
			break;
		}
		trimWhitespace(line);
		copyString(logLines[logLineCount], static_cast<int>(sizeof(logLines[logLineCount])), line);
		logLineCount += 1;
	}

	std::fclose(file);
}

void GUI::render() {
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	if (!ImGui::Begin("NanoDB Studio", 0, flags)) {
		ImGui::End();
		return;
	}

	renderToolbar();

	float statusHeight = 28.0f;
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float bodyHeight = avail.y - statusHeight;
	if (bodyHeight < 100.0f) {
		bodyHeight = avail.y;
	}

	ImGui::BeginChild("Body", ImVec2(0.0f, bodyHeight), false);
	float sidebarWidth = 260.0f;
	ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, 0.0f), true);
		renderSetupPanel();
		ImGui::Separator();
		renderSchemaPanel();
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("MainArea", ImVec2(0.0f, 0.0f), false);
		ImVec2 mainAvail = ImGui::GetContentRegionAvail();
		float editorHeight = mainAvail.y * editorRatio;
		if (editorHeight < 200.0f) {
			editorHeight = 200.0f;
		}
		ImGui::BeginChild("Editor", ImVec2(0.0f, editorHeight), true);
			renderQueryPanel();
		ImGui::EndChild();

		ImGui::BeginChild("Output", ImVec2(0.0f, 0.0f), true);
			if (ImGui::BeginTabBar("OutputTabs")) {
				if (ImGui::BeginTabItem("Results")) {
					renderResultsPanel();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Log")) {
					renderLogPanel();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Diagnostics")) {
					if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
						renderMemoryPanel();
					}
					if (ImGui::CollapsingHeader("Indexes")) {
						renderIndexPanel();
					}
					if (ImGui::CollapsingHeader("Benchmarks")) {
						renderBenchmarkPanel();
					}
					if (ImGui::CollapsingHeader("Query Queue")) {
						renderQueuePanel();
					}
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		ImGui::EndChild();
	ImGui::EndChild();
	ImGui::EndChild();

	renderStatusBar();
	ImGui::End();
}

void GUI::renderToolbar() {
	ImGui::BeginChild("Toolbar", ImVec2(0.0f, 42.0f), false, ImGuiWindowFlags_NoScrollbar);
	ImGui::AlignTextToFramePadding();
	ImGui::Text("NanoDB Studio");
	ImGui::SameLine(200.0f);

	if (ImGui::Button("Build DB")) {
		if (executor != 0) {
			dataReady = executor->prepareDefaultData(dataDir, setupStatus, static_cast<int>(sizeof(setupStatus)));
		} else {
			copyString(setupStatus, static_cast<int>(sizeof(setupStatus)), "Query engine not ready.");
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Run Workload")) {
		if (executor != 0) {
			executor->runWorkloadFile("tests/queries.txt");
			refreshLogLines();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Run")) {
		if (executor != 0 && hasNonWhitespace(queryBuffer)) {
			int priority = adminMode ? 0 : 1;
			executor->submitQuery(queryBuffer, priority);
			executor->processQueue();
			if (isSelectQuery(queryBuffer)) {
				runSelectAndCache(queryBuffer);
			} else {
				clearResults();
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		queryBuffer[0] = '\0';
		clearResults();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Admin", &adminMode);
	ImGui::EndChild();
	ImGui::Separator();
}

void GUI::renderSchemaPanel() {
	ImGui::Text("Schema");
	ImGui::Separator();

	const char* tables[] = { "customer", "orders", "lineitem" };
	const int tableCount = 3;

	if (ImGui::TreeNode("nanodb")) {
		if (ImGui::TreeNode("Tables")) {
			for (int i = 0; i < tableCount; ++i) {
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
				if (selectedTable == i) {
					flags |= ImGuiTreeNodeFlags_Selected;
				}
				bool open = ImGui::TreeNodeEx(tables[i], flags);
				if (ImGui::IsItemClicked()) {
					selectedTable = i;
				}

				if (open) {
					TableMeta* meta = executor != 0 ? executor->getTableMeta(tables[i]) : 0;
					if (meta != 0) {
						int limit = meta->columnCount;
						if (limit > 10) {
							limit = 10;
						}
						for (int c = 0; c < limit; ++c) {
							ImGui::BulletText("%s", meta->columnNames[c]);
						}
					} else {
						ImGui::TextDisabled("No metadata available.");
					}
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::Text("Quick actions");
	if (selectedTable >= 0 && selectedTable < tableCount) {
		if (ImGui::Button("SELECT *")) {
			char query[256];
			std::snprintf(query, static_cast<size_t>(sizeof(query)), "SELECT * FROM %s", tables[selectedTable]);
			copyString(queryBuffer, static_cast<int>(sizeof(queryBuffer)), query);
		}
		ImGui::SameLine();
		if (ImGui::Button("SELECT WHERE key")) {
			TableMeta* meta = executor != 0 ? executor->getTableMeta(tables[selectedTable]) : 0;
			const char* key = (meta != 0 && meta->columnCount > 0) ? meta->columnNames[0] : "id";
			char query[256];
			std::snprintf(query, static_cast<size_t>(sizeof(query)), "SELECT * FROM %s WHERE %s = 1", tables[selectedTable], key);
			copyString(queryBuffer, static_cast<int>(sizeof(queryBuffer)), query);
		}
	}
}

void GUI::renderStatusBar() {
	ImGui::BeginChild("StatusBar", ImVec2(0.0f, 28.0f), true, ImGuiWindowFlags_NoScrollbar);
	const char* tables[] = { "customer", "orders", "lineitem" };
	const int tableCount = 3;
	const char* tableName = (selectedTable >= 0 && selectedTable < tableCount) ? tables[selectedTable] : "-";
	const char* statusText = dataReady ? "ready" : "needs build";

	ImGui::AlignTextToFramePadding();
	ImGui::Text("DB: %s", statusText);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::Text("Table: %s", tableName);
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	ImGui::Text("Rows: %d", resultRowCount);

	BufferPool* pool = executor != 0 ? executor->getBufferPool() : 0;
	if (pool != 0) {
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::Text("Pages: %d/%d", pool->getUsedPages(), pool->getMaxPages());
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::Text("Evictions: %d", pool->getEvictionCount());
	}

	ImGui::EndChild();
}

void GUI::renderSetupPanel() {
	ImGui::Text("Database");
	ImGui::Separator();

	ImGui::InputText("Data folder", dataDir, static_cast<int>(sizeof(dataDir)));
	ImGui::Text("TPC-H files: customer.tbl, orders.tbl, lineitem.tbl");
	ImGui::Text("Binary store: data/*.bin");
	ImGui::Separator();

	const char* tables[] = { "customer", "orders", "lineitem" };
	bool allBins = true;
	for (int i = 0; i < 3; ++i) {
		char binPath[256];
		std::snprintf(binPath, static_cast<size_t>(sizeof(binPath)), "data/%s.bin", tables[i]);
		char tblFile[96];
		std::snprintf(tblFile, static_cast<size_t>(sizeof(tblFile)), "%s.tbl", tables[i]);
		char tblPath[256];
		joinPath(dataDir, tblFile, tblPath, static_cast<int>(sizeof(tblPath)));

		bool binReady = fileExists(binPath);
		bool tblReady = fileExists(tblPath);
		if (!binReady) {
			allBins = false;
		}

		ImVec4 color = binReady ? ImVec4(0.10f, 0.50f, 0.20f, 1.0f)
			: (tblReady ? ImVec4(0.65f, 0.45f, 0.10f, 1.0f) : ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
		ImGui::TextColored(color, "%s  %s", tables[i], binReady ? "[bin ready]" : (tblReady ? "[tbl ready]" : "[missing]"));
	}

	dataReady = allBins;
	ImGui::Separator();
	if (ImGui::Button("Build database from .tbl")) {
		if (executor != 0) {
			dataReady = executor->prepareDefaultData(dataDir, setupStatus, static_cast<int>(sizeof(setupStatus)));
		} else {
			copyString(setupStatus, static_cast<int>(sizeof(setupStatus)), "Query engine not ready.");
		}
	}
	ImGui::SameLine();
	ImGui::Text(dataReady ? "Status: ready" : "Status: needs build");

	if (setupStatus[0] != '\0') {
		ImGui::Text("%s", setupStatus);
	}
	ImGui::Text("Build once, then run queries.");
}

void GUI::renderQueryPanel() {
	ImGui::Text("Query Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("(use double quotes for strings)");
	ImGui::Separator();

	bool ready = fileExists("data/customer.bin") && fileExists("data/orders.bin") && fileExists("data/lineitem.bin");
	dataReady = ready;
	if (!ready) {
		ImGui::TextColored(ImVec4(0.70f, 0.20f, 0.20f, 1.0f), "Database not ready. Build it in the left panel.");
	}

	ImGui::PushFont(monoFont);
	ImVec2 editorSize = ImVec2(-1.0f, ImGui::GetContentRegionAvail().y - 70.0f);
	if (editorSize.y < 140.0f) {
		editorSize.y = 140.0f;
	}
	ImGui::InputTextMultiline("##query", queryBuffer, static_cast<int>(sizeof(queryBuffer)), editorSize);
	ImGui::PopFont();

	if (ImGui::Button("Run")) {
		if (executor != 0 && hasNonWhitespace(queryBuffer)) {
			int priority = adminMode ? 0 : 1;
			executor->submitQuery(queryBuffer, priority);
			executor->processQueue();
			if (isSelectQuery(queryBuffer)) {
				runSelectAndCache(queryBuffer);
			} else {
				clearResults();
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		queryBuffer[0] = '\0';
		clearResults();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Admin", &adminMode);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("Editor", &editorRatio, 0.25f, 0.65f, "%.2f");

	ImGui::Separator();
	ImGui::Text("Examples:");
	if (ImGui::Button("Customer by key")) {
		copyString(queryBuffer, static_cast<int>(sizeof(queryBuffer)), "SELECT * FROM customer WHERE c_custkey = 1");
	}
	ImGui::SameLine();
	if (ImGui::Button("Orders by customer")) {
		copyString(queryBuffer, static_cast<int>(sizeof(queryBuffer)), "SELECT * FROM orders WHERE o_custkey = 1");
	}
	ImGui::SameLine();
	if (ImGui::Button("Lineitems by order")) {
		copyString(queryBuffer, static_cast<int>(sizeof(queryBuffer)), "SELECT * FROM lineitem WHERE l_orderkey = 1");
	}
	ImGui::Text("Results preview supports single-table SELECT only.");
}

void GUI::renderResultsPanel() {
	ImGui::Text("Results");
	ImGui::Separator();

	if (resultColCount == 0) {
		ImGui::Text("No results yet.");
		return;
	}

	ImGui::Text("Rows: %d", resultRowCount);
	ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
	float tableHeight = ImGui::GetContentRegionAvail().y - 70.0f;
	if (tableHeight < 200.0f) {
		tableHeight = 200.0f;
	}
	if (ImGui::BeginTable("ResultsTable", resultColCount, flags, ImVec2(-1.0f, tableHeight))) {
		for (int c = 0; c < resultColCount; ++c) {
			ImGui::TableSetupColumn(resultColumns[c]);
		}
		ImGui::TableHeadersRow();

		int startRow = resultPage * 50;
		int endRow = startRow + 50;
		if (endRow > resultRowCount) {
			endRow = resultRowCount;
		}

		ImU32 evenColor = ImGui::GetColorU32(ImVec4(0.98f, 0.97f, 0.95f, 1.0f));
		ImU32 oddColor = ImGui::GetColorU32(ImVec4(0.94f, 0.93f, 0.91f, 1.0f));

		for (int r = startRow; r < endRow; ++r) {
			ImGui::TableNextRow();
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, (r % 2 == 0) ? evenColor : oddColor);
			for (int c = 0; c < resultColCount; ++c) {
				ImGui::TableSetColumnIndex(c);
				ImGui::TextUnformatted(resultCells[r][c]);
			}
		}

		ImGui::EndTable();
	}

	ImGui::Separator();
	if (ImGui::Button("Prev") && resultPage > 0) {
		resultPage -= 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("Next")) {
		int maxPages = (resultRowCount + 49) / 50;
		if (resultPage + 1 < maxPages) {
			resultPage += 1;
		}
	}
}

void GUI::renderLogPanel() {
	ImGui::Text("Activity Log");
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScroll);
	refreshLogLines();
	float logHeight = ImGui::GetContentRegionAvail().y;
	if (logHeight < 120.0f) {
		logHeight = 120.0f;
	}
	ImGui::BeginChild("LogScroll", ImVec2(0, logHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
	for (int i = 0; i < logLineCount; ++i) {
		const char* line = logLines[i];
		ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		if (startsWith(line, "[BENCHMARK]")) {
			color = ImVec4(1.0f, 0.86f, 0.35f, 1.0f);
		} else if (startsWith(line, "[ERROR]")) {
			color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
		} else if (startsWith(line, "[EVICT]")) {
			color = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
		} else if (startsWith(line, "[LOG]")) {
			color = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
		}

		ImGui::TextColored(color, "%s", line);
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}

void GUI::renderMemoryPanel() {
	BufferPool* pool = executor != 0 ? executor->getBufferPool() : 0;
	if (pool == 0) {
		ImGui::Text("BufferPool not available.");
		return;
	}

	int totalPages = pool->getMaxPages();
	int usedPages = pool->getUsedPages();
	int faults = pool->getPageFaultCount();
	int evictions = pool->getEvictionCount();
	int lastEvicted = pool->getLastEvictedPageId();

	ImGui::Text("Total pages: %d", totalPages);
	ImGui::Text("Used pages: %d", usedPages);
	float progress = totalPages > 0 ? static_cast<float>(usedPages) / static_cast<float>(totalPages) : 0.0f;
	ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
	ImGui::Text("Page fault count: %d", faults);
	ImGui::Text("Eviction count: %d", evictions);

	ImGui::Separator();
	ImGui::Text("LRU Cache");

	LRUCache* lru = pool->getLRUCache();
	if (lru != 0) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 start = ImGui::GetCursorScreenPos();
		float boxWidth = 50.0f;
		float boxHeight = 30.0f;
		float spacing = 8.0f;

		DLLNode* node = lru->getHead();
		int index = 0;
		while (node != 0) {
			ImVec2 boxMin = ImVec2(start.x + index * (boxWidth + spacing), start.y);
			ImVec2 boxMax = ImVec2(boxMin.x + boxWidth, boxMin.y + boxHeight);
			ImU32 color = (node->pageId == lastEvicted && lastEvicted >= 0)
				? ImGui::GetColorU32(ImVec4(0.9f, 0.2f, 0.2f, 1.0f))
				: ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
			drawList->AddRectFilled(boxMin, boxMax, color, 4.0f);
			drawList->AddRect(boxMin, boxMax, ImGui::GetColorU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f)));

			char label[16];
			std::snprintf(label, static_cast<size_t>(sizeof(label)), "%d", node->pageId);
			ImVec2 textSize = ImGui::CalcTextSize(label);
			ImVec2 textPos = ImVec2(boxMin.x + (boxWidth - textSize.x) * 0.5f,
									 boxMin.y + (boxHeight - textSize.y) * 0.5f);
			drawList->AddText(textPos, ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), label);

			node = node->next;
			index += 1;
		}

		ImGui::Dummy(ImVec2(index * (boxWidth + spacing), boxHeight + 4.0f));
	}
}

void GUI::renderIndexPanel() {
	if (ImGui::BeginTabBar("IndexTabs")) {
		const char* tables[] = { "customer", "orders", "lineitem" };
		for (int t = 0; t < 3; ++t) {
			if (ImGui::BeginTabItem(tables[t])) {
				AVLTree* index = executor != 0 ? executor->getIndex(tables[t]) : 0;
				Table* table = executor != 0 ? executor->getOrLoadTable(tables[t]) : 0;

				int height = index != 0 ? index->getHeight() : 0;
				int nodes = index != 0 ? index->getNodeCount() : 0;
				int comparisons = index != 0 ? index->getLastSearchComparisons() : 0;

				ImGui::Text("Tree height: %d", height);
				ImGui::Text("Node count: %d", nodes);
				ImGui::Text("Last search: %d comparisons", comparisons);

				if (hasBenchmark) {
					ImGui::Text("Last search time: %.3fms vs sequential %.3fms", benchAvlMs[0], benchSeqMs[0]);
				} else {
					ImGui::Text("Last search time: n/a");
				}

				if (ImGui::Button("Rebuild Index")) {
					if (index != 0 && table != 0) {
						index->buildIndex(table, 0);
					}
				}
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
}

void GUI::renderBenchmarkPanel() {
	if (ImGui::Button("Run benchmark")) {
		Table* table = executor != 0 ? executor->getOrLoadTable("customer") : 0;
		AVLTree* tree = executor != 0 ? executor->getIndex("customer") : 0;

		if (table != 0 && tree != 0) {
			const int targets[3] = { 1000, 10000, 100000 };
			int totalRows = table->getRowCount();

			for (int i = 0; i < 3; ++i) {
				int limit = targets[i];
				if (limit > totalRows) {
					limit = totalRows;
				}

				if (limit <= 0) {
					benchSeqMs[i] = 0.0;
					benchAvlMs[i] = 0.0;
					continue;
				}

				Row* sampleRow = table->getRow(limit / 2);
				int key = 0;
				if (sampleRow != 0) {
					Field* field = sampleRow->getField(0);
					if (field != 0) {
						key = static_cast<int>(field->toDouble());
					}
				}

				using Clock = std::chrono::high_resolution_clock;
				auto seqStart = Clock::now();
				for (int r = 0; r < limit; ++r) {
					Row* row = table->getRow(r);
					if (row == 0) {
						continue;
					}
					Field* field = row->getField(0);
					if (field != 0 && static_cast<int>(field->toDouble()) == key) {
						break;
					}
				}
				auto seqEnd = Clock::now();

				auto avlStart = Clock::now();
				tree->search(key);
				auto avlEnd = Clock::now();

				benchSeqMs[i] = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count() / 1000.0;
				benchAvlMs[i] = std::chrono::duration_cast<std::chrono::microseconds>(avlEnd - avlStart).count() / 1000.0;
			}

			hasBenchmark = true;
		}
	}

	ImGui::Separator();
	if (!hasBenchmark) {
		ImGui::Text("Run the benchmark to compare sequential scan vs AVL index.");
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 start = ImGui::GetCursorScreenPos();
	float chartWidth = ImGui::GetContentRegionAvail().x;
	float chartHeight = 200.0f;
	float barWidth = chartWidth / 6.0f;
	float maxVal = 1.0f;

	for (int i = 0; i < 3; ++i) {
		if (benchSeqMs[i] > maxVal) {
			maxVal = static_cast<float>(benchSeqMs[i]);
		}
		if (benchAvlMs[i] > maxVal) {
			maxVal = static_cast<float>(benchAvlMs[i]);
		}
	}

	if (maxVal < 1.0f) {
		maxVal = 1.0f;
	}

	const char* labels[3] = { "1K", "10K", "100K" };
	for (int i = 0; i < 3; ++i) {
		float seqHeight = static_cast<float>(benchSeqMs[i]) / maxVal * chartHeight;
		float avlHeight = static_cast<float>(benchAvlMs[i]) / maxVal * chartHeight;

		ImVec2 seqMin = ImVec2(start.x + i * barWidth * 2.0f, start.y + chartHeight - seqHeight);
		ImVec2 seqMax = ImVec2(seqMin.x + barWidth, start.y + chartHeight);
		ImVec2 avlMin = ImVec2(seqMin.x + barWidth + 4.0f, start.y + chartHeight - avlHeight);
		ImVec2 avlMax = ImVec2(avlMin.x + barWidth, start.y + chartHeight);

		drawList->AddRectFilled(seqMin, seqMax, ImGui::GetColorU32(ImVec4(0.7f, 0.3f, 0.3f, 1.0f)));
		drawList->AddRectFilled(avlMin, avlMax, ImGui::GetColorU32(ImVec4(0.3f, 0.7f, 0.4f, 1.0f)));
		drawList->AddText(ImVec2(seqMin.x, start.y + chartHeight + 4.0f), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), labels[i]);
	}

	ImGui::Dummy(ImVec2(chartWidth, chartHeight + 20.0f));
}

void GUI::renderQueuePanel() {
	if (ImGui::Button("Process All")) {
		if (executor != 0) {
			executor->processQueue();
		}
	}

	PriorityQueue* queue = executor != 0 ? executor->getQueue() : 0;
	if (queue == 0) {
		ImGui::Text("Queue not available.");
		return;
	}

	int size = queue->getSize();
	const QueryItem* items = queue->getItems();
	for (int i = 0; i < size; ++i) {
		const QueryItem& item = items[i];
		ImVec4 color = item.priority == 0 ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

		char preview[80];
		copyString(preview, static_cast<int>(sizeof(preview)), item.query);
		preview[static_cast<int>(sizeof(preview)) - 1] = '\0';

		ImGui::TextColored(color, "%s %s", item.priority == 0 ? "[ADMIN]" : "[USER]", preview);
	}
}
