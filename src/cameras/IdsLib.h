#ifndef IDS_LIB_H
#define IDS_LIB_H
#ifdef WITH_IDS

#include <ids_peak_comfort_c/ids_peak_comfort_c.h>

#include <QString>

#define IDS IdsLib::instance()

class IdsLib
{
public:
    static IdsLib& instance();

    bool loaded();
    void unload();

    QString getPeakError(peak_status status);

    QString gfaGetStr(peak_camera_handle hCam, const char* prop);

    #define PROC_S(name) peak_status (__cdecl *name)
    #define PROC_A(name) peak_access_status (__cdecl *name)
    #define PROC_B(name) peak_bool (__cdecl *name)

    PROC_S(peak_Library_Init)();
    PROC_S(peak_Library_Exit)();
    PROC_S(peak_Library_GetLastError)(peak_status*, char*, size_t*);
    PROC_S(peak_CameraList_Update)(size_t*);
    PROC_S(peak_CameraList_Get)(peak_camera_descriptor*, size_t*);
    PROC_A(peak_ExposureTime_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_ExposureTime_GetRange)(peak_camera_handle, double*, double*, double*);
    PROC_S(peak_ExposureTime_Set)(peak_camera_handle, double);
    PROC_S(peak_ExposureTime_Get)(peak_camera_handle, double*);
    PROC_A(peak_FrameRate_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_FrameRate_GetRange)(peak_camera_handle, double*, double*, double*);
    PROC_S(peak_FrameRate_Set)(peak_camera_handle, double);
    PROC_S(peak_FrameRate_Get)(peak_camera_handle, double*);
    PROC_B(peak_Acquisition_IsStarted)(peak_camera_handle);
    PROC_S(peak_Acquisition_Start)(peak_camera_handle, uint32_t);
    PROC_S(peak_Acquisition_Stop)(peak_camera_handle);
    PROC_S(peak_Acquisition_WaitForFrame)(peak_camera_handle, uint32_t, peak_frame_handle*);
    PROC_S(peak_Acquisition_GetInfo)(peak_camera_handle, peak_acquisition_info*);
    PROC_S(peak_Camera_GetDescriptor)(peak_camera_id, peak_camera_descriptor*);
    PROC_S(peak_Camera_Open)(peak_camera_id, peak_camera_handle*);
    PROC_S(peak_Camera_Close)(peak_camera_handle);
    PROC_S(peak_PixelFormat_GetList)(peak_camera_handle, peak_pixel_format*, size_t*);
    PROC_S(peak_PixelFormat_GetInfo)(peak_pixel_format, peak_pixel_format_info*);
    PROC_S(peak_PixelFormat_Get)(peak_camera_handle, peak_pixel_format*);
    PROC_S(peak_PixelFormat_Set)(peak_camera_handle, peak_pixel_format);
    PROC_S(peak_ROI_Get)(peak_camera_handle, peak_roi*);
    PROC_S(peak_ROI_Set)(peak_camera_handle, peak_roi);
    PROC_S(peak_ROI_Size_GetRange)(peak_camera_handle, peak_size*, peak_size*, peak_size*);
    PROC_S(peak_Frame_Buffer_Get)(peak_frame_handle, peak_buffer*);
    PROC_S(peak_Frame_Release)(peak_camera_handle, peak_frame_handle);
    PROC_S(peak_GFA_Float_Get)(peak_camera_handle, peak_gfa_module, const char*, double*);
    PROC_S(peak_GFA_String_Get)(peak_camera_handle, peak_gfa_module, const char*, char*, size_t*);
    PROC_A(peak_GFA_Feature_GetAccessStatus)(peak_camera_handle, peak_gfa_module, const char*);
    // PROC_S(peak_IPL_Gain_GetRange)(peak_camera_handle, peak_gain_channel, double*, double*, double*);
    // PROC_S(peak_IPL_Gain_Set)(peak_camera_handle peak_gain_channel, double);
    // PROC_S(peak_IPL_Gain_Get)(peak_camera_handle, peak_gain_channel, double*);
    PROC_A(peak_Gain_GetAccessStatus)(peak_camera_handle, peak_gain_type, peak_gain_channel);
    PROC_S(peak_Gain_GetChannelList)(peak_camera_handle, peak_gain_type, peak_gain_channel*, size_t*);
    PROC_S(peak_Gain_GetRange)(peak_camera_handle, peak_gain_type, peak_gain_channel, double*, double*, double*);
    PROC_S(peak_Gain_Set)(peak_camera_handle, peak_gain_type, peak_gain_channel, double);
    PROC_S(peak_Gain_Get)(peak_camera_handle, peak_gain_type, peak_gain_channel, double*);
    PROC_A(peak_Mirror_LeftRight_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_Mirror_LeftRight_Enable)(peak_camera_handle, peak_bool);
    PROC_B(peak_Mirror_LeftRight_IsEnabled)(peak_camera_handle);
    PROC_A(peak_Mirror_UpDown_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_Mirror_UpDown_Enable)(peak_camera_handle, peak_bool);
    PROC_B(peak_Mirror_UpDown_IsEnabled)(peak_camera_handle);
    PROC_S(peak_IPL_Mirror_UpDown_Enable)(peak_camera_handle, peak_bool);
    PROC_B(peak_IPL_Mirror_UpDown_IsEnabled)(peak_camera_handle);
    PROC_S(peak_IPL_Mirror_LeftRight_Enable)(peak_camera_handle, peak_bool);
    PROC_B(peak_IPL_Mirror_LeftRight_IsEnabled)(peak_camera_handle);
    PROC_A(peak_Binning_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_Binning_FactorX_GetList)(peak_camera_handle, uint32_t*, size_t*);
    PROC_S(peak_Binning_FactorY_GetList)(peak_camera_handle, uint32_t*, size_t*);
    PROC_S(peak_Binning_Set)(peak_camera_handle, uint32_t, uint32_t);
    PROC_S(peak_Binning_Get)(peak_camera_handle, uint32_t*, uint32_t*);
    PROC_A(peak_Decimation_GetAccessStatus)(peak_camera_handle);
    PROC_S(peak_Decimation_FactorX_GetList)(peak_camera_handle, uint32_t*, size_t*);
    PROC_S(peak_Decimation_FactorY_GetList)(peak_camera_handle, uint32_t*, size_t*);
    PROC_S(peak_Decimation_Set)(peak_camera_handle, uint32_t, uint32_t);
    PROC_S(peak_Decimation_Get)(peak_camera_handle, uint32_t*, uint32_t*);

    #undef PROC_S
    #undef PROC_A
    #undef PROC_B

private:
    IdsLib();
};

#endif // WITH_IDS
#endif // IDS_LIB_H
