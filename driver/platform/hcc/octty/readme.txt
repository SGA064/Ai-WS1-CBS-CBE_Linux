octty新增在位检测功能，使用方法如下：

使用命令为：octty [chip_type_to_detect] [bardrate] [xtal_freq] [gpio_uart_rx] [gpio_power_on]

参数说明：
[chip_type_to_detect]：芯片型号，当前固定为ws73,其他值返回错误
[bardrate]：uart 波特率，当前ws73 romboot状态下，该值固定为1000000，其他值会导致通讯错误
[xtal_freq]:该入参暂不检查
[gpio_uart_rx]：uart rx 管脚号
[gpio_power_on]：ws73 power on 管脚号
以上参数不支持缺省

命令示例：octty ws73 1000000 40M 12 40
调用命令后输入 echo $? 命令获取返回值

验证参考：
~ # ./octty ws73 100000 40M 12 40  		// 设置错误波特率，通讯异常，导致检测失败
octty open
octty chip detect test done
~ # echo $?
190										// 检测失败返回非0异常值
~ # ./octty ws73 1000000 40M 12 40		// 设置正确参数，且73在位，检测成功
octty open
octty chip detect test done
~ # echo $?
0										// 检测成功，返回0

以上内容在3518环境下验证通过，其他不同主控请注意以下需要适配的内容：
1. UART_TTY_DEV_NAME 串口设备名
2. UART_MUX_REG以及UART_MUX_REG_BASE 串口管脚复用控制寄存器
3. gpio_export及其他操作接口中，操作文件节点时，文件节点路径在不同主控下可能不同