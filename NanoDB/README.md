# NanoDB
GitHub: [your repo link]

## Dependencies
- Dear ImGui (included in third_party/imgui/)
- GLFW3
- OpenGL

## Build
mkdir build && cd build
cmake .. && make

## Run GUI
./nanodb

## Run Tests
./test_runner

## Required Data
Place TPC-H .tbl files in data/ folder:
- data/customer.tbl
- data/orders.tbl
- data/lineitem.tbl
Generate using: https://github.com/electrum/tpch-dbgen
Scale factor 0.1 gives ~100K rows total
