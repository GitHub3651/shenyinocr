# ResultReceiver

独立的 Qt TCP 接收端。实时路径只把合法产品 JSON 写入按日期划分的 JSONL，写入并 `flush` 成功后返回 ACK；重复 ID 直接返回相同 ACK，不依赖 CSV。

操作员可在界面点击“同步 JSONL 到 CSV”一次性处理输出目录中的所有日期。CSV 使用 UTF-8 BOM、无表头、每行一个值；OK 写二维码原文，NG 写 `noQR`。

ResultReceiver 固定监听本机所有 IPv4 网卡，实际地址为 `0.0.0.0:<操作员设置的端口>`。界面只配置监听端口，首次运行默认 `35680`，点击“开始监听”时保存端口和输出目录；例如端口设为 `35680` 时监听 `0.0.0.0:35680`，设为 `40000` 时监听 `0.0.0.0:40000`。

发送端不能连接 `0.0.0.0`，必须连接接收电脑实际的局域网 IP 和 ResultReceiver 界面设置的端口。例如接收电脑 IP 为 `192.168.10.100`、ResultReceiver 端口为 `35680`，发送端应配置为 `192.168.10.100:35680`。建议接收电脑使用固定 IP 或静态 IP，避免地址变化后发送端无法连接。

使用 Qt Creator 选择 Release 配置并重新运行 qmake、构建。链接完成后工程会调用当前 Qt Kit 自带的 `windeployqt`，将 `Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`、`Qt5Network.dll`、`platforms/qwindows.dll` 等运行文件部署到构建目录的 `release` 文件夹，并校验部署是否完整。交付到接收电脑时必须复制整个 `release` 文件夹，不能只复制 `ResultReceiver.exe`。
