# Cache 参数测试工具

在 Python 3 和 `make` 可用的环境中运行脚本，不带参数时会进入交互模式：

```sh
python3 tools/cache_test.py
```

按提示输入测试名称、I-cache 和 D-cache 的参数，以及要运行的测试文件：

- 测试名称直接回车时使用 `test1`。
- 任一 cache 参数直接回车时使用原始参数：I-cache 为 `6/4/5`，D-cache 为 `8/8/5`。
- 测试文件直接回车时运行全部测试；输入一个或多个 `.x` 文件路径（以空格分隔）则只运行指定文件。

例如，只运行 cache 测试：

```sh
python3 tools/cache_test.py
# 按提示输入测试名称 test1，六项参数均回车，测试文件输入：
# inputs/cache/test1.x
```

工具会更新 `src/cache.h` 中的六个参数，执行 `make clean` 和 `make` 确保重新编译，
再将标准输出保存到 `results/<测试名称>.txt`。随后在
`results/cache_parameter_map.csv` 中新增或更新对应测试名称的记录，包含输入文件和
全部六个 cache 参数。相同测试名称再次运行时，会覆盖对应结果文件并更新映射表中的记录。

也可以通过命令行传入参数，便于自动化运行；未指定的参数仍使用原始值：

```sh
python3 tools/cache_test.py test1 \
  --i-group-bits 6 --i-ways 4 --i-offset-bits 5 \
  --d-group-bits 8 --d-ways 8 --d-offset-bits 5 \
  inputs/cache/test1.x
```

参数范围：组索引位数为 0–30，块内偏移位数为 2–30；路数必须大于 0，且不超过
无符号 32 位整数的范围。
