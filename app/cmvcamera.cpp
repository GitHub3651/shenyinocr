#include "cmvcamera.h"
#include <mutex>
#include <QDebug>
#include <chrono>
#include <thread>
static void __stdcall ImageCallBack(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser);

using namespace cv;

CMvCamera::CMvCamera()
{
    m_hDevHandle = MV_NULL;
}

CMvCamera::~CMvCamera()
{
    if (m_hDevHandle)
    {
        MV_CC_DestroyHandle(m_hDevHandle);
        m_hDevHandle = MV_NULL;
    }
}

void *CMvCamera::GetHandle() const
{
    return m_hDevHandle;
}

// ch:获取SDK版本号 | en:Get SDK Version
int CMvCamera::GetSDKVersion()
{
    return MV_CC_GetSDKVersion();
}

// ch:枚举设备 | en:Enumerate Device
int CMvCamera::EnumDevices(unsigned int nTLayerType, MV_CC_DEVICE_INFO_LIST* pstDevList)
{
    return MV_CC_EnumDevices(nTLayerType, pstDevList);
}

// ch:判断设备是否可达 | en:Is the device accessible
bool CMvCamera::IsDeviceAccessible(MV_CC_DEVICE_INFO* pstDevInfo, unsigned int nAccessMode)
{
    return MV_CC_IsDeviceAccessible(pstDevInfo, nAccessMode);
}

// ch:打开设备 | en:Open Device
int CMvCamera::Open(MV_CC_DEVICE_INFO* pstDeviceInfo)
{
    if (MV_NULL == pstDeviceInfo)
    {
        return MV_E_PARAMETER;
    }

    if (m_hDevHandle)
    {
        return MV_E_CALLORDER;
    }

    int nRet  = MV_CC_CreateHandle(&m_hDevHandle, pstDeviceInfo);
    if (MV_OK != nRet)
    {
        return nRet;
    }

    nRet = MV_CC_OpenDevice(m_hDevHandle);
    if (MV_OK != nRet)
    {
        MV_CC_DestroyHandle(m_hDevHandle);
        m_hDevHandle = MV_NULL;
    }

    return nRet;
}

// ch:关闭设备 | en:Close Device
int CMvCamera::Close()
{
    if (MV_NULL == m_hDevHandle)
    {
        return MV_E_HANDLE;
    }

    MV_CC_CloseDevice(m_hDevHandle);

    int nRet = MV_CC_DestroyHandle(m_hDevHandle);
    m_hDevHandle = MV_NULL;

    return nRet;
}

// ch:判断相机是否处于连接状态 | en:Is The Device Connected
bool CMvCamera::IsDeviceConnected()
{
    return MV_CC_IsDeviceConnected(m_hDevHandle);
}

// ch:注册图像数据回调 | en:Register Image Data CallBack
int CMvCamera::RegisterImageCallBack()
{
    return MV_CC_RegisterImageCallBackEx(m_hDevHandle, ImageCallBack, this);
}

// ch:开启抓图 | en:Start Grabbing
int CMvCamera::StartGrabbing()
{
    return MV_CC_StartGrabbing(m_hDevHandle);
}

// ch:停止抓图 | en:Stop Grabbing
int CMvCamera::StopGrabbing()
{
    return MV_CC_StopGrabbing(m_hDevHandle);
}

// ch:主动获取一帧图像数据 | en:Get one frame initiatively
int CMvCamera::GetImageBuffer(MV_FRAME_OUT* pFrame, int nMsec)
{
    return MV_CC_GetImageBuffer(m_hDevHandle, pFrame, nMsec);
}

// ch:释放图像缓存 | en:Free image buffer
int CMvCamera::FreeImageBuffer(MV_FRAME_OUT* pFrame)
{
    return MV_CC_FreeImageBuffer(m_hDevHandle, pFrame);
}

// ch:设置显示窗口句柄 | en:Set Display Window Handle
int CMvCamera::DisplayOneFrame(MV_DISPLAY_FRAME_INFO* pDisplayInfo)
{
    return MV_CC_DisplayOneFrame(m_hDevHandle, pDisplayInfo);
}

// ch:设置SDK内部图像缓存节点个数 | en:Set the number of the internal image cache nodes in SDK
int CMvCamera::SetImageNodeNum(unsigned int nNum)
{
    return MV_CC_SetImageNodeNum(m_hDevHandle, nNum);
}

// ch:获取设备信息 | en:Get device information
int CMvCamera::GetDeviceInfo(MV_CC_DEVICE_INFO* pstDevInfo)
{
    return MV_CC_GetDeviceInfo(m_hDevHandle, pstDevInfo);
}

