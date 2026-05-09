#pragma once

struct GLFWwindow;
struct Token;
class QueryExecutor;
struct ImFont;

class GUI {
	static const int MAX_RESULT_ROWS = 1000;
	static const int MAX_RESULT_COLS = 16;
	static const int MAX_LOG_LINES = 2048;

	QueryExecutor* executor;
	char queryBuffer[2048];
	char dataDir[256];
	char setupStatus[512];
	char logPath[256];
	bool autoScroll;
	bool adminMode;
	bool dataReady;
	int selectedTable;
	float editorRatio;
	ImFont* baseFont;
	ImFont* monoFont;

	int resultRowCount;
	int resultColCount;
	int resultPage;
	char resultColumns[MAX_RESULT_COLS][64];
	char resultCells[MAX_RESULT_ROWS][MAX_RESULT_COLS][128];

	int logLineCount;
	char logLines[MAX_LOG_LINES][512];

	double benchSeqMs[3];
	double benchAvlMs[3];
	bool hasBenchmark;

	void renderToolbar();
	void renderSchemaPanel();
	void renderSetupPanel();
	void renderQueryPanel();
	void renderResultsPanel();
	void renderLogPanel();
	void renderMemoryPanel();
	void renderIndexPanel();
	void renderBenchmarkPanel();
	void renderQueuePanel();
	void renderStatusBar();

	void refreshLogLines();
	void clearResults();
	void runSelectAndCache(const char* query);
	void parseTableNames(struct Token* tokens, int tokenCount, char names[][64], int& count);
	void copyString(char* dest, int destSize, const char* src) const;
	void trimWhitespace(char* text) const;
	bool equalsIgnoreCase(const char* a, const char* b) const;

public:
	GUI();
	~GUI();

	bool init(GLFWwindow* window, const char* glslVersion);
	void shutdown();
	void beginFrame();
	void endFrame();
	void render();

	void setExecutor(QueryExecutor* exec);
	void setLogPath(const char* path);
};
