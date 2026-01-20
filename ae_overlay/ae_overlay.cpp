#include <stdio.h>
#include "macro.hpp"

#include "is_bad_mem_ptr.hpp"

#include "assert.hpp"

static void ShowConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen_s(reinterpret_cast<FILE**>(stdin), "conin$", "r", stdin);
        freopen_s(reinterpret_cast<FILE**>(stdout), "conout$", "w", stdout);
        freopen_s(reinterpret_cast<FILE**>(stderr), "conout$", "w", stderr);
        const HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        GetConsoleMode(hConsole, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hConsole, dwMode);
    }
}

int DllMain(
    void* _DllHandle,
    unsigned long _Reason,
    void* _Reserved)
{
    if (_Reason == DLL_PROCESS_ATTACH)
    {
        ShowConsole();
        verbose_printf("==== AE Overlay started ====\n");
    }
    return 1;  // remain loaded
}

#if defined(__AE__)

#include <AEConfig.h>
#include <entry.h>
#include <AEFX_SuiteHelper.h>
#include <PrSDKAESupport.h>
#include <AE_Effect.h>
#include <AE_EffectCBSuites.h>
#include <AE_Macros.h>
#include <String_Utils.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#include "Param_Utils.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <Windows.h>

#define DESCRIPTION "\nCopyright illusiony Software.\rSimple diff effect."

#define NAME "Diff"
#define MAJOR_VERSION 5
#define MINOR_VERSION 6
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

enum
{
    NOISE_INPUT = 0,
    NOISE_MODE,
    NOISE_COMPARE,
    NOISE_THRESHOLD,
    NOISE_AMP,
    NOISE_GAMMA,
    NOISE_NUM_PARAMS,
};

enum
{
    CompareBack = 1,
    CompareFront,
};

enum
{
    ModeAbsolute = 1,
    ModeHeat,
    ModeHorizontal,
};

extern "C"
{
DllExport
    PF_Err
    EffectMain(
        PF_Cmd cmd,
        PF_InData* in_data,
        PF_OutData* out_data,
        PF_ParamDef* params[],
        PF_LayerDef* output,
        void* extra);
}

static PF_Err
About(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_SPRINTF(out_data->return_msg,
               "%s, v%d.%d\r%s",
               NAME,
               MAJOR_VERSION,
               MINOR_VERSION,
               DESCRIPTION);

    return PF_Err_NONE;
}

static PF_Err
GlobalSetup(
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;

    out_data->my_version = PF_VERSION(MAJOR_VERSION,
                                      MINOR_VERSION,
                                      BUG_VERSION,
                                      STAGE_VERSION,
                                      BUILD_VERSION);

    out_data->out_flags = PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_NON_PARAM_VARY | PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA;

    if (in_dataP->appl_id == kAppID_Premiere)
    {
        AEFX_SuiteScoper<PF_PixelFormatSuite1> pixelFormatSuite =
            AEFX_SuiteScoper<PF_PixelFormatSuite1>(in_dataP,
                                                   kPFPixelFormatSuite,
                                                   kPFPixelFormatSuiteVersion1,
                                                   out_data);
        ERR((*pixelFormatSuite->ClearSupportedPixelFormats)(in_dataP->effect_ref));
        ERR((*pixelFormatSuite->AddSupportedPixelFormat)(
            in_dataP->effect_ref,
            PrPixelFormat_BGRA_4444_8u));
    }

    return err;
}

static PF_Err
ParamsSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP(
        "Mode",
        3,
        ModeAbsolute,
        "Absolute|Heat|Horizontal",
        NOISE_MODE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP(
        "Compare To",
        2,
        CompareBack,
        "Previous Frame|Next Frame",
        NOISE_COMPARE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER(
        "Threshold",
        0,
        255,
        0,
        255,
        0,
        NOISE_THRESHOLD);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(
        "Amplification",
        0.0,
        20.0,
        0.0,
        10.0,
        1,
        4.0,
        2,
        0,
        0,
        NOISE_AMP);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDER(
        "Gamma",
        0.1,
        5.0,
        0.1,
        3.0,
        1,
        1.0,
        2,
        0,
        0,
        NOISE_GAMMA);

    out_data->num_params = NOISE_NUM_PARAMS;

    return err;
}

#ifndef CLAMP
#define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
#endif

struct FrameCompareData
{
    uint8_t* curr_data;
    uint8_t* comp_data;
    uint8_t* out_data;
    int curr_rowbytes;
    int comp_rowbytes;
    int out_rowbytes;
    int width;
    int height;
    double threshold;
    int thresholdi;
    double amplification;
    double gamma;
};

static void ProcessMode_Absolute(FrameCompareData& data)
{
    uint8_t* curr_row = data.curr_data;
    uint8_t* comp_row = data.comp_data;
    uint8_t* out_row = data.out_data;

    for (int y = 0; y < data.height; y++)
    {
        uint8_t* curr_pixel = curr_row;
        uint8_t* comp_pixel = comp_row;
        uint8_t* out_pixel = out_row;

        for (int x = 0; x < data.width; x++)
        {
            const int curr_B = curr_pixel[1];
            const int curr_G = curr_pixel[2];
            const int curr_R = curr_pixel[3];

            const int comp_B = comp_pixel[1];
            const int comp_G = comp_pixel[2];
            const int comp_R = comp_pixel[3];

            const int diff_B = abs(curr_B - comp_B);
            const int diff_G = abs(curr_G - comp_G);
            const int diff_R = abs(curr_R - comp_R);

            const double diff_B_norm = (double)diff_B / 255.0;
            const double diff_G_norm = (double)diff_G / 255.0;
            const double diff_R_norm = (double)diff_R / 255.0;

            const double magnitude = sqrt(diff_B_norm * diff_B_norm +
                                          diff_G_norm * diff_G_norm +
                                          diff_R_norm * diff_R_norm);

            const bool changed = (magnitude > data.threshold);

            if (changed)
            {
                out_pixel[0] = 255;
                out_pixel[1] = 255;
                out_pixel[2] = 255;
                out_pixel[3] = 255;
            }
            else
            {
                out_pixel[0] = 0;
                out_pixel[1] = 0;
                out_pixel[2] = 0;
                out_pixel[3] = 255;
            }

            curr_pixel += 4;
            comp_pixel += 4;
            out_pixel += 4;
        }

        curr_row += data.curr_rowbytes;
        comp_row += data.comp_rowbytes;
        out_row += data.out_rowbytes;
    }
}

static void ProcessMode_Heat(FrameCompareData& data)
{
    uint8_t* curr_row = data.curr_data;
    uint8_t* comp_row = data.comp_data;
    uint8_t* out_row = data.out_data;

    const double amplification = data.amplification;
    const double gamma = data.gamma;

    for (int y = 0; y < data.height; y++)
    {
        uint8_t* curr_pixel = curr_row;
        uint8_t* comp_pixel = comp_row;
        uint8_t* out_pixel = out_row;

        for (int x = 0; x < data.width; x++)
        {
            const int diff_B = curr_pixel[1] - comp_pixel[1];
            const int diff_G = curr_pixel[2] - comp_pixel[2];
            const int diff_R = curr_pixel[3] - comp_pixel[3];

            const double diff_B_norm = (double)diff_B / 255.0;
            const double diff_G_norm = (double)diff_G / 255.0;
            const double diff_R_norm = (double)diff_R / 255.0;

            double magnitude = sqrt(diff_B_norm * diff_B_norm +
                                    diff_G_norm * diff_G_norm +
                                    diff_R_norm * diff_R_norm);

            magnitude = magnitude * amplification;
            magnitude = CLAMP(magnitude, 0.0, 1.0);

            magnitude = pow(magnitude, gamma);

            if (magnitude < data.threshold)
            {
                out_pixel[0] = 0;
                out_pixel[1] = 0;
                out_pixel[2] = 0;
                out_pixel[3] = 255;
            }
            else
            {
                double normalized = (magnitude - data.threshold) / (1.0 - data.threshold);
                normalized = CLAMP(normalized, 0.0, 1.0);

                out_pixel[0] = 0;

                if (normalized < 0.5)
                {
                    uint8_t val = (uint8_t)(normalized * 512.0);
                    out_pixel[1] = val;
                    out_pixel[2] = val;
                }
                else
                {
                    uint8_t val = (uint8_t)((1.0 - normalized) * 512.0);
                    out_pixel[1] = val;
                    out_pixel[2] = 255;
                }

                out_pixel[3] = 255;
            }

            curr_pixel += 4;
            comp_pixel += 4;
            out_pixel += 4;
        }

        curr_row += data.curr_rowbytes;
        comp_row += data.comp_rowbytes;
        out_row += data.out_rowbytes;
    }
}

static void ProcessMode_Horizontal(FrameCompareData& data)
{
    uint8_t* curr_row = data.curr_data;
    uint8_t* comp_row = data.comp_data;
    uint8_t* out_row = data.out_data;

    for (int y = 0; y < data.height; y++)
    {
        uint8_t* curr_pixel = curr_row;
        uint8_t* comp_pixel = comp_row;

        int lowest = INT_MAX;
        int highest = 0;
        int average = 0;

        for (int x = 0; x < data.width; x++)
        {
            const int diff_B = curr_pixel[x * 4 + 1] - comp_pixel[x * 4 + 1];
            const int diff_G = curr_pixel[x * 4 + 2] - comp_pixel[x * 4 + 2];
            const int diff_R = curr_pixel[x * 4 + 3] - comp_pixel[x * 4 + 3];

            const double diff_B_norm = (double)diff_B / 255.0;
            const double diff_G_norm = (double)diff_G / 255.0;
            const double diff_R_norm = (double)diff_R / 255.0;

            const double magnitude = sqrt(diff_B_norm * diff_B_norm +
                                          diff_G_norm * diff_G_norm +
                                          diff_R_norm * diff_R_norm);

            const int distance = (int)(magnitude * 255.0);

            if (distance < lowest)
            {
                lowest = distance;
            }
            if (distance > highest)
            {
                highest = distance;
            }
            average += distance;
        }

        average /= data.width;

        const int threshold_int = data.thresholdi;

        lowest = (lowest < threshold_int) ? 0 : lowest;
        highest = (highest < threshold_int) ? 0 : highest;
        average = (average < threshold_int) ? 0 : average;

        lowest = CLAMP(lowest, 0, 255);
        highest = CLAMP(highest, 0, 255);
        average = CLAMP(average, 0, 255);

        uint8_t* out_pixel = out_row;

        for (int x = 0; x < data.width; x++)
        {
            out_pixel[0] = average;
            out_pixel[1] = highest;
            out_pixel[2] = lowest;
            out_pixel[3] = 255;
            out_pixel += 4;
        }

        curr_row += data.curr_rowbytes;
        comp_row += data.comp_rowbytes;
        out_row += data.out_rowbytes;
    }
}

static PF_Err Render2(
    PF_InData* in_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;

    const int mode = params[NOISE_MODE]->u.pd.value;
    const int comparison = params[NOISE_COMPARE]->u.pd.value;
    const int threshold = params[NOISE_THRESHOLD]->u.sd.value;
    const double amplification = params[NOISE_AMP]->u.fs_d.value;
    const double gamma = params[NOISE_GAMMA]->u.fs_d.value;

    const int prev_time = in_data->current_time - in_data->time_step;
    const int next_time = in_data->current_time + in_data->time_step;

    if ((comparison == CompareBack && prev_time < 0) || (comparison == CompareFront && next_time > in_data->total_time))
    {
        return PF_Err_NONE;
    }

    PF_ParamDef current_param;
    AEFX_CLR_STRUCT(current_param);
    PF_ParamDef compare_param;
    AEFX_CLR_STRUCT(compare_param);

    const int compare_time = (comparison == CompareBack) ? prev_time : next_time;
    ERR(PF_CHECKOUT_PARAM(in_data, 0, in_data->current_time, in_data->time_step, in_data->time_scale, &current_param));
    if (err)
    {
        return err;
    }
    ERR(PF_CHECKOUT_PARAM(in_data, 0, compare_time, in_data->time_step, in_data->time_scale, &compare_param));
    if (err)
    {
        return err;
    }
    PF_LayerDef* compare_frame = &compare_param.u.ld;
    PF_LayerDef* current_frame = &current_param.u.ld;

    if (!err)
    {
        FrameCompareData data = {};
        data.curr_data = (uint8_t*)current_frame->data;
        data.comp_data = (uint8_t*)compare_frame->data;
        data.out_data = (uint8_t*)output->data;
        data.curr_rowbytes = current_frame->rowbytes;
        data.comp_rowbytes = compare_frame->rowbytes;
        data.out_rowbytes = output->rowbytes;
        data.width = output->width;
        data.height = output->height;
        data.threshold = (threshold > 0 ? (double)threshold / 255.0 : threshold);
        data.thresholdi = threshold;
        data.amplification = amplification;
        data.gamma = gamma;

        switch (mode)
        {
            case ModeAbsolute:
            {
                ProcessMode_Absolute(data);
                break;
            }
            case ModeHeat:
            {
                ProcessMode_Heat(data);
                break;
            }
            case ModeHorizontal:
            {
                ProcessMode_Horizontal(data);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    ERR(PF_CHECKIN_PARAM(in_data, &current_param));
    ERR(PF_CHECKIN_PARAM(in_data, &compare_param));
    return err;
}

static PF_Err
Render(
    PF_InData* in_dataP,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;

    // Do high-bit depth rendering in Premiere Pro
    if (in_dataP->appl_id == kAppID_Premiere)
    {
        // Get the Premiere pixel format suite
        AEFX_SuiteScoper<PF_PixelFormatSuite1> pixelFormatSuite =
            AEFX_SuiteScoper<PF_PixelFormatSuite1>(in_dataP,
                                                   kPFPixelFormatSuite,
                                                   kPFPixelFormatSuiteVersion1,
                                                   out_data);

        PrPixelFormat destinationPixelFormat = PrPixelFormat_BGRA_4444_8u;

        pixelFormatSuite->GetPixelFormat(output, &destinationPixelFormat);

        if (destinationPixelFormat == PrPixelFormat_BGRA_4444_8u)
        {
            // scanline render, too complicated to setup for small things
            // like graph lines, etc.
            // and text bounds would need to be calculated for it too
            // better to run on entire frame
            if (1)
            {
                Render2(in_dataP, params, output);
            }
        }
        else
        {
            //	Return error, because we don't know how to handle the specified pixel type
            return PF_Err_UNRECOGNIZED_PARAM_TYPE;
        }
    }
    return err;
}

extern "C" DllExport
    PF_Err
    PluginDataEntryFunction2(
        PF_PluginDataPtr inPtr,
        PF_PluginDataCB2 inPluginDataCallBackPtr,
        SPBasicSuite* inSPBasicSuitePtr,
        const char* inHostName,
        const char* inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;

    result = PF_REGISTER_EFFECT_EXT2(
        inPtr,
        inPluginDataCallBackPtr,
        "Diff",            // Name
        "Diff",            // Match Name
        "Diff",            // Category
        AE_RESERVED_INFO,  // Reserved Info
        "EffectMain",      // Entry point
        "");               // support URL

    return result;
}

static PF_Err
SequenceSetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    if (out_data->sequence_data)
    {
        PF_DISPOSE_HANDLE(out_data->sequence_data);
    }
    return PF_Err_NONE;
}

static PF_Err
SequenceResetup(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    if (!in_data->sequence_data)
    {
        return SequenceSetup(in_data, out_data, params, output);
    }
    return PF_Err_NONE;
}

static PF_Err
SequenceSetdown(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    return PF_Err_NONE;
}

static PF_Err
SequenceFlatten(
    PF_InData* in_data,
    PF_OutData* out_data)
{
    PF_Err err = PF_Err_NONE;
    return err;
}

static PF_Err
GetFlattenedSequenceData(
    PF_InData* in_data,
    PF_OutData* out_data)
{
    PF_Err err = PF_Err_NONE;
    return err;
}

static PF_Err
PopDialog(
    PF_InData* in_data,
    PF_OutData* out_data,
    PF_ParamDef* params[],
    PF_LayerDef* output)
{
    PF_Err err = PF_Err_NONE;
    return err;
}

static const char* PF_CmdName(const PF_Cmd cmd)
{
    switch (cmd)
    {
        ENUM_STR(PF_Cmd_ABOUT);
        ENUM_STR(PF_Cmd_GLOBAL_SETUP);
        ENUM_STR(PF_Cmd_UNUSED_0);
        ENUM_STR(PF_Cmd_GLOBAL_SETDOWN);
        ENUM_STR(PF_Cmd_PARAMS_SETUP);
        ENUM_STR(PF_Cmd_SEQUENCE_SETUP);
        ENUM_STR(PF_Cmd_SEQUENCE_RESETUP);
        ENUM_STR(PF_Cmd_SEQUENCE_FLATTEN);
        ENUM_STR(PF_Cmd_SEQUENCE_SETDOWN);
        ENUM_STR(PF_Cmd_DO_DIALOG);
        ENUM_STR(PF_Cmd_FRAME_SETUP);
        ENUM_STR(PF_Cmd_RENDER);
        ENUM_STR(PF_Cmd_FRAME_SETDOWN);
        ENUM_STR(PF_Cmd_USER_CHANGED_PARAM);
        ENUM_STR(PF_Cmd_UPDATE_PARAMS_UI);
        ENUM_STR(PF_Cmd_EVENT);
        ENUM_STR(PF_Cmd_GET_EXTERNAL_DEPENDENCIES);
        ENUM_STR(PF_Cmd_COMPLETELY_GENERAL);
        ENUM_STR(PF_Cmd_QUERY_DYNAMIC_FLAGS);
        ENUM_STR(PF_Cmd_AUDIO_RENDER);
        ENUM_STR(PF_Cmd_AUDIO_SETUP);
        ENUM_STR(PF_Cmd_AUDIO_SETDOWN);
        ENUM_STR(PF_Cmd_ARBITRARY_CALLBACK);
        ENUM_STR(PF_Cmd_SMART_PRE_RENDER);
        ENUM_STR(PF_Cmd_SMART_RENDER);
        ENUM_STR(PF_Cmd_RESERVED1);
        ENUM_STR(PF_Cmd_RESERVED2);
        ENUM_STR(PF_Cmd_RESERVED3);
        ENUM_STR(PF_Cmd_GET_FLATTENED_SEQUENCE_DATA);
        ENUM_STR(PF_Cmd_TRANSLATE_PARAMS_TO_PREFS);
        ENUM_STR(PF_Cmd_RESERVED4);
        ENUM_STR(PF_Cmd_SMART_RENDER_GPU);
        ENUM_STR(PF_Cmd_GPU_DEVICE_SETUP);
        ENUM_STR(PF_Cmd_GPU_DEVICE_SETDOWN);
        ENUM_STR(PF_Cmd_NUM_CMDS);
        default:
            break;
    }
    return "Unknown cmd";
}

extern "C" DllExport
    PF_Err
    EffectMain(
        PF_Cmd cmd,
        PF_InData* in_dataP,
        PF_OutData* out_data,
        PF_ParamDef* params[],
        PF_LayerDef* output,
        void* extra)
{
    PF_Err err = PF_Err_NONE;

    try
    {
        if (cmd != PF_Cmd_RENDER && cmd != PF_Cmd_FRAME_SETUP && cmd != PF_Cmd_FRAME_SETDOWN)
        {
            debug_printf("cmd %d %s\n", cmd, PF_CmdName(cmd));
        }
        switch (cmd)
        {
            case PF_Cmd_ABOUT:
                err = About(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_SEQUENCE_SETUP:
                err = SequenceSetup(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_SEQUENCE_SETDOWN:
                err = SequenceSetdown(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_SEQUENCE_RESETUP:
                err = SequenceResetup(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
                err = GetFlattenedSequenceData(in_dataP, out_data);
                break;
            case PF_Cmd_SEQUENCE_FLATTEN:
                err = SequenceFlatten(in_dataP, out_data);
                break;
            case PF_Cmd_DO_DIALOG:
                err = PopDialog(in_dataP, out_data, params, output);
                break;
            case PF_Cmd_RENDER:
                err = Render(in_dataP, out_data, params, output);
                break;
        }
    }
    catch (PF_Err& thrown_err)
    {
        // Never EVER throw exceptions into AE.
        err = thrown_err;
    }
    return err;
}

#endif