// ch:获取GEV相机的统计信息 | en:Get detect info of GEV camera
int CMvCamera::GetGevAllMatchInfo(MV_MATCH_INFO_NET_DETECT* pMatchInfoNetDetect)
{
    if (MV_NULL == pMatchInfoNetDetect)
    {
        return MV_E_PARAMETER;
    }

    MV_CC_DEVICE_INFO stDevInfo = {0};
    GetDeviceInfo(&stDevInfo);
    if (stDevInfo.nTLayerType != MV_GIGE_DEVICE)
    {
        return MV_E_SUPPORT;
    }

    MV_ALL_MATCH_INFO struMatchInfo = {0};

    struMatchInfo.nType = MV_MATCH_TYPE_NET_DETECT;
    struMatchInfo.pInfo = pMatchInfoNetDetect;
    struMatchInfo.nInfoSize = sizeof(MV_MATCH_INFO_NET_DETECT);
    memset(struMatchInfo.pInfo, 0, sizeof(MV_MATCH_INFO_NET_DETECT));

    return MV_CC_GetAllMatchInfo(m_hDevHandle, &struMatchInfo);
}

// ch:获取U3V相机的统计信息 | en:Get detect info of U3V camera
int CMvCamera::GetU3VAllMatchInfo(MV_MATCH_INFO_USB_DETECT* pMatchInfoUSBDetect)
{
    if (MV_NULL == pMatchInfoUSBDetect)
    {
        return MV_E_PARAMETER;
    }

    MV_CC_DEVICE_INFO stDevInfo = {0};
    GetDeviceInfo(&stDevInfo);
    if (stDevInfo.nTLayerType != MV_USB_DEVICE)
    {
        return MV_E_SUPPORT;
    }

    MV_ALL_MATCH_INFO struMatchInfo = {0};

    struMatchInfo.nType = MV_MATCH_TYPE_USB_DETECT;
    struMatchInfo.pInfo = pMatchInfoUSBDetect;
    struMatchInfo.nInfoSize = sizeof(MV_MATCH_INFO_USB_DETECT);
    memset(struMatchInfo.pInfo, 0, sizeof(MV_MATCH_INFO_USB_DETECT));

    return MV_CC_GetAllMatchInfo(m_hDevHandle, &struMatchInfo);
}

// ch:获取和设置Int型参数，如 Width和Height，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
// en:Get Int type parameters, such as Width and Height, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::GetIntValue(IN const char* strKey, OUT MVCC_INTVALUE_EX *pIntValue)
{
    return MV_CC_GetIntValueEx(m_hDevHandle, strKey, pIntValue);
}

int CMvCamera::SetIntValue(IN const char* strKey, IN int64_t nValue)
{
    return MV_CC_SetIntValueEx(m_hDevHandle, strKey, nValue);
}

// ch:获取和设置Enum型参数，如 PixelFormat，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
// en:Get Enum type parameters, such as PixelFormat, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::GetEnumValue(IN const char* strKey, OUT MVCC_ENUMVALUE *pEnumValue)
{
    return MV_CC_GetEnumValue(m_hDevHandle, strKey, pEnumValue);
}

int CMvCamera::SetEnumValue(IN const char* strKey, IN unsigned int nValue)
{
    return MV_CC_SetEnumValue(m_hDevHandle, strKey, nValue);
}

int CMvCamera::SetEnumValueByString(IN const char* strKey, IN const char* sValue)
{
    return MV_CC_SetEnumValueByString(m_hDevHandle, strKey, sValue);
}

int CMvCamera::GetEnumEntrySymbolic(IN const char* strKey, IN MVCC_ENUMENTRY* pstEnumEntry)
{
    return MV_CC_GetEnumEntrySymbolic(m_hDevHandle, strKey, pstEnumEntry);
}

// ch:获取和设置Float型参数，如 ExposureTime和Gain，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
// en:Get Float type parameters, such as ExposureTime and Gain, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::GetFloatValue(IN const char* strKey, OUT MVCC_FLOATVALUE *pFloatValue)
{
    return MV_CC_GetFloatValue(m_hDevHandle, strKey, pFloatValue);
}

int CMvCamera::SetFloatValue(IN const char* strKey, IN float fValue)
{
    return MV_CC_SetFloatValue(m_hDevHandle, strKey, fValue);
}

// ch:获取和设置Bool型参数，如 ReverseX，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
// en:Get Bool type parameters, such as ReverseX, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::GetBoolValue(IN const char* strKey, OUT bool *pbValue)
{
    return MV_CC_GetBoolValue(m_hDevHandle, strKey, pbValue);
}

