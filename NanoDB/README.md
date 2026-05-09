# NanoDB

GitHub repository: https://github.com/kumailx051/NanoDB.git

NanoDB is a C++ query engine project that loads TPC-H data, builds local indexes, runs a scripted workload, and writes execution logs.

## Dependencies
- C++17 compiler
- CMake 3.20+
- GLFW3
- OpenGL
- Dear ImGui is bundled in `third_party/imgui/`

## Data Files
Place these TPC-H tables in `NanoDB/data/`:
- `customer.tbl`
- `orders.tbl`
- `lineitem.tbl`

The project expects only those three tables. A scale factor of `0.1` is suitable for the course dataset size target of roughly 100,000 rows total.

## Build
From the `NanoDB/` folder:

```bash
cmake -S . -B build
cmake --build build
```

## Run the GUI
After building:

```bash
./build/nanodb
```

## Run the Automated Test Runner
The test runner reads `tests/queries.txt`, loads the TPC-H tables, and executes the workload automatically.

```bash
./build/test_runner
```

The runner writes execution details to `logs/nanodb_execution.log`.

## Workload File
The automated workload is stored in `tests/queries.txt` and includes SELECT, INSERT, UPDATE, JOIN, priority queue, stress test, and persistence commands.

## Notes
- Compiled `.exe` files are ignored by `.gitignore` and are not meant to be committed.
- Raw TPC-H `.tbl` inputs are also ignored and should remain local.
