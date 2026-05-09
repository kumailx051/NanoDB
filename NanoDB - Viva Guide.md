# NanoDB Viva Guide (Full Detail)

This guide is a complete, end-to-end explanation of the NanoDB project for the live demo and viva. It includes build steps, dataset preparation, demo scripts, detailed architecture notes, complexity analysis, and common viva questions with model answers.

---

## 0) Purpose and Constraints
Your viva is not just about running code. You must defend how every component works, how it meets the rubric, and how you verified results.

Key constraints:
- No STL containers or algorithms: no std::vector, std::map, std::list, std::stack, std::queue, std::set, std::sort, etc.
- Raw pointers + custom data structures only.
- Memory and data persistence must be manual and explicit.

---

## 1) Quick Facts
- Language: C++17
- Build system: CMake
- GUI: ImGui + GLFW + OpenGL
- Dataset: TPC-H (customer, orders, lineitem) at ~100K total rows
- Output log: logs/nanodb_execution.log
- Workload file: NanoDB/tests/queries.txt (50 commands)

---

## 2) Environment Setup (Windows + MSYS2)

### Tools required
- MSYS2 with MinGW-w64 toolchain
- GLFW + OpenGL
- CMake

If MSYS2 is installed in C:\msys64, build with:

```powershell
Set-Location "C:\Users\mkr20\OneDrive\Desktop\NanoDB AP Project\NanoDB"
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build build
```

Run the GUI:
```powershell
.\build\nanodb.exe
```

Run the automated test runner:
```powershell
.\build\test_runner.exe
```

---

## 3) Data Preparation (TPC-H)

### Required input files
Place these files in NanoDB/data:
- customer.tbl
- orders.tbl
- lineitem.tbl

### Expected scale
Total records ~100,000 rows:
- customer: 20,000
- orders: 30,000
- lineitem: 50,000

### Output binary files
The system converts .tbl to .bin:
- data/customer.bin
- data/orders.bin
- data/lineitem.bin

### GUI build
Use the "Build DB" button in the toolbar or left sidebar to generate .bin files.

---

## 4) Logging (Required for Evaluation)

The evaluation log is:
- logs/nanodb_execution.log

This file is written by a shared logger (console + file). It includes:
- Parser conversion logs: [LOG] Infix ... converted to Postfix ...
- LRU eviction logs: [LOG] Page X evicted via LRU, written to disk
- MST routing logs: [LOG] Multi-table join routed via MST: ...
- Benchmark logs: [BENCHMARK] ...

During the demo, always run the test runner at least once to regenerate this log in front of the evaluator.

---

## 5) Command Language (SQL-like)

Supported commands in the workload file and GUI:
- SELECT
- INSERT
- UPDATE
- SELECT_SEQ (benchmark sequential scan)
- SELECT_IDX (benchmark AVL index)
- STRESS_TEST (buffer pool stress)
- FLOOD_TEST (queue flooding)
- ADMIN (priority query wrapper)
- PERSIST_TEST (insert + reload in-process)

Notes:
- Use double quotes for strings.
- Use == for equality comparisons.
- Results preview in the GUI only supports single-table SELECT.

---

## 6) Live Demo Script (15 Minutes)

