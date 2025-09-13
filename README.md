```markdown
# STM32F103-Demo

本仓库收集常用外设/功能的 **最小可运行示例**，每个示例均为独立文件夹，可直接用 CubeMX + Keil 快速跑通。

## 仓库结构

```
STM32F103-Demo/                 
├─ 01-ADC_DMA/
│  ├─ ADC_DMA.ioc // CubeMX 工程
│  ├─ main.c      // 已修改的 main.c
│  └─ USER/       // 用户额外代码
└─ ...
```

## 运行任何一个示例的 3 步

1. **打开 CubeMX**  
   双击 `.ioc` 文件 → 点击 **Generate Code**（保持路径不变）。

2. **替换 main.c**  
   把同目录下的 `main.c` 覆盖掉 `Core/Src/main.c`。

3. **Keil 添加 USER 代码**  
   - 打开生成的 `.uvprojx`  
   - 右键 **Project → Add Group…** 新建 `USER`  
   - 右键 `USER` → **Add Existing Files…** 把当前示例 `USER/` 下所有 `.c` 文件加进来  
   - **Rebuild** → **Download** → 运行。

> 注意：示例默认使用 **STM32F103C8T6**、**HSE 8 MHz**、**Keil-MDK 5**。若芯片不同，请在 CubeMX 中先切换对应型号再生成。cubemx建议使用最新版6.15，cubemx向前兼容。