int CMvCamera::SetBoolValue(IN const char* strKey, IN bool bValue)
{
    return MV_CC_SetBoolValue(m_hDevHandle, strKey, bValue);
}


int CMvCamera::GetValidImageNum(unsigned int * pnValidImageNum)
{
    //return MV_CC_GetValidImageNum(m_hDevHandle,pnValidImageNum);
    return 0;
}
// ch:获取和设置String型参数，如 DeviceUserID，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件UserSetSave
// en:Get String type parameters, such as DeviceUserID, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::GetStringValue(IN const char* strKey, MVCC_STRINGVALUE *pStringValue)
{
    return MV_CC_GetStringValue(m_hDevHandle, strKey, pStringValue);
}

int CMvCamera::SetStringValue(IN const char* strKey, IN const char* strValue)
{
    return MV_CC_SetStringValue(m_hDevHandle, strKey, strValue);
}

// ch:执行一次Command型命令，如 UserSetSave，详细内容参考SDK安装目录下的 MvCameraNode.xlsx 文件
// en:Execute Command once, such as UserSetSave, for details please refer to MvCameraNode.xlsx file under SDK installation directory
int CMvCamera::CommandExecute(IN const char* strKey)
{
    return MV_CC_SetCommandValue(m_hDevHandle, strKey);
}

// ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
int CMvCamera::GetOptimalPacketSize(unsigned int* pOptimalPacketSize)
{
    if (MV_NULL == pOptimalPacketSize)
    {
        return MV_E_PARAMETER;
    }

    int nRet = MV_CC_GetOptimalPacketSize(m_hDevHandle);
    if (nRet < MV_OK)
    {
        return nRet;
    }

    *pOptimalPacketSize = (unsigned int)nRet;

    return MV_OK;
}

// ch:注册消息异常回调 | en:Register Message Exception CallBack
int CMvCamera::RegisterExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType, void* pUser),void* pUser)
{
    return MV_CC_RegisterExceptionCallBack(m_hDevHandle, cbException, pUser);
}

// ch:注册单个事件回调 | en:Register Event CallBack
int CMvCamera::RegisterEventCallBack(const char* pEventName, void(__stdcall* cbEvent)(MV_EVENT_OUT_INFO * pEventInfo, void* pUser), void* pUser)
{
    return MV_CC_RegisterEventCallBackEx(m_hDevHandle, pEventName, cbEvent, pUser);
}

// ch:强制IP | en:Force IP
int CMvCamera::ForceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay)
{
    return MV_GIGE_ForceIpEx(m_hDevHandle, nIP, nSubNetMask, nDefaultGateWay);
}

// ch:配置IP方式 | en:IP configuration method
int CMvCamera::SetIpConfig(unsigned int nType)
{
    return MV_GIGE_SetIpConfig(m_hDevHandle, nType);
}

// ch:设置网络传输模式 | en:Set Net Transfer Mode
int CMvCamera::SetNetTransMode(unsigned int nType)
{
    return MV_GIGE_SetNetTransMode(m_hDevHandle, nType);
}

// ch:像素格式转换 | en:Pixel format conversion
int CMvCamera::ConvertPixelType(MV_CC_PIXEL_CONVERT_PARAM* pstCvtParam)
{
    return MV_CC_ConvertPixelType(m_hDevHandle, pstCvtParam);
}

// ch:保存图片 | en:save image
int CMvCamera::SaveImage(MV_SAVE_IMAGE_PARAM_EX* pstParam)
{
    return MV_CC_SaveImageEx2(m_hDevHandle, pstParam);
}

// ch:保存图片为文件 | en:Save the image as a file
int CMvCamera::SaveImageToFile(MV_SAVE_IMG_TO_FILE_PARAM* pstSaveFileParam)
{
    return MV_CC_SaveImageToFile(m_hDevHandle, pstSaveFileParam);
}

// ch:绘制圆形辅助线 | en:Draw circle auxiliary line
int CMvCamera::DrawCircle(MVCC_CIRCLE_INFO* pCircleInfo)
{
    return MV_CC_DrawCircle(m_hDevHandle, pCircleInfo);
}

// ch:绘制线形辅助线 | en:Draw lines auxiliary line
int CMvCamera::DrawLines(MVCC_LINES_INFO* pLinesInfo)
{
    return MV_CC_DrawLines(m_hDevHandle, pLinesInfo);
}

//读取相机中的图像
int CMvCamera::ReadBuffer(Mat &image)
{
    Mat* getImage = new Mat();
    unsigned int nRecvBufSize = 0;
    MVCC_INTVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_INTVALUE));
    int tempValue = MV_CC_GetIntValue(m_hDevHandle, "PayloadSize", &stParam);
    if (tempValue != 0)
    {
        return -1;
    }
    nRecvBufSize = stParam.nCurValue;
    unsigned char* pDate;
    pDate=(unsigned char *)malloc(nRecvBufSize);

    MV_FRAME_OUT_INFO_EX stImageInfo = {0};
    tempValue= MV_CC_GetOneFrameTimeout(m_hDevHandle, pDate, nRecvBufSize, &stImageInfo, 700);
    if(tempValue!=0)
    {
        return -1;
    }
    m_nBufSizeForSaveImage = stImageInfo.nWidth * stImageInfo.nHeight * 3 + 2048;
    unsigned char* m_pBufForSaveImage;
    m_pBufForSaveImage = (unsigned char*)malloc(m_nBufSizeForSaveImage);


    bool isMono;
    switch (stImageInfo.enPixelType)
    {
    case PixelType_Gvsp_Mono8:
    case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono10_Packed:
    case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono12_Packed:
        isMono=true;
        break;
    default:
        isMono=false;
        break;
    }
    if(isMono)
    {
        *getImage = Mat(stImageInfo.nHeight,stImageInfo.nWidth,CV_8UC1,pDate);
        //imwrite("d:\\测试opencv_Mono.tif", image);
    }
    else
    {
        //转换图像格式为BGR8
        MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
        memset(&stConvertParam, 0, sizeof(MV_CC_PIXEL_CONVERT_PARAM));
        stConvertParam.nWidth = stImageInfo.nWidth;                 //ch:图像宽 | en:image width
        stConvertParam.nHeight = stImageInfo.nHeight;               //ch:图像高 | en:image height
        //stConvertParam.pSrcData = m_pBufForDriver;                  //ch:输入数据缓存 | en:input data buffer
        stConvertParam.pSrcData = pDate;                  //ch:输入数据缓存 | en:input data buffer
        stConvertParam.nSrcDataLen = stImageInfo.nFrameLen;         //ch:输入数据大小 | en:input data size
        stConvertParam.enSrcPixelType = stImageInfo.enPixelType;    //ch:输入像素格式 | en:input pixel format
        stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed; //ch:输出像素格式 | en:output pixel format  适用于OPENCV的图像格式
        //stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed; //ch:输出像素格式 | en:output pixel format
        stConvertParam.pDstBuffer = m_pBufForSaveImage;                    //ch:输出数据缓存 | en:output data buffer
        stConvertParam.nDstBufferSize = m_nBufSizeForSaveImage;            //ch:输出缓存大小 | en:output buffer size
        MV_CC_ConvertPixelType(m_hDevHandle, &stConvertParam);

        *getImage = Mat(stImageInfo.nHeight,stImageInfo.nWidth,CV_8UC3,m_pBufForSaveImage);
        //imwrite("d:\\测试opencv_Color.tif", image);
    }
    (*getImage).copyTo(image);
    (*getImage).release();
    free(pDate);
    free(m_pBufForSaveImage);
    return 0;
}
// 图像回调函数
void __stdcall ImageCallBack(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    if (!pFrameInfo || !pData) {
        qDebug() << "Error: Invalid frame info or data pointer.";
        return;
    }

    CMvCamera* camera = static_cast<CMvCamera*>(pUser);
    if (!camera) {
        qDebug() << "Error: Invalid camera object.";
        return;
    }

    cv::Mat image;
    void* handle = camera->GetHandle();
    if (!handle) {
        qDebug() << "Error: Invalid camera handle.";
        return;
    }


    bool isMono = false;
    switch (pFrameInfo->enPixelType) {
    case PixelType_Gvsp_Mono8:
    case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono10_Packed:
    case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono12_Packed:
        isMono = true;
        break;
    default:
        isMono = false;
        break;
    }

    try {
        if (isMono) {
            image = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC1, pData).clone();
            //qDebug() << "Mono image created.";
        } else {
            MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
            stConvertParam.nWidth = pFrameInfo->nWidth;
            stConvertParam.nHeight = pFrameInfo->nHeight;
            stConvertParam.pSrcData = pData;
            stConvertParam.nSrcDataLen = pFrameInfo->nFrameLen;
            stConvertParam.enSrcPixelType = pFrameInfo->enPixelType;
            stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
            unsigned int dstBufferSize = pFrameInfo->nWidth * pFrameInfo->nHeight * 3+2048;
            std::vector<unsigned char> dstBuffer(dstBufferSize);
            stConvertParam.pDstBuffer = dstBuffer.data();
            stConvertParam.nDstBufferSize = dstBufferSize;

            int nRet = MV_CC_ConvertPixelType(handle, &stConvertParam);
            if (nRet != MV_OK) {
                qDebug() << "Error: Pixel type conversion failed with code" << nRet;
                return;
            }

            image = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, dstBuffer.data()).clone();
        }
        std::lock_guard<std::mutex> lock(camera->m_mutex);
        camera->m_image = image;
        camera->m_frameseq++;
        if(camera->m_switchafternext.exchange(false))
        {
            camera->m_nonblocking=false;
        }
        camera->m_imageReadyForGetImage = true;
        camera->m_cvForGetImage.notify_one();
        camera->m_imageReadyForMain = true;
        camera->m_cvForMain.notify_one();
    } catch (const std::exception& e) {
        qDebug() << "Exception caught during image processing:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception caught during image processing.";
    }
}


