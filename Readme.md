# sorter and cache

### How to build
```bash
$ cd sort_and_cache
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .
```
### This repo consist of 3 apps 
## `generate_file` usage (generates 1gb file)
```bash
$ generate_file input.txt
```
## `sort_files` usage (sorts input.txt)
```bash
$ sort_files input.txt input.sorted.txt
```
> **features/limitations:**
>
> - Uses `placament new` to reduce memory allocs
> - To support multi thread program uses thread-specific buffers
> - **Multithreading is not done completely!**
> * **Note:** On `linux` sorting takes approximately 2 min, while on win it may take +-18 min.

## `cache_files` usage (interaction via `cin`/`cout`)
```bash
$ cache_files
```

