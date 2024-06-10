#include <stdio.h>
#include <stdlib.h>

#include "ids_peak_comfort_c.h"

#define CHECK_ERR(msg) \
    if (PEAK_ERROR(res)) { \
        printf("ERR: %s (code=%#06x)\n", msg, res); \
        goto stop; \
    }

#define REMOTE PEAK_GFA_MODULE_REMOTE_DEVICE
#define LOCAL PEAK_GFA_MODULE_LOCAL_DEVICE

#define GFA_GET_FLOAT(prop) \
    if (PEAK_IS_READABLE(peak_GFA_Feature_GetAccessStatus(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop))) { \
        double value; \
        res = peak_GFA_Float_Get(hCam, PEAK_GFA_MODULE_REMOTE_DEVICE, prop, &value); \
        if (PEAK_SUCCESS(res)) \
            printf("%s: %f\n", prop, value); \
        else printf("Failed to read %s (code=%#06x)\n", prop, res); \
    } else printf("%s is not readable\n", prop);

#define GFA_GET_STRING(module, prop) \
    if (PEAK_IS_READABLE(peak_GFA_Feature_GetAccessStatus(hCam, module, prop))) { \
        size_t size; \
        res = peak_GFA_String_Get(hCam, module, prop, NULL, &size); \
        if (PEAK_SUCCESS(res)) { \
            char *buf = (char*)malloc(size); \
            res = peak_GFA_String_Get(hCam, module, prop, buf, &size); \
            if (PEAK_SUCCESS(res)) \
                printf("%s: %s\n", prop, buf); \
            else printf("Failed to read %s (code=%#06x)\n", prop, res); \
            free(buf); \
        } else printf("Failed to read %s size (code=%#06x)\n", prop, res); \
    } else printf("%s is not readable\n", prop);

#define GFA_SET_STRING(module, prop, value) \
    if (PEAK_IS_WRITEABLE(peak_GFA_Feature_GetAccessStatus(hCam, module, prop))) { \
        res = peak_GFA_String_Set(hCam, module, prop, value); \
        if (PEAK_SUCCESS(res)) \
            printf("%s: %s (WRITTEN)\n", prop, value); \
        else printf("Failed to write %s size (code=%#06x)\n", prop, res); \
    } else printf("%s is not writeable\n", prop);

int main() {
    peak_status res = peak_Library_Init();
    CHECK_ERR("Init library")

    res = peak_CameraList_Update(NULL);
    CHECK_ERR("Update camera list")

    peak_camera_handle hCam = PEAK_INVALID_HANDLE;
    res = peak_Camera_OpenFirstAvailable(&hCam);
    CHECK_ERR("Open first available camera")

    peak_camera_descriptor descr;
    res = peak_Camera_GetDescriptor(peak_Camera_ID_FromHandle(hCam), &descr);
    CHECK_ERR("Get camera descriptor")
    printf("Camera: %s (SN: %s, ID: %llu)\n", descr.modelName, descr.serialNumber, descr.cameraID);

    GFA_GET_FLOAT("SensorPixelWidth")
    GFA_GET_FLOAT("SensorPixelHeight")
    GFA_GET_STRING(REMOTE, "DeviceModelName")
    GFA_GET_STRING(REMOTE, "DeviceVendorName")
    GFA_GET_STRING(REMOTE, "DeviceSerialNumber")
    GFA_GET_STRING(REMOTE, "DeviceUserDefinedName")
    GFA_GET_STRING(LOCAL, "DeviceUserDefinedName")
    GFA_GET_STRING(LOCAL, "DeviceID")
    GFA_GET_STRING(REMOTE, "DeviceUserID")
    GFA_SET_STRING(REMOTE, "DeviceUserID", "Test camera")

stop:
    if (hCam != PEAK_INVALID_HANDLE) {
        if (peak_Acquisition_IsStarted(hCam))
            peak_Acquisition_Stop(hCam);
        peak_Camera_Close(hCam);
    }
    peak_Library_Exit();
    return 0;
}
