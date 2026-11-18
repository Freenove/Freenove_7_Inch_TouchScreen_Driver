# Freenove 7 寸面板 + LVGL 配置指南

本文说明如何在 **ESP32-P4（16MB Flash + 16MB PSRAM）** 上配置并使用：

- `Freenove_7_Inch_TouchScreen_Driver`：屏幕 / 触摸 / 背光驱动
- `lvgl`（v9.x）：GUI，配置文件为 `lvgl/src/lv_conf.h`

后续写业务代码时，优先对照本文与示例 `04` / `05` / `06`。

---

## 1. 硬件与板选项

| 项目 | 值 |
|------|-----|
| 主控 | ESP32-P4 |
| Flash / PSRAM | 16MB / 16MB |
| 分辨率 | 1024 × 600，RGB565 |
| LCD | EK79007，MIPI-DSI |
| 触摸 | GT911（I2C） |
| 面板 MCU | I2C `0x45`（电源 / 复位 / 背光） |
| 默认 I2C | SDA=7，SCL=8 |

### Arduino IDE（必须）

1. 板卡必须选 **ESP32P4**（本库仅支持 ESP32-P4；其它芯片编译期 `#error`）
2. **PSRAM: Enabled**（不开则大图 / GIF / LVGL 缓存会分配失败）
3. Flash Size 选 **16MB**
4. 串口监视器建议 115200

---

## 2. 库目录结构

典型路径（Windows）：

```text
Documents/Arduino/libraries/
├── Freenove_7_Inch_TouchScreen_Driver/  # 本库
│   ├── src/
│   │   ├── freenove_lvgl_demo_widgets.c   # Widgets Demo 打包（见第 7 节）
│   │   └── freenove_lvgl_demo_widgets.h
│   ├── examples/
│   └── docs/LVGL_Configuration_Guide.md   # 本文
└── lvgl/                      # 第三方库：除 lv_conf.h 外不要改目录/源码
    ├── src/
    │   └── lv_conf.h          # ★ 唯一应改的配置入口（Freenove 已调过）
    ├── demos/                 # 上游官方 demos（保持原样，不要拷进 src）
    └── examples/              # 上游官方 examples（保持原样）
```

依赖关系：

- 仅画点 / 触摸 / 背光 → 只需 `Freenove_7_Inch_TouchScreen_Driver`
- 用 LVGL UI → 同时安装 `lvgl`，只通过 `lv_conf.h` 做板级配置；UI 写在 Freenove 示例或你的草图里

`library.properties` 中 `includes=Freenove_7_Inch_Panel.h`，草图里写：

```cpp
#include <Freenove_7_Inch_Panel.h>
#include <lvgl.h>   // 用 LVGL 时
```

---

## 3. Freenove_7_Inch_TouchScreen_Driver 用法

### 3.1 最小初始化

```cpp
#include <Freenove_7_Inch_Panel.h>

Freenove_7_Inch_Panel panel;

void setup() {
    Serial.begin(115200);
    if (!panel.begin()) {          // Wire + 上电 + DSI + FB + 触摸
        while (true) delay(1000);
    }
    panel.setBacklight(70);        // 0..100
}

void loop() {
    uint16_t x, y;
    if (panel.getTouch(&x, &y)) {
        // ...
    }
}
```

注意：`begin()` 内部会初始化 `Wire`，不要再用别的引脚调 `Wire.begin()`。

### 3.2 直接画 framebuffer（不用 LVGL）

```cpp
uint16_t *fb = (uint16_t *)panel.getFrameBuffer();
fb[y * Freenove_7_Inch_Panel::WIDTH + x] = panel.color565(255, 0, 0);

// DPI 从 PSRAM 读帧缓冲：CPU 写完必须 cache sync
panel.syncFrameBuffer();                 // 全屏
// 或
panel.syncFrameBufferLines(y0, y1);      // [y0, y1) 行，局部刷新更省
```

