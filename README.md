## 1. 烧录命令
我是用 VSCode 工程自带的 Makefile 编译，然后用 STM32CubeProgrammer 的命令行工具通过 ST-LINK 烧录的。
---
### 1.1. 进入工程目录
在终端执行：
cd "/Users/jc/Desktop/Practica1_IoT_Julio"
---
### 1.2. 编译工程
先指定 STM32 ARM GCC 工具链：
STM_GCC_DIR="/Users/jc/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin"

PATH="$STM_GCC_DIR:$PATH" make -C Debug -j4 all
成功时会看到：
Finished building target: Practica1_IoT_Julio.elf
生成的烧录文件是：
/Users/jc/Desktop/Practica1_IoT_Julio/Debug/Practica1_IoT_Julio.elf
你的 Makefile 原来引用了老师 Windows 电脑上的路径：
C:\Users\jruimor\...
我已经改成当前工程可用的相对路径：
../STM32F303ZETX_FLASH.ld
所以以后可以直接执行上面的编译命令。
---
### 1.3. 检查板子是否连接
执行：
"/Users/jc/Library/Application Support/stm32cube/bundles/programmer/2.23.0/bin/STM32_Programmer_CLI" --list
当时检测到：
Board Name  : NUCLEO-F303ZE
ST-LINK SN  : 066FFF544949878667191126
---
### 1.4. 烧录、校验并复位
执行的完整命令是：
"/Users/jc/Library/Application Support/stm32cube/bundles/programmer/2.23.0/bin/STM32_Programmer_CLI" \
  -c port=SWD sn=066FFF544949878667191126 \
  -w Debug/Practica1_IoT_Julio.elf \
  -v \
  -rst
参数含义：
- -c port=SWD：通过 ST-LINK 的 SWD 接口连接芯片。
- sn=...：指定当前这块板子的 ST-LINK。
- -w ...elf：将编译出的程序写入 Flash。
- -v：烧录后读取并校验。
- -rst：成功后复位 MCU，让新程序立即运行。
---
### 1.5. 烧录结果
最终出现下面两句就说明烧录成功：
Download verified successfully
Software reset is performed
以后修改代码后，只需要重复“第 2 步编译”和“第 4 步烧录”即可。
---