/************************************************************************/
/* 以C++接口为基础，对常用函数进行二次封装，方便用户使用                */
/************************************************************************/

#ifndef _MV_CAMERA_H_
#define _MV_CAMERA_H_

#include "MvCameraControl.h"
#include <string.h>
#include <condition_variable>
#include <mutex>
#include "opencv.hpp"
#include <atomic>

//注意此处可能会跟系统函数定义冲突
using namespace cv;
using namespace std;
#ifndef MV_NULL
#define MV_NULL    0
#endif

class CMvCamera
{
public:
    CMvCamera();
    ~CMvCamera();
    cv::Mat lastImage;
    cv::Mat secondLastImage;
    cv::Mat image;
    cv::Mat m_image;
    std::mutex m_mutex;
    atomic<bool> m_nonblocking{true};
    atomic<bool> m_switchafternext{false};
    atomic<uint64_t>m_frameseq{0};
    condition_variable m_cv;
    bool m_imageReady=false;
    std::condition_variable m_cvForGetImage;  // 用于GetImage函数的条件变量
    std::condition_variable m_cvForMain;  // 用于主线程的条件变量
    bool m_imageReadyForGetImage = false;  // GetImage函数的图像准备状态
    bool m_imageReadyForMain = false;  // 主线程的图像准备状态
    int datasize;
    unsigned char *mpdata;
    void* GetHandle() const;
    // ch:获取SDK版本号 | en:Get SDK Version
    static int GetSDKVersion();

    // ch:枚举设备 | en:Enumerate Device
    static int EnumDevices(unsigned int nTLayerType, MV_CC_DEVICE_INFO_LIST* pstDevList);

    // ch:判断设备是否可达 | en:Is the device accessible
    static bool IsDeviceAccessible(MV_CC_DEVICE_INFO* pstDevInfo, unsigned int nAccessMode);

    // ch:打开设备 | en:Open Device
    int Open(MV_CC_DEVICE_INFO* pstDeviceInfo);

    // ch:关闭设备 | en:Close Device
    int Close();

    // ch:判断相机是否处于连接状态 | en:Is The Device Connected
    bool IsDeviceConnected();

    // ch:注册图像数据回调 | en:Register Image Data CallBack
    int RegisterImageCallBack();

    // ch:开启抓图 | en:Start Grabbing
    int StartGrabbing();

    // ch:停止抓图 | en:Stop Grabbing
    int StopGrabbing();

    // ch:主动获取一帧图像数据 | en:Get one frame initiatively
    int GetImageBuffer(MV_FRAME_OUT* pFrame, int nMsec);

    // ch:释放图像缓存 | en:Free image buffer
    int FreeImageBuffer(MV_FRAME_OUT* pFrame);

    // ch:显示一帧图像 | en:Display one frame image
    int DisplayOneFrame(MV_DISPLAY_FRAME_INFO* pDisplayInfo);

    // ch:设置SDK内部图像缓存节点个数 | en:Set the number of the internal image cache nodes in SDK
    int SetImageNodeNum(unsigned int nNum);

    // ch:获取设备信息 | en:Get device information
    int GetDeviceInfo(MV_CC_DEVICE_INFO* pstDevInfo);

    // ch:获取GEV相机的统计信息 | en:Get detect info of GEV camera
    int GetGevAllMatchInfo(MV_MATCH_INFO_NET_DETECT* pMatchInfoNetDetect);

    // ch:获取U3V相机的统计信息 | en:Get detect info of U3V camera
    int GetU3VAllMatchInfo(MV_MATCH_INFO_USB_DETECT* pMatchInfoUSBDetect);

    // ch:获取和设置Int型参数，如 Width和Height，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
    // en:Get Int type parameters, such as Width and Height, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int GetIntValue(IN const char* strKey, OUT MVCC_INTVALUE_EX *pIntValue);
    int SetIntValue(IN const char* strKey, IN int64_t nValue);

    // ch:获取和设置Enum型参数，如 PixelFormat，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
    // en:Get Enum type parameters, such as PixelFormat, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int GetEnumValue(IN const char* strKey, OUT MVCC_ENUMVALUE *pEnumValue);
    int SetEnumValue(IN const char* strKey, IN unsigned int nValue);
    int SetEnumValueByString(IN const char* strKey, IN const char* sValue);
    int GetEnumEntrySymbolic(IN const char* strKey, IN MVCC_ENUMENTRY* pstEnumEntry);
    int GetValidImageNum(OUT unsigned int *  pnValidImageNum) ;
    // ch:获取和设置Float型参数，如 ExposureTime和Gain，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
    // en:Get Float type parameters, such as ExposureTime and Gain, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int GetFloatValue(IN const char* strKey, OUT MVCC_FLOATVALUE *pFloatValue);
    int SetFloatValue(IN const char* strKey, IN float fValue);

    // ch:获取和设置Bool型参数，如 ReverseX，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
    // en:Get Bool type parameters, such as ReverseX, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int GetBoolValue(IN const char* strKey, OUT bool *pbValue);
    int SetBoolValue(IN const char* strKey, IN bool bValue);

    // ch:获取和设置String型参数，如 DeviceUserID，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件UserSetSave
    // en:Get String type parameters, such as DeviceUserID, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int GetStringValue(IN const char* strKey, MVCC_STRINGVALUE *pStringValue);
    int SetStringValue(IN const char* strKey, IN const char * strValue);

    // ch:执行一次Command型命令，如 UserSetSave，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
    // en:Execute Command once, such as UserSetSave, for details please refer to MvCameraNode.xlsx file under SDK installation directory
    int CommandExecute(IN const char* strKey);

    // ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
    int GetOptimalPacketSize(unsigned int* pOptimalPacketSize);

    // ch:注册消息异常回调 | en:Register Message Exception CallBack
    int RegisterExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType, void* pUser), void* pUser);

    // ch:注册单个事件回调 | en:Register Event CallBack
    int RegisterEventCallBack(const char* pEventName, void(__stdcall* cbEvent)(MV_EVENT_OUT_INFO * pEventInfo, void* pUser), void* pUser);

    // ch:强制IP | en:Force IP
    int ForceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay);

    // ch:配置IP方式 | en:IP configuration method
    int SetIpConfig(unsigned int nType);

    // ch:设置网络传输模式 | en:Set Net Transfer Mode
    int SetNetTransMode(unsigned int nType);

    // ch:像素格式转换 | en:Pixel format conversion
    int ConvertPixelType(MV_CC_PIXEL_CONVERT_PARAM* pstCvtParam);

    // ch:保存图片 | en:save image
    int SaveImage(MV_SAVE_IMAGE_PARAM_EX* pstParam);

    // ch:保存图片为文件 | en:Save the image as a file
    int SaveImageToFile(MV_SAVE_IMG_TO_FILE_PARAM* pstParam);

    // ch:绘制圆形辅助线 | en:Draw circle auxiliary line
    int DrawCircle(MVCC_CIRCLE_INFO* pCircleInfo);

    // ch:绘制线形辅助线 | en:Draw lines auxiliary line
    int DrawLines(MVCC_LINES_INFO* pLinesInfo);

    //读取buffer
    int ReadBuffer(Mat &image);

    cv::Mat GetImage();

    cv::Mat timesGetImage();

    bool isImageReadyForMain() ;
    bool takeImageForMainIfReady(cv::Mat &image);

    void setnonblocking(bool on);//初始或需要继续非阻塞取图时调用

    void deferswitchtoblockingafternextframe();//在画完框那一刻调用不是立刻阻塞

    void requestStop();

    bool isStopRequested() const;


private:

    void *m_hDevHandle;
    int consecutiveOnesCount;

    //用于保存图像的缓存
    unsigned int m_nBufSizeForSaveImage;
    atomic<bool> m_stopRequested{false};  // 添加停止标志


};

#endif//_MV_CAMERA_H_