也可用封装 API：`fillScreen` / `drawPixel` / `drawLine` / `drawImage` / `print` 等。  
`setAutoSync(true)` 可在每次绘制后自动 sync（方便但批量绘制更慢）；默认关闭。

### 3.3 方向与触摸校正

| API | 作用 |
|-----|------|
| `setDisplayMirrorX/Y` | 面板 MADCTL 镜像画面 |
| `setTouchMirrorX/Y` / `setTouchSwapXY` | 软件变换触摸坐标 |

LVGL 示例里用宏统一设置（见 `Freenove_LVGL_Port.h`）：

```cpp
#define DISPLAY_DIRECTION  (0)   // 0..3 画面镜像
#define TOUCH_DIRECTION    (3)   // 0..7 触摸镜像/交换（默认 3 = 镜像 X+Y）
```

改方向后重新编译即可，无需改业务 UI。

### 3.4 示例一览

| 示例 | 内容 |
|------|------|
| `01.ColorBar` | 色条，验证屏 |
| `02.Draw` | 直接画图 API |
| `03.Touch` | 触摸点 |
| `04.LvglBasic` | LVGL 标签 + 按钮 |
| `05.LvglSdGallery` | SD 卡 JPG/BMP/PNG 相册 |
| `06.LvglArduino` | 官方 Widgets Demo（经本库打包上游 demos） |

---

## 4. LVGL：`lv_conf.h` 关键配置

路径：`lvgl/src/lv_conf.h`  
带 `Freenove:` 注释的项是为本板调过的，改之前先看注释。

### 4.1 内存（最重要）

```c
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
```

| 模式 | 行为 | 建议 |
|------|------|------|
| `LV_STDLIB_CLIB`（推荐） | `lv_malloc` → 系统 `malloc`，可吃 PSRAM | 大图 / GIF / Demo |
| `LV_STDLIB_BUILTIN` | 固定池 `LV_MEM_SIZE`（曾为 128KB） | 不够解 600×600 PNG |

LodePNG 也走 `lv_malloc`。600×600 PNG 解码约需 **~1.4MB**，在 128KB 池上必失败，即使 `LV_USE_LODEPNG` 已开。

有 16MB PSRAM 时：**用 CLIB + IDE 打开 PSRAM**，不要靠加大固定池硬扛。

可选缓存（按需）：

```c
#define LV_CACHE_DEF_SIZE (2 * 1024 * 1024)  // 解码图缓存，相册切换更顺；可再加大
```

### 4.2 颜色与绘制

```c
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_DPI_DEF 170
#define LV_DRAW_BUF_ALIGN 64          // P4 cache line，配合 syncFrameBufferLines
#define LV_DEF_REFR_PERIOD 10         // 刷新/触摸轮询更跟手
#define LV_USE_OS LV_OS_NONE          // Arduino 单 loop，不接 RTOS
```

面板吃的是 **非 swapped** RGB565（`LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 0`）。

### 4.3 图片解码与 SD 文件系统

```c
#define LV_USE_LODEPNG 1     // PNG（内置 LodePNG）
#define LV_USE_BMP 1
#define LV_USE_TJPGD 1       // JPEG（软件，速度一般）
#define LV_USE_GIF 1
#define LV_GIF_CACHE_DECODE_DATA 1
#define LV_GIF_MAX_WIDTH 1024
#define LV_GIF_MAX_HEIGHT 1024

#define LV_USE_FS_POSIX 1
#define LV_FS_POSIX_LETTER 'S'
#define LV_FS_POSIX_PATH "/sdcard"   // 必须与 SD 挂载点一致
```

LVGL 里打开 SD 上的文件：

```cpp
lv_image_set_src(img, "S:/3.png");   // → 实际路径 /sdcard/3.png
```

`05` 示例用 `Freenove_SD_Card` 挂载到 `/sdcard`，与上面一致。

### 4.4 字体（Widgets Demo / 大屏）

已开启示例：

- `LV_FONT_MONTSERRAT_12 / 14 / 16 / 18 / 20 / 24 / 26`

