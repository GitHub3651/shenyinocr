// 文件作用：本文件用于建立Qt应用进程，执行启动保护并进入主事件循环。
// 主要职责：建立Qt应用进程，执行启动保护并进入主事件循环。
// 模块位置：启动层；只负责进程初始化和对象组装，不放置业务规则。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "startup/application_startup.h"

// 函数说明：main 函数完成应用进程初始化并进入Qt事件循环。
int main(int argc, char *argv[])
{
    return ApplicationStartup::run(argc, argv);
}