### Minute 0-2: Setup
- Open project
- Verify data/*.tbl exists
- Build and run test runner:

```powershell
.\build\test_runner.exe
```

### Minute 2-4: Show log output
Open logs/nanodb_execution.log and point to:
- Infix -> Postfix logs
- LRU eviction lines
- MST routing line
- Benchmark lines

### Minute 4-12: Test Cases A-G
Run the following live (GUI or CLI):

#### A) Parser and Evaluator
```
SELECT WHERE (c_acctbal > 5000 AND c_mktsegment == "BUILDING") OR c_nationkey == 15
```
Explain: tokenizer -> postfix -> evaluate using custom stacks

#### B) Index Optimizer
```
SELECT_SEQ * FROM customer WHERE c_custkey == 500
SELECT_IDX * FROM customer WHERE c_custkey == 500
```
Explain: sequential O(N) vs AVL O(log N)

#### C) Join Optimizer
```
SELECT * FROM customer JOIN orders JOIN lineitem ON c_custkey == o_custkey AND o_orderkey == l_orderkey
```
Explain: Graph + MST chooses join path; joined rows printed (first 50)

#### D) Memory Stress Test
```
STRESS_TEST lineitem 5000
```
Explain: buffer pool is fixed 50 pages, evictions logged

#### E) Priority Queue Concurrency
```
FLOOD_TEST 50
ADMIN UPDATE customer SET c_acctbal = 1234.56 WHERE c_custkey == 1
```
Explain: priority queue executes admin first

#### F) Deep Expression Tree
```
SELECT * FROM orders WHERE ((o_totalprice * 1.5) > 100000 AND (o_custkey % 2 == 0)) OR (o_orderstatus != "O")
```
Explain: parser handles precedence and arithmetic operators

#### G) Persistence (True Restart)
You must do a real restart for the viva:
1) INSERT 5 rows
2) Close NanoDB completely
3) Re-open and query those rows

Example inserts:
```
INSERT INTO customer VALUES (900001, "Persist User 1", "Persist Addr 1", 1, "555-9001", 1234.00, "BUILDING", "persist")
INSERT INTO customer VALUES (900002, "Persist User 2", "Persist Addr 2", 1, "555-9002", 1234.00, "BUILDING", "persist")
INSERT INTO customer VALUES (900003, "Persist User 3", "Persist Addr 3", 1, "555-9003", 1234.00, "BUILDING", "persist")
INSERT INTO customer VALUES (900004, "Persist User 4", "Persist Addr 4", 1, "555-9004", 1234.00, "BUILDING", "persist")
INSERT INTO customer VALUES (900005, "Persist User 5", "Persist Addr 5", 1, "555-9005", 1234.00, "BUILDING", "persist")
```

After restart:
```
SELECT * FROM customer WHERE c_custkey == 900001
SELECT * FROM customer WHERE c_custkey == 900005
```

---

## 7) Architecture (Component by Component)

### 7.1 Memory Layer (Pager)
- BufferPool holds a fixed array of Page objects (MAX_PAGES = 50)
- LRUCache is a custom doubly linked list + hash table for O(1) get/put/evict
- When full, the least recently used page is evicted and serialized to disk
- Logging: eviction lines are written to nanodb_execution.log

Data flow:
1) fetchPage(pageId, tableName)
2) check LRU
3) if miss and full -> evict LRU and flush
4) load from disk into Page

### 7.2 Schema and Type System
- Field is a polymorphic base class
- IntField, FloatField, StringField implement all comparison operators
- Row is a dynamic array of Field* (no STL)
- Table owns Row** and column name arrays

### 7.3 System Catalog (Hashing)
- Hash table with chaining by linked list
- O(1) average lookup for table metadata
- Contains table name, file path, column metadata

### 7.4 Parser and Evaluator
- Tokenizer breaks query into tokens
- PostfixConverter converts infix to postfix using custom Stack
- ExpressionEvaluator evaluates postfix using three stacks (num, bool, string)
- Logs: infix and postfix expressions are logged every time

### 7.5 Priority Queue
- Binary heap
- Lower priority number = higher priority
- Admin queries use priority 0
- FLOOD_TEST enqueues many SELECTs to show admin override

### 7.6 Index (AVL Tree)
- Self-balancing AVL
- insert, search, remove in O(log N)
- Search logs number of comparisons
- Benchmarks log sequential vs AVL time

### 7.7 Optimizer (Graph + MST)
- Each table is a node
- Join weights set by row counts
- MST computed using union-find
- Join order logged before executing

### 7.8 Execution Engine (QueryExecutor)
- SELECT: single-table scan with optional WHERE
- SELECT_SEQ: forced sequential scan with benchmark
- SELECT_IDX: forced AVL lookup with benchmark
- STRESS_TEST: page scan to trigger evictions
- PERSIST_TEST: inserts + reload verification
- JOIN: nested loop join for customer/orders/lineitem

### 7.9 Persistence and File I/O
- FileManager loads .tbl and saves .bin
- Pages serialize in binary
- INSERT and UPDATE flush changes to .bin

---

## 8) Complexity Analysis (Explain in Viva)

| Component | Operation | Complexity |
|----------|-----------|------------|
| BufferPool | fetchPage | O(1) average (LRU + hash) |
| LRUCache | get/put/evict | O(1) |
| SystemCatalog | lookup | O(1) average |
| Tokenizer | tokenize | O(n) in query length |
| PostfixConverter | convert | O(n) tokens |
| ExpressionEvaluator | evaluate | O(n) tokens |
| AVL Tree | insert/search | O(log N) |
| Sequential scan | search | O(N) |
| Join (nested loops) | 2-table | O(N*M) |
| Join (3-table) | O(N*M*L) |

---

## 9) Viva Q&A (Model Answers)

### Q1: Why no STL?
Because the rubric forbids it. All structures are manual arrays + pointers.

### Q2: How does LRU eviction work?
LRU is a doubly linked list. On access, the node moves to the head. Eviction removes tail in O(1). Hash table maps pageId -> node for O(1) access.

### Q3: How do you parse WHERE expressions?
Tokenizer -> infix to postfix using stack -> evaluate postfix with custom stacks for numeric and string values.

### Q4: Why AVL tree?
Guarantees O(log N) search by keeping the tree balanced after each insert/delete.

### Q5: How does MST help joins?
Tables become graph nodes, join costs are edges. MST yields minimum total cost order. That order is logged before join execution.

### Q6: How do you show persistence?
Insert rows, shut down program, relaunch, and query again. The FileManager writes .bin files on insert/update.

### Q7: What is the buffer pool size?
Fixed at 50 pages, enforced by MAX_PAGES in BufferPool.

---

## 10) Troubleshooting

- Missing .tbl files: place them in data/ and rebuild DB
- Empty log: run test_runner or execute queries in GUI
- No results: verify dataset present and use correct column names
- Build fails: ensure MinGW path is set in PATH

---

## 11) Submission Notes

- GitHub repo must show multiple commits
- Do not push .exe or TPC-H .tbl files
- Provide README with build/test instructions

---

## 12) Day-of-Demo Checklist

- Laptop charged
- Repo synced
- build completed
- data/*.tbl ready
- logs/nanodb_execution.log exists
- test_runner.exe ready
- queries A-G prepared

---

End of guide. This is your full viva script and technical explanation.