1024 宽屏走 Widgets 的 **LARGE** 分支，主要用 **16 + 24**。

### 4.5 Demo / Example 开关（Freenove 当前值）

```c
#define LV_BUILD_EXAMPLES 1
#define LV_BUILD_DEMOS 1
#define LV_USE_DEMO_WIDGETS 1   // Freenove: 声明 API；源码由本库 .c 打包，不拷进 lvgl/src
#define LV_USE_PRIVATE_API 1    // Freenove: demos 依赖私有头
```

说明：

- Arduino **只会自动编译** `lvgl/src/`；`lvgl/demos/`、`lvgl/examples/` **不会**进固件。
- `LV_USE_DEMO_WIDGETS 1` 只打开头文件/API；真正把 demo 源码编进固件靠本库第 7 节方案。
- 不需要 Widgets 时可改回 `0`，并去掉草图对 `freenove_lvgl_demo_widgets.h` / `build_opt.h` 的依赖。

### 4.6 布局 / 主题（Demo 依赖）

```c
#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_THEME_DEFAULT 1
```

Widgets Demo 要求 FLEX + GRID。

---

## 5. LVGL 与面板对接（推荐写法）

不要在草图里手写 flush/触摸样板，直接拷贝示例中的：

- `Freenove_LVGL_Port.h`
- `Freenove_LVGL_Port.cpp`

（`04` / `05` / `06` 中内容一致。）

### 5.1 标准流程

```cpp
#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>
#include "Freenove_LVGL_Port.h"

Freenove_7_Inch_Panel panel;
Freenove_LVGL_Port lvglPort(panel);

void setup() {
    Serial.begin(115200);
    if (!panel.begin()) { while (true) delay(1000); }
    panel.setBacklight(70);
    if (!lvglPort.begin()) { while (true) delay(1000); }  // lv_init + display + indev

    // 在此创建 UI：lv_label_create / lv_demo_widgets / ...
}

void loop() {
    lvglPort.handler();   // == lv_timer_handler()
    delay(5);
}
```

`lvglPort.begin()` 内部会：

1. 应用 `DISPLAY_DIRECTION` / `TOUCH_DIRECTION`
2. `lv_init()` + `lv_tick_set_cb(millis)`
3. 分配 64 字节对齐的 partial draw buffer（默认约 40 行）
4. 注册 flush：拷贝到 DPI framebuffer → `syncFrameBufferLines`
5. 注册指针 indev（含短时 touch hold，减轻 GT911 丢点导致的按键抖动）

### 5.2 自建 UI 注意点

- 默认 screen 已被去掉 `SCROLLABLE`，避免子控件越界带动整屏滚动
- 颜色深度按 RGB565 设计；图片解码后由 LVGL 转成屏格式
- 大图 / GIF 尽量接近显示尺寸，别无用“原图像素远大于控件”

---

## 6. SD 卡图片 / GIF

### 6.1 挂载

使用 `05` 中的 `Freenove_SD_Card`（TF 槽 SDMMC 4-bit，挂载 `/sdcard`）：

```cpp
#include "Freenove_SD_Card.h"
Freenove_SD_Card sd;
if (!sd.begin()) { /* halt */ }
```

### 6.2 路径约定

| LVGL 路径 | 卡上真实路径 |
|-----------|----------------|
| `S:/1.jpg` | `/sdcard/1.jpg` |
| `S:/gif/a.gif` | `/sdcard/gif/a.gif` |

盘符 `S` 与挂载点 `/sdcard` 都在 `lv_conf.h`，改一处必须两边对齐。

### 6.3 素材建议（16MB PSRAM）

| 类型 | 建议 |
|------|------|
| 静态 PNG/JPG | 边长贴近控件（如 300～600）；全屏可 1024×600 JPG |
| GIF | 尽量 ≤ 屏宽；`W×H×4` 为画布量级，勿同时播多个大 GIF |
| 解码失败排查 | ① PSRAM 是否开启 ② 是否仍为 BUILTIN 小堆 ③ 文件是否损坏 / 路径是否带 `S:` |

