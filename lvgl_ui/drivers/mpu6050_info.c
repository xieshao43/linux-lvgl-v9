//
// Created by lxj on 24-4-22.
//
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "AHRS.h"
fp32 INS_quat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
fp32 INS_angle[3] = {0.0f, 0.0f, 0.0f};      //euler angle, unit rad.欧拉角 单位 rad
void* mpu6050_read_acc()
{
    short resive_data[6];
    static const float timing_time = 0.001f;   //tast run time , unit s.任务运行的时间 单位 s
    fp32 INS_gyro[3] = {0.0f, 0.0f, 0.0f};
    static fp32 INS_accel[3] = {0.0f, 0.0f, 0.0f};
    static fp32 INS_mag[3] = {0.0f, 0.0f, 0.0f};
    printf("led_tiny test\n");

    /*打开文件*/
    int fd = open("/dev/i2c-0", O_RDWR);
    if(fd < 0)
    {
        printf("打开I2C设备失败！请检查权限并确保i2c-dev模块已加载\n");
        while (1);
    }
    
    // 设置I2C从设备地址为0x68(MPU6050的默认地址)
    if(ioctl(fd, I2C_SLAVE, 0x68) < 0) {
        printf("无法设置I2C从设备地址，请确认MPU6050已正确连接\n");
        close(fd);
        while (1);
    }
    
    AHRS_init(INS_quat,INS_accel,INS_mag);
    int error;
    while (1)
    {

        /*读取数据*/
        error = read(fd,resive_data,12);
        if(error < 0)
        {
            printf("write file error! \n");
            close(fd);
            /*判断是否关闭成功*/
        }

//        printf("AX=%d, AY=%d, AZ=%d ",(int)resive_data[0],(int)resive_data[1],(int)resive_data[2]);
//        printf("GX=%d, GY=%d, GZ=%d \n \n",(int)resive_data[3],(int)resive_data[4],(int)resive_data[5]);
        for (int i = 0; i < 3; ++i) {
            INS_accel[i] = resive_data[i];
            INS_gyro[i] = resive_data[i+3];
        }
        AHRS_update(INS_quat,timing_time,INS_gyro,INS_accel,INS_mag);
        get_angle(INS_quat,&INS_angle[0],&INS_angle[1],&INS_angle[2]);
        printf("yaw=%f, pitch=%f, roll=%f \n \n",INS_angle[0]*57.29577951308238f,INS_angle[1]*57.29577951308238f,INS_angle[2]*57.29577951308238f);
        usleep(1000);
    }

    error = close(fd);
    if(error < 0)
    {
        printf("close file error! \n");
    }

    return 0;
}
