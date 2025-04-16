# linux_lvgl_V9.0
Install steps
bash
mkdir build && cd build \  
cmake ..   \ 
make -j   \
cd /linux-lvgl-v9/bin \  
./main   
# Linux board minimum requirements
CPU: Allwinner H3, 四核 Cortex-A7 @ 1GHz \
GPU: ARM Mali400 MP2 GPU \
内存: 512MB LPDDR3 RAM \
存储: 16GB eMMC \
接口: 以太网，SPI, I2C, UART, 可复用的 GPIO, MIC, LINEOUT \
GPIO: 2.0mm 间距 26 针式接头连接器，包括 USB OTG，USB 串口，I2C，UART，SPI，I2S，GPIO \
USB: USB 2.0×2 / USB Type-C×1 \
Wifi: RTL8723BU IEEE 802.11 b/g/n @2.4GHz \
蓝牙: BT V2.1 / BT V3.0 / BT V4.0 \
板载外设: 麦克风，MPU6050 运动传感器（陀螺仪 + 加速度计）, 按钮（GPIO-KEY, Uboot, Recovery, Reset） \
显示屏: ST7789vw 驱动 1.14 寸，分辨率 240×135 \
外部存储: Micro SD 卡插槽 \
喇叭功放: LM4871，最大推 3 瓦功率的喇叭 
# Update log
Ported the lvgl 9.0 \
Added an lvgl example to monitor CPU and Memory usage \
Modified lv_conf.h settings for the core and st7789w framebuffer \
Features \
Added a self-designed example that can build and run on Linux \
Optimized the performance and efficiency of the Linux version of lvgl \
Replaced the Uboot loading logo  \ 
Tested hardware compatibility with peripherals (e.g., serial ports)  