PNG 本身合法但“显示不出来”，多数是 **内存不足**，不是解码器没开。

---

## 7. 第三方库边界与 Widgets Demo（重要）

### 7.1 边界约定

`lvgl` 是第三方库，本仓库对它的约定：

| 可以做 | 不要做 |
|--------|--------|
| 只改 `lvgl/src/lv_conf.h`（板级配置，保留 `Freenove:` 注释） | 把 `demos` / `examples` **拷进** `lvgl/src/` |
| 在 **Freenove 库** 里 `#include` 上游 `lvgl/demos/...` 源文件进行编译 | 改 lvgl 源码里的相对 `#include` |
| 草图写 UI 或调用 `lv_demo_widgets()` | 为跑通 Arduino 去破坏上游目录结构 |

### 7.2 为什么需要本库打包？

Arduino 默认只编译 `lvgl/src/`，库根下的 `lvgl/demos` 不会自动进固件。  
因此本库用一个 **C 翻译单元** 把上游 `.c` 拉进来编译，源文件仍留在 `lvgl/demos/`，不复制、不改路径。

### 7.3 本库做法（`06.LvglArduino`）

| 步骤 | 位置 | 作用 |
|------|------|------|
| 1 | `lv_conf.h` | `LV_USE_DEMO_WIDGETS 1`、`LV_USE_PRIVATE_API 1` |
| 2 | `src/freenove_lvgl_demo_widgets.c` | 定义了 `FREENOVE_LVGL_DEMO_WIDGETS` 时，`#include` 上游 demos |
| 3 | `06/build_opt.h` | `-DFREENOVE_LVGL_DEMO_WIDGETS=1` |
| 4 | 草图 | `#include <freenove_lvgl_demo_widgets.h>` 后调用 `lv_demo_widgets()` |

`FREENOVE_UI_MODE`（草图内）：`1` → widgets；`0` → Hello 标签。

打包文件当前包含：

```c
#include "../../lvgl/demos/lv_demos.c"                 // lv_demo_args_init 等公共辅助
#include "../../lvgl/demos/widgets/lv_demo_widgets.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_components.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_profile.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_analytics.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_shop.c"
#include "../../lvgl/demos/widgets/assets/img_*.c"     // logo / clothes / avatar / needle
```

其他 Freenove 示例不传 `FREENOVE_LVGL_DEMO_WIDGETS` 时，该 `.c` 为空，不占体积。

### 7.4 必须用 `.c`，不能用 `.cpp`

上游 demos 是 **C99**：`void*` 隐式转换、嵌套 designated initializer。  
若写成 `.cpp`，Arduino 会用 `g++` 编译，即使包了 `extern "C"` 也只改链接名、不改语言规则，会出现：

- `invalid conversion from 'void*' to 'lv_obj_t*'`
- `expected primary-expression before '.' token`（嵌套 `.header.w = ...`）

因此打包文件必须是 `freenove_lvgl_demo_widgets.c`；头文件里对 C++ 草图提供 `extern "C"` 声明即可。

### 7.5 链接注意

`lv_demo_widgets()` 会调用 `lv_demo_args_init()`（定义在 `lvgl/demos/lv_demos.c`）。  
打包时必须一并 `#include`，否则链接报：

```text
undefined reference to `lv_demo_args_init'
```

当前 `lv_conf` 只开了 Widgets，不会连带拉起 Music / Benchmark 等其它 demo。

---

## 8. 新建工程检查清单

1. IDE：**PSRAM Enabled**，Flash 16MB  
2. 库：`Freenove_7_Inch_TouchScreen_Driver` + `lvgl`（只通过 `lv_conf.h` 配置）  
3. 内存：`LV_USE_STDLIB_MALLOC == LV_STDLIB_CLIB`  
4. 需要图：确认 `LODEPNG` / `TJPGD` / `GIF` 与 `FS_POSIX` + `S:` + `/sdcard`  
5. 草图：拷贝 `Freenove_LVGL_Port.*`，按第 5 节初始化；UI 写在草图里  
6. 方向不对：只改 `DISPLAY_DIRECTION` / `TOUCH_DIRECTION`  
7. 直接写 FB：记得 `syncFrameBuffer` / `syncFrameBufferLines`  
8. **不要**往 `lvgl/src/` 拷 demos/examples；要 Widgets 用 `06` + `build_opt.h`（第 7 节）

---

## 9. 最小 LVGL 业务模板

```cpp
#include <lvgl.h>
#include <Freenove_7_Inch_Panel.h>
#include "Freenove_LVGL_Port.h"

Freenove_7_Inch_Panel panel;
Freenove_LVGL_Port lvglPort(panel);

static void createUi() {
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "My App");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void setup() {
    Serial.begin(115200);
    if (!panel.begin()) while (true) delay(1000);
    panel.setBacklight(70);
    if (!lvglPort.begin()) while (true) delay(1000);
    createUi();
}

void loop() {
    lvglPort.handler();
    delay(5);
}
```

把 `Freenove_LVGL_Port.h/.cpp` 与 `.ino` 放在同一草图目录即可。

---

## 10. 常见问题

| 现象 | 可能原因 | 处理 |
|------|----------|------|
| 黑屏 / `begin` 失败 | 供电、排线、I2C | 看串口 log；先跑 `01.ColorBar` |
| 画面正常触摸反了 | 触摸变换 | 改 `TOUCH_DIRECTION` |
| PNG/GIF 不显示，路径正确 | 内存 / PSRAM | 确认 CLIB + PSRAM；缩小图片 |
| 找不到 `demos/lv_demos.h` | Arduino 不编库根 demos | 用第 7 节打包；勿往 `lvgl/src` 拷文件 |
| `void*` / designated initializer 编译失败 | demo 被编成 C++ | 确认打包文件是 `.c` 不是 `.cpp` |
| `undefined reference to lv_demo_args_init` | 未编入 `lv_demos.c` | 打包里加上 `#include ../../lvgl/demos/lv_demos.c` |
| 局部花屏 / 底部残影 | 未 cache sync | flush 后调用 `syncFrameBufferLines`（Port 已做） |
| JPG 解码偏慢 | 当前走软件 TJPGD | 见第 12 节后续计划（硬件 JPEG） |

---

## 11. 相关文件速查

| 文件 | 说明 |
|------|------|
| `Freenove_7_Inch_TouchScreen_Driver/src/Freenove_7_Inch_Panel.h` | 面板 API（类名仍为 `Freenove_7_Inch_Panel`） |
| `src/freenove_lvgl_demo_widgets.c` / `.h` | Widgets Demo 打包（C 翻译单元） |
| `examples/06/build_opt.h` | 打开 `FREENOVE_LVGL_DEMO_WIDGETS` |
| `examples/04~06/Freenove_LVGL_Port.*` | LVGL display + touch 移植 |
| `examples/05/Freenove_SD_Card.*` | TF 卡挂载 |
| `lvgl/src/lv_conf.h` | LVGL 全部宏配置（对第三方库唯一应改处） |
| `lvgl/demos/`、`lvgl/examples/` | 上游官方示例（保持原样，不要挪进 src） |

配置有变更时，优先改 `lv_conf.h` 并在改动旁保留 `Freenove:` 注释，便于以后对比上游模板。

---

## 12. 后续计划

| 项 | 说明 | 状态 |
|----|------|------|
| ESP32-P4 硬件 JPEG 解码 | 用乐鑫 `esp_lv_decoder` / JPEG 外设替代或旁路软件 TJPGD，提升相册等大图速度 | 已列入计划，暂未开工 |
| 相册示例性能 | 在 `05.LvglSdGallery` 上接入硬件解码路径 | 依赖上一项 |
