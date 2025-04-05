# linux_lvgl_V9.0

Install steps:

  mkdir build and cd build
  cmake ..
  make -j
  cd /linux-lvgl-v9/bin
  ./main

#linux board need lowest:

CPU: Allwinner H3, 四核Cortex-A7 @ 1GHz
GPU: ARM Mali400 MP2 GPU
内存: 512MB LPDDR3 RAM
存储: 16GB eMMC
接口: 以太网, SPI, I2C, UART, 可复用的GPIO, MIC, LINEOUT
GPIO: 2.0mm间距26针式接头连接器，包括USB OTG，USB串口，I2C，UART，SPI，I2S，GPIO
USB: USB 2.0×2 USB Type-C×1
Wifi: RTL8723BU IEEE 802.11 b/g/n @2.4GHz
蓝牙: BT V2.1/ BT V3.0/ BT V4.0
板载外设: 麦克风, MPU6050运动传感器(陀螺仪 + 加速度计), 按钮 (GPIO-KEY, Uboot, Recovery, Reset)
显示屏: ST7789vw驱动1.14寸，分辨率135x240
外部存储: Micro SD卡插槽
喇叭功放：LM4871，最大推3瓦功率的喇叭

# Update line:

 Ported the lvgl 9.0 
 Write in  the lvgl example that monitor the CPU and Memory 
 Rechanged the lv_conf settings for the Core and st7789w framebuffer


# What it do?

  Add an self-design example that could builds and opperates in linux

  Optiminze the linux version lvgl performance and effective

  Repalce the logo that started with uboot loading

  Try to see the application could work with hardware such as serrials 

