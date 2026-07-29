# BarcodeDecoder.dll

这是主程序二维码读码接口的可重建源码工程。它保持
`../BarcodeDecoderApi.h`中的原有C ABI，不依赖Qt和OpenCV。

底层优先使用ZXing-C++ 3.1.0-rc1。ZXing快速路径失败并且主程序
传入`TRY_HARDER`时，Data Matrix会再使用libdmtx 0.7.8进行一次
限时兜底。兜底前固定执行3×3中值滤波和灰度范围归一化，单次
libdmtx区域搜索预算为15 ms。

两个引擎都静态链接到同一个DLL中，主程序目录不需要增加其他DLL。
构建时只启用Data Matrix和QR Code读取，关闭写码器、示例和测试，
并使用静态CRT生成独立x64 DLL。

## 构建

前提：

- Visual Studio 2022；
- 使用“使用C++的桌面开发”组件；
- CMake；
- 第一次配置工程时能够访问GitHub。

双击：

```text
build_release_vs2022.cmd
```

生成文件：

```text
build-vs2022/dist/Release/BarcodeDecoder.dll
```

先关闭主程序，再备份并替换主程序同目录中的旧DLL。主程序日志应显示：

```text
BarcodeDecoder/2.1.0 ZXing-C++/3.1.0-rc1 libdmtx/0.7.8
```

ZXing-C++许可证为Apache-2.0：

```text
https://github.com/zxing-cpp/zxing-cpp
```

libdmtx许可证为Simplified BSD：

```text
https://github.com/dmtx/libdmtx
```
