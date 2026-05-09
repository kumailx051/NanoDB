#include "src/memory/BufferPool.h"
#include "src/schema/Table.h"
#include "src/catalog/SystemCatalog.h"
#include "src/parser/PostfixConverter.h"
#include "src/parser/PriorityQueue.h"
#include "src/index/AVLTree.h"
#include "src/optimizer/MSTOptimizer.h"
#include "src/engine/QueryExecutor.h"
#include "src/engine/FileManager.h"
#include "src/gui/GUI.h"

#include <cstdio>
#include <GLFW/glfw3.h>

static bool fileExists(const char* path) {
    if (path == 0) {
        return false;
    }
    FILE* file = std::fopen(path, "rb");
    if (file == 0) {
        return false;
    }
    std::fclose(file);
    return true;
}

static void buildColumnsFromMeta(Table* table, const TableMeta* meta) {
    if (table == 0 || meta == 0) {
        return;
    }

    for (int i = 0; i < meta->columnCount && i < 10; ++i) {
        table->addColumn(meta->columnNames[i]);
    }
}

static void loadTableData(FileManager* fm, SystemCatalog* cat, Table* table,
                          const char* tableName, const char* tblPath) {
    if (fm == 0 || cat == 0 || table == 0 || tableName == 0 || tblPath == 0) {
        return;
    }

    TableMeta* meta = cat->lookup(tableName);
    if (meta == 0) {
        return;
    }

    buildColumnsFromMeta(table, meta);

    if (fileExists(meta->filePath)) {
        fm->loadTable(table, meta->filePath);
    } else if (fileExists(tblPath)) {
        fm->loadTBLFile(tblPath, table);
        fm->saveTable(table, meta->filePath);
    }
}

int main() {
    std::printf("[NanoDB] Starting engine...\n");

    if (!glfwInit()) {
        std::printf("[NanoDB] Failed to initialize GLFW.\n");
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1400, 900, "NanoDB Query Engine", 0, 0);
    if (window == 0) {
        std::printf("[NanoDB] Failed to create window.\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    FileManager* fm = new FileManager();
    BufferPool* bp = new BufferPool(fm);
    SystemCatalog* cat = new SystemCatalog();

    Table* customer = new Table("customer", 25000);
    Table* orders = new Table("orders", 35000);
    Table* lineitem = new Table("lineitem", 55000);

    loadTableData(fm, cat, customer, "customer", "data/customer.tbl");
    loadTableData(fm, cat, orders, "orders", "data/orders.tbl");
    loadTableData(fm, cat, lineitem, "lineitem", "data/lineitem.tbl");

    AVLTree* custIndex = new AVLTree();
    custIndex->buildIndex(customer, 0);

    QueryExecutor* executor = new QueryExecutor(bp, cat);
    executor->initLog("logs/nanodb_execution.log");

    GUI* gui = new GUI();
    gui->setExecutor(executor);
    gui->setLogPath("logs/nanodb_execution.log");
    if (!gui->init(window, glslVersion)) {
        std::printf("[NanoDB] Failed to initialize GUI.\n");
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        gui->beginFrame();
        gui->render();

        glClearColor(0.93f, 0.94f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        gui->endFrame();

        glfwSwapBuffers(window);
    }

    bp->flushAll();
    executor->closeLog();

    delete gui;
    delete executor;
    delete custIndex;
    delete cat;
    delete bp;
    delete customer;
    delete orders;
    delete lineitem;
    delete fm;

    glfwDestroyWindow(window);
    glfwTerminate();

    std::printf("[NanoDB] Shutdown complete.\n");
    return 0;
}