cv::Mat CMvCamera::GetImage() {
    std::unique_lock<std::mutex> lock(m_mutex);  // 使用std::unique_lock而不是std::lock_guard
    m_cvForGetImage.wait(lock, [this] { return m_imageReadyForGetImage; });
    m_imageReadyForGetImage = false;
    return m_image.clone();  // 返回图像的副本
}

//cv::Mat CMvCamera::timesGetImage()
//{
//    while (true)
//    {
//        cv::Mat img = m_image.clone();
//        qDebug()<<"image clone success";
//        if (!img.empty())
//        {
//            return img;
//        }
//        std::this_thread::sleep_for(std::chrono::milliseconds(10));
//    }
//}



//cv::Mat CMvCamera::timesGetImage()
//{
//    std::unique_lock<std::mutex> lock(m_mutex);

//    // 等到有新图并且非空
//    m_cvForGetImage.wait(lock, [this]{
//        return m_imageReadyForGetImage && !m_image.empty();
//    });

//    // 在锁内做深拷贝，避免并发修改
//    cv::Mat img = m_image.clone();
//    qDebug() << "timesGetImage: image clone success, size="
//             << img.cols << "x" << img.rows;

//    // 消费这帧标志位
//    m_imageReadyForGetImage = false;

//    return img;  // 返回拷贝
//}

cv::Mat CMvCamera::timesGetImage()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (!m_stopRequested.load()) {
        // 检查非阻塞模式
        if (m_nonblocking && !m_image.empty()) {
            return m_image.clone();
        }

        // 等待，带超时
        bool result = m_cvForGetImage.wait_for(lock, std::chrono::milliseconds(100), [this]{
            return (m_imageReadyForGetImage && !m_image.empty()) || m_stopRequested.load();
        });

        if (m_stopRequested.load()) {
            return cv::Mat();
        }

        if (result && m_imageReadyForGetImage && !m_image.empty()) {
            cv::Mat img = m_image.clone();
            m_imageReadyForGetImage = false;
            return img;
        }
        // 如果超时，继续循环等待
    }

    return cv::Mat();  // 停止请求时返回空图像
}


bool CMvCamera::isImageReadyForMain()
//{
//    std::unique_lock<std::mutex> lock(m_mutex);
//    m_cvForMain.wait(lock, [this] { return m_imageReadyForMain; });
//    m_imageReadyForMain = false;  // 重置标志
//    return true;  // 图像准备好了
//}
{
    std::lock_guard<std::mutex> lock(m_mutex);  // 锁定互斥量
    if (m_imageReadyForMain) {
        m_imageReadyForMain = false;  // 重置标志
        return true;  // 图像已准备好
    }

    return false;  // 没有新图像
}

bool CMvCamera::takeImageForMainIfReady(cv::Mat &targetImage)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_imageReadyForMain || m_image.empty()) {
        return false;
    }

    targetImage = m_image.clone();
    m_imageReadyForMain = false;
    return !targetImage.empty();
}

void CMvCamera::setnonblocking(bool on)
{
    std::lock_guard<std::mutex> lock(m_mutex);  // 添加锁保护
    m_nonblocking = on;
    if(!on)
    {
        m_switchafternext.store(false);  // 使用原子操作
    }
}

void CMvCamera::deferswitchtoblockingafternextframe()
{
    std::lock_guard<std::mutex> lock(m_mutex);  // 添加锁保护
    m_nonblocking = true;
    m_switchafternext.store(true);  // 使用原子操作
}

void CMvCamera::requestStop()
{
    qDebug() << "CMvCamera::requestStop() called";
    m_stopRequested.store(true);

    // 唤醒所有等待的线程
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cvForGetImage.notify_all();
        m_cvForMain.notify_all();
    }
}

bool CMvCamera::isStopRequested() const
{
    return m_stopRequested.load();
}

