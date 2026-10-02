import 'dart:ffi' as ffi;
import 'dart:io';
import 'package:ffi/ffi.dart';

// --- Typedefs cho các hàm C-ABI của J2ME Core ---
typedef J2meCreateC = ffi.Pointer<ffi.Void> Function(ffi.Pointer<Utf8> storageDir);
typedef J2meCreateDart = ffi.Pointer<ffi.Void> Function(ffi.Pointer<Utf8> storageDir);

typedef J2meDestroyC = ffi.Void Function(ffi.Pointer<ffi.Void> inst);
typedef J2meDestroyDart = void Function(ffi.Pointer<ffi.Void> inst);

typedef J2meLoadJarC = ffi.Bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> bytes, ffi.Size size);
typedef J2meLoadJarDart = bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> bytes, int size);

typedef J2meLoadJarFileC = ffi.Bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<Utf8> filePath);
typedef J2meLoadJarFileDart = bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<Utf8> filePath);

typedef J2meActionC = ffi.Void Function(ffi.Pointer<ffi.Void> inst);
typedef J2meActionDart = void Function(ffi.Pointer<ffi.Void> inst);

typedef J2meLockFBC = ffi.Pointer<ffi.Uint32> Function(
    ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Int32> w, ffi.Pointer<ffi.Int32> h, ffi.Pointer<ffi.Bool> dirty);
typedef J2meLockFBDart = ffi.Pointer<ffi.Uint32> Function(
    ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Int32> w, ffi.Pointer<ffi.Int32> h, ffi.Pointer<ffi.Bool> dirty);

typedef J2meCopyFrameC = ffi.Int32 Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> dst, ffi.Size cap,
    ffi.Int32 scale, ffi.Bool force, ffi.Pointer<ffi.Int32> w, ffi.Pointer<ffi.Int32> h);
typedef J2meCopyFrameDart = int Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> dst, int cap,
    int scale, bool force, ffi.Pointer<ffi.Int32> w, ffi.Pointer<ffi.Int32> h);

typedef J2meScreenSerialC = ffi.Int32 Function(ffi.Pointer<ffi.Void> inst);
typedef J2meScreenSerialDart = int Function(ffi.Pointer<ffi.Void> inst);
typedef J2meScreenGetC = ffi.Bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<Utf8> out, ffi.Size maxLen);
typedef J2meScreenGetDart = bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<Utf8> out, int maxLen);
typedef J2meScreenSubmitC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 cmdIndex, ffi.Pointer<Utf8> texts);
typedef J2meSendKeyIdxC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 index);
typedef J2meSendKeyIdxDart = void Function(ffi.Pointer<ffi.Void> inst, int index);
typedef J2meScreenSubmitDart = void Function(ffi.Pointer<ffi.Void> inst, int cmdIndex, ffi.Pointer<Utf8> texts);
typedef J2meSendKeyC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 keyCode, ffi.Bool pressed);
typedef J2meSendKeyDart = void Function(ffi.Pointer<ffi.Void> inst, int keyCode, bool pressed);

typedef J2meSendTouchC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 action, ffi.Int32 x, ffi.Int32 y);
typedef J2meSendTouchDart = void Function(ffi.Pointer<ffi.Void> inst, int action, int x, int y);

typedef J2meSetDimsC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 w, ffi.Int32 h);
typedef J2meSetDimsDart = void Function(ffi.Pointer<ffi.Void> inst, int w, int h);

typedef J2meGetStringC = ffi.Pointer<Utf8> Function(ffi.Pointer<ffi.Void> inst);
typedef J2meGetStringDart = ffi.Pointer<Utf8> Function(ffi.Pointer<ffi.Void> inst);

typedef J2meGetFpsC = ffi.Int32 Function(ffi.Pointer<ffi.Void> inst);
typedef J2meGetFpsDart = int Function(ffi.Pointer<ffi.Void> inst);

typedef J2meSetFpsC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 fps);
typedef J2meSetFpsDart = void Function(ffi.Pointer<ffi.Void> inst, int fps);

// Audio
typedef J2mePlayToneC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 note, ffi.Int32 durationMs, ffi.Int32 volume);
typedef J2mePlayToneDart = void Function(ffi.Pointer<ffi.Void> inst, int note, int durationMs, int volume);

typedef J2mePlayMidiC = ffi.Bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> midiBytes, ffi.Size length);
typedef J2mePlayMidiDart = bool Function(ffi.Pointer<ffi.Void> inst, ffi.Pointer<ffi.Uint8> midiBytes, int length);

typedef J2meSetVolumeC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 volume);
typedef J2meSetVolumeDart = void Function(ffi.Pointer<ffi.Void> inst, int volume);

// OEM & Device
typedef J2meDeviceVibrateC = ffi.Void Function(ffi.Pointer<ffi.Void> inst, ffi.Int32 durationMs, ffi.Int32 frequency);
typedef J2meDeviceVibrateDart = void Function(ffi.Pointer<ffi.Void> inst, int durationMs, int frequency);

// Keymap
typedef J2meKeymapSetLayoutC = ffi.Void Function(ffi.Int32 layoutType);
typedef J2meKeymapSetLayoutDart = void Function(int layoutType);

typedef J2meKeymapGetLayoutC = ffi.Int32 Function();
typedef J2meKeymapGetLayoutDart = int Function();

// Profile & Settings
typedef J2meProfileCreateDefaultC = ffi.UintPtr Function();
typedef J2meProfileCreateDefaultDart = int Function();

typedef J2meProfileLoadC = ffi.UintPtr Function(ffi.Pointer<Utf8> jsonStr);
typedef J2meProfileLoadDart = int Function(ffi.Pointer<Utf8> jsonStr);

typedef J2meProfileSaveC = ffi.Bool Function(ffi.UintPtr handle, ffi.Pointer<Utf8> outBuf, ffi.Size maxLen);
typedef J2meProfileSaveDart = bool Function(int handle, ffi.Pointer<Utf8> outBuf, int maxLen);

typedef J2meProfileGetIntC = ffi.Int32 Function(ffi.UintPtr handle, ffi.Pointer<Utf8> key, ffi.Int32 defaultVal);
typedef J2meProfileGetIntDart = int Function(int handle, ffi.Pointer<Utf8> key, int defaultVal);

typedef J2meProfileSetIntC = ffi.Void Function(ffi.UintPtr handle, ffi.Pointer<Utf8> key, ffi.Int32 value);
typedef J2meProfileSetIntDart = void Function(int handle, ffi.Pointer<Utf8> key, int value);

typedef J2meProfileGetStringC = ffi.Bool Function(ffi.UintPtr handle, ffi.Pointer<Utf8> key, ffi.Pointer<Utf8> outBuf, ffi.Size maxLen);
typedef J2meProfileGetStringDart = bool Function(int handle, ffi.Pointer<Utf8> key, ffi.Pointer<Utf8> outBuf, int maxLen);

typedef J2meProfileSetStringC = ffi.Void Function(ffi.UintPtr handle, ffi.Pointer<Utf8> key, ffi.Pointer<Utf8> value);
typedef J2meProfileSetStringDart = void Function(int handle, ffi.Pointer<Utf8> key, ffi.Pointer<Utf8> value);

typedef J2meProfileDestroyC = ffi.Void Function(ffi.UintPtr handle);
typedef J2meProfileDestroyDart = void Function(int handle);

typedef J2meApplyProfileC = ffi.Bool Function(ffi.Pointer<ffi.Void> inst, ffi.UintPtr handle);
typedef J2meApplyProfileDart = bool Function(ffi.Pointer<ffi.Void> inst, int handle);

typedef J2meGetPresetResolutionCountC = ffi.Size Function();
typedef J2meGetPresetResolutionCountDart = int Function();

typedef J2meGetPresetResolutionC = ffi.Bool Function(
    ffi.Size index, ffi.Pointer<ffi.Int32> outW, ffi.Pointer<ffi.Int32> outH, ffi.Pointer<Utf8> outName, ffi.Size maxLen);
typedef J2meGetPresetResolutionDart = bool Function(
    int index, ffi.Pointer<ffi.Int32> outW, ffi.Pointer<ffi.Int32> outH, ffi.Pointer<Utf8> outName, int maxLen);

// Java CLDC VM
typedef J2meVmCreateC = ffi.UintPtr Function();
typedef J2meVmCreateDart = int Function();

typedef J2meVmLoadClassC = ffi.Bool Function(ffi.UintPtr vmHandle, ffi.Pointer<ffi.Uint8> classBytes, ffi.Size size);
typedef J2meVmLoadClassDart = bool Function(int vmHandle, ffi.Pointer<ffi.Uint8> classBytes, int size);

typedef J2meVmInvokeStaticIntC = ffi.Int32 Function(
    ffi.UintPtr vmHandle, ffi.Pointer<Utf8> className, ffi.Pointer<Utf8> methodName, ffi.Pointer<Utf8> desc);
typedef J2meVmInvokeStaticIntDart = int Function(
    int vmHandle, ffi.Pointer<Utf8> className, ffi.Pointer<Utf8> methodName, ffi.Pointer<Utf8> desc);

typedef J2meVmDestroyC = ffi.Void Function(ffi.UintPtr vmHandle);
typedef J2meVmDestroyDart = void Function(int vmHandle);

// Section 16: TiledLayer & LayerManager
typedef J2meTiledLayerCreateC = ffi.UintPtr Function(ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meTiledLayerCreateDart = int Function(int, int, int, int, int, int);

typedef J2meTiledLayerSetCellC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meTiledLayerSetCellDart = void Function(int, int, int, int);

typedef J2meTiledLayerGetCellC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32, ffi.Int32);
typedef J2meTiledLayerGetCellDart = int Function(int, int, int);

typedef J2meTiledLayerFillCellsC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meTiledLayerFillCellsDart = void Function(int, int, int, int, int, int);

typedef J2meTiledLayerCreateAnimC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meTiledLayerCreateAnimDart = int Function(int, int);

typedef J2meTiledLayerSetAnimC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32);
typedef J2meTiledLayerSetAnimDart = void Function(int, int, int);

typedef J2meTiledLayerGetAnimC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meTiledLayerGetAnimDart = int Function(int, int);

typedef J2meTiledLayerDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meTiledLayerDestroyDart = void Function(int);

typedef J2meLayerManagerCreateC = ffi.UintPtr Function();
typedef J2meLayerManagerCreateDart = int Function();

typedef J2meLayerManagerAppendC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meLayerManagerAppendDart = void Function(int, int);

typedef J2meLayerManagerInsertC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr, ffi.Int32);
typedef J2meLayerManagerInsertDart = void Function(int, int, int);

typedef J2meLayerManagerGetSizeC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meLayerManagerGetSizeDart = int Function(int);

typedef J2meLayerManagerRemoveC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meLayerManagerRemoveDart = void Function(int, int);

typedef J2meLayerManagerSetViewC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meLayerManagerSetViewDart = void Function(int, int, int, int, int);

typedef J2meLayerManagerPaintC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Int32);
typedef J2meLayerManagerPaintDart = void Function(int, ffi.Pointer<ffi.Void>, int, int);

typedef J2meLayerManagerDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meLayerManagerDestroyDart = void Function(int);

// Section 17: Speed Multiplier & Screenshot PNG
typedef J2meSetSpeedMultC = ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Int32);
typedef J2meSetSpeedMultDart = void Function(ffi.Pointer<ffi.Void>, int);

typedef J2meGetSpeedMultC = ffi.Int32 Function(ffi.Pointer<ffi.Void>);
typedef J2meGetSpeedMultDart = int Function(ffi.Pointer<ffi.Void>);

typedef J2meCaptureScreenshotC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>);
typedef J2meCaptureScreenshotDart = bool Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>);

// Section 18: GCF Datagram UDP Networking
typedef J2meDatagramCreateC = ffi.UintPtr Function(ffi.Int32, ffi.Pointer<Utf8>);
typedef J2meDatagramCreateDart = int Function(int, ffi.Pointer<Utf8>);

typedef J2meDatagramGetAddressC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meDatagramGetAddressDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meDatagramSetAddressC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2meDatagramSetAddressDart = void Function(int, ffi.Pointer<Utf8>);

typedef J2meDatagramGetLengthC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meDatagramGetLengthDart = int Function(int);

typedef J2meDatagramSetLengthC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meDatagramSetLengthDart = void Function(int, int);

typedef J2meDatagramGetOffsetC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meDatagramGetOffsetDart = int Function(int);

typedef J2meDatagramGetDataC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meDatagramGetDataDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meDatagramSetDataC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Int32, ffi.Int32);
typedef J2meDatagramSetDataDart = void Function(int, ffi.Pointer<ffi.Uint8>, int, int);

typedef J2meDatagramResetC = ffi.Void Function(ffi.UintPtr);
typedef J2meDatagramResetDart = void Function(int);

typedef J2meDatagramWriteC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meDatagramWriteDart = void Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meDatagramReadC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meDatagramReadDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meDatagramDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meDatagramDestroyDart = void Function(int);

typedef J2meDatagramConnOpenC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meDatagramConnOpenDart = int Function(ffi.Pointer<Utf8>);

typedef J2meDatagramConnSendC = ffi.Bool Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meDatagramConnSendDart = bool Function(int, int);

typedef J2meDatagramConnReceiveC = ffi.Bool Function(ffi.UintPtr, ffi.UintPtr, ffi.Int32);
typedef J2meDatagramConnReceiveDart = bool Function(int, int, int);

typedef J2meDatagramConnGetPortC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meDatagramConnGetPortDart = int Function(int);

typedef J2meDatagramConnCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meDatagramConnCloseDart = void Function(int);

// Section 19: M3G Animation & MorphingMesh
typedef J2meM3gVBCreateC = ffi.UintPtr Function();
typedef J2meM3gVBCreateDart = int Function();

typedef J2meM3gVBSetCoordsC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>, ffi.Int32);
typedef J2meM3gVBSetCoordsDart = void Function(int, ffi.Pointer<ffi.Float>, int);

typedef J2meM3gVBDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gVBDestroyDart = void Function(int);

typedef J2meM3gKeyframeSeqCreateC = ffi.UintPtr Function(ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meM3gKeyframeSeqCreateDart = int Function(int, int, int);

typedef J2meM3gKeyframeSeqSetDurationC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gKeyframeSeqSetDurationDart = void Function(int, int);

typedef J2meM3gKeyframeSeqGetDurationC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meM3gKeyframeSeqGetDurationDart = int Function(int);

typedef J2meM3gKeyframeSeqSetRepeatModeC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gKeyframeSeqSetRepeatModeDart = void Function(int, int);

typedef J2meM3gKeyframeSeqGetRepeatModeC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meM3gKeyframeSeqGetRepeatModeDart = int Function(int);

typedef J2meM3gKeyframeSeqSetKeyframeC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Pointer<ffi.Float>);
typedef J2meM3gKeyframeSeqSetKeyframeDart = void Function(int, int, int, ffi.Pointer<ffi.Float>);

typedef J2meM3gKeyframeSeqSampleC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Float>, ffi.Int32);
typedef J2meM3gKeyframeSeqSampleDart = bool Function(int, int, ffi.Pointer<ffi.Float>, int);

typedef J2meM3gKeyframeSeqDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gKeyframeSeqDestroyDart = void Function(int);

typedef J2meM3gAnimCtrlCreateC = ffi.UintPtr Function();
typedef J2meM3gAnimCtrlCreateDart = int Function();

typedef J2meM3gAnimCtrlSetActiveIntervalC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32);
typedef J2meM3gAnimCtrlSetActiveIntervalDart = void Function(int, int, int);

typedef J2meM3gAnimCtrlGetActiveIntervalC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meM3gAnimCtrlGetActiveIntervalDart = int Function(int);

typedef J2meM3gAnimCtrlSetSpeedC = ffi.Void Function(ffi.UintPtr, ffi.Float, ffi.Int32);
typedef J2meM3gAnimCtrlSetSpeedDart = void Function(int, double, int);

typedef J2meM3gAnimCtrlGetSpeedC = ffi.Float Function(ffi.UintPtr);
typedef J2meM3gAnimCtrlGetSpeedDart = double Function(int);

typedef J2meM3gAnimCtrlSetPositionC = ffi.Void Function(ffi.UintPtr, ffi.Float, ffi.Int32);
typedef J2meM3gAnimCtrlSetPositionDart = void Function(int, double, int);

typedef J2meM3gAnimCtrlGetPositionC = ffi.Float Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gAnimCtrlGetPositionDart = double Function(int, int);

typedef J2meM3gAnimCtrlSetWeightC = ffi.Void Function(ffi.UintPtr, ffi.Float);
typedef J2meM3gAnimCtrlSetWeightDart = void Function(int, double);

typedef J2meM3gAnimCtrlGetWeightC = ffi.Float Function(ffi.UintPtr);
typedef J2meM3gAnimCtrlGetWeightDart = double Function(int);

typedef J2meM3gAnimCtrlDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gAnimCtrlDestroyDart = void Function(int);

typedef J2meM3gAnimTrackCreateC = ffi.UintPtr Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gAnimTrackCreateDart = int Function(int, int);

typedef J2meM3gAnimTrackSetCtrlC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gAnimTrackSetCtrlDart = void Function(int, int);

typedef J2meM3gAnimTrackGetCtrlC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2meM3gAnimTrackGetCtrlDart = int Function(int);

typedef J2meM3gAnimTrackGetSeqC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2meM3gAnimTrackGetSeqDart = int Function(int);

typedef J2meM3gAnimTrackGetPropC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meM3gAnimTrackGetPropDart = int Function(int);

typedef J2meM3gAnimTrackDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gAnimTrackDestroyDart = void Function(int);

typedef J2meM3gMorphMeshCreateC = ffi.UintPtr Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.UintPtr>);
typedef J2meM3gMorphMeshCreateDart = int Function(int, int, ffi.Pointer<ffi.UintPtr>);

typedef J2meM3gMorphMeshSetWeightsC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>, ffi.Int32);
typedef J2meM3gMorphMeshSetWeightsDart = void Function(int, ffi.Pointer<ffi.Float>, int);

typedef J2meM3gMorphMeshGetWeightsC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>, ffi.Int32);
typedef J2meM3gMorphMeshGetWeightsDart = void Function(int, ffi.Pointer<ffi.Float>, int);

typedef J2meM3gMorphMeshMorphC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gMorphMeshMorphDart = void Function(int);

typedef J2meM3gMeshAddAnimTrackC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gMeshAddAnimTrackDart = void Function(int, int);

typedef J2meM3gMeshAnimateC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gMeshAnimateDart = int Function(int, int);

typedef J2meM3gMeshGetPosC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>);
typedef J2meM3gMeshGetPosDart = void Function(int, ffi.Pointer<ffi.Float>);

typedef J2meM3gMeshGetVertPosC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Float>);
typedef J2meM3gMeshGetVertPosDart = void Function(int, int, ffi.Pointer<ffi.Float>);

typedef J2meM3gMeshDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gMeshDestroyDart = void Function(int);

// Section 20: M3G SkinnedMesh & Bone Skeleton
typedef J2meM3gNodeCreateC = ffi.UintPtr Function();
typedef J2meM3gNodeCreateDart = int Function();

typedef J2meM3gNodeSetTranslationC = ffi.Void Function(ffi.UintPtr, ffi.Float, ffi.Float, ffi.Float);
typedef J2meM3gNodeSetTranslationDart = void Function(int, double, double, double);

typedef J2meM3gNodeGetTranslationC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>);
typedef J2meM3gNodeGetTranslationDart = void Function(int, ffi.Pointer<ffi.Float>);

typedef J2meM3gNodeSetOrientationC = ffi.Void Function(ffi.UintPtr, ffi.Float, ffi.Float, ffi.Float, ffi.Float);
typedef J2meM3gNodeSetOrientationDart = void Function(int, double, double, double, double);

typedef J2meM3gNodeGetOrientationC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>);
typedef J2meM3gNodeGetOrientationDart = void Function(int, ffi.Pointer<ffi.Float>);

typedef J2meM3gNodeSetScaleC = ffi.Void Function(ffi.UintPtr, ffi.Float, ffi.Float, ffi.Float);
typedef J2meM3gNodeSetScaleDart = void Function(int, double, double, double);

typedef J2meM3gNodeGetScaleC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>);
typedef J2meM3gNodeGetScaleDart = void Function(int, ffi.Pointer<ffi.Float>);

typedef J2meM3gNodeGetGlobalTransformC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Float>);
typedef J2meM3gNodeGetGlobalTransformDart = void Function(int, ffi.Pointer<ffi.Float>);

typedef J2meM3gNodeAddAnimTrackC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gNodeAddAnimTrackDart = void Function(int, int);

typedef J2meM3gNodeAnimateC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gNodeAnimateDart = int Function(int, int);

typedef J2meM3gNodeDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gNodeDestroyDart = void Function(int);

typedef J2meM3gGroupCreateC = ffi.UintPtr Function();
typedef J2meM3gGroupCreateDart = int Function();

typedef J2meM3gGroupAddChildC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gGroupAddChildDart = void Function(int, int);

typedef J2meM3gGroupRemoveChildC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gGroupRemoveChildDart = void Function(int, int);

typedef J2meM3gGroupGetChildCountC = ffi.Size Function(ffi.UintPtr);
typedef J2meM3gGroupGetChildCountDart = int Function(int);

typedef J2meM3gGroupGetChildC = ffi.UintPtr Function(ffi.UintPtr, ffi.Size);
typedef J2meM3gGroupGetChildDart = int Function(int, int);

typedef J2meM3gGroupAnimateC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gGroupAnimateDart = int Function(int, int);

typedef J2meM3gGroupDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gGroupDestroyDart = void Function(int);

typedef J2meM3gSkinnedMeshCreateC = ffi.UintPtr Function(ffi.UintPtr, ffi.UintPtr);
typedef J2meM3gSkinnedMeshCreateDart = int Function(int, int);

typedef J2meM3gSkinnedMeshAddTransformC = ffi.Void Function(ffi.UintPtr, ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meM3gSkinnedMeshAddTransformDart = void Function(int, int, int, int, int);

typedef J2meM3gSkinnedMeshGetBoneCountC = ffi.Size Function(ffi.UintPtr);
typedef J2meM3gSkinnedMeshGetBoneCountDart = int Function(int);

typedef J2meM3gSkinnedMeshGetBoneC = ffi.UintPtr Function(ffi.UintPtr, ffi.Size);
typedef J2meM3gSkinnedMeshGetBoneDart = int Function(int, int);

typedef J2meM3gSkinnedMeshGetSkeletonC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2meM3gSkinnedMeshGetSkeletonDart = int Function(int);

typedef J2meM3gSkinnedMeshSkinC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gSkinnedMeshSkinDart = void Function(int);

typedef J2meM3gSkinnedMeshAnimateC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meM3gSkinnedMeshAnimateDart = int Function(int, int);

typedef J2meM3gSkinnedMeshGetVertPosC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Float>);
typedef J2meM3gSkinnedMeshGetVertPosDart = void Function(int, int, ffi.Pointer<ffi.Float>);

typedef J2meM3gSkinnedMeshGetVertNormC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Float>);
typedef J2meM3gSkinnedMeshGetVertNormDart = void Function(int, int, ffi.Pointer<ffi.Float>);

typedef J2meM3gSkinnedMeshDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gSkinnedMeshDestroyDart = void Function(int);

// Section 21: App Management & Installer
final class J2meAppItemInfoFFI extends ffi.Struct {
  @ffi.Int32()
  external int id;

  @ffi.Array(128)
  external ffi.Array<ffi.Uint8> path;

  @ffi.Array(128)
  external ffi.Array<ffi.Uint8> title;

  @ffi.Array(128)
  external ffi.Array<ffi.Uint8> author;

  @ffi.Array(32)
  external ffi.Array<ffi.Uint8> version;

  @ffi.Array(256)
  external ffi.Array<ffi.Uint8> imagePath;

  @ffi.Int64()
  external int installedTimestamp;

  @ffi.Int64()
  external int lastPlayedTimestamp;

  @ffi.Int32()
  external int playCount;
}

typedef J2meAppRepoGetCountC = ffi.Size Function(ffi.Pointer<ffi.Void>);
typedef J2meAppRepoGetCountDart = int Function(ffi.Pointer<ffi.Void>);

typedef J2meAppRepoGetItemC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Size, ffi.Pointer<J2meAppItemInfoFFI>);
typedef J2meAppRepoGetItemDart = bool Function(ffi.Pointer<ffi.Void>, int, ffi.Pointer<J2meAppItemInfoFFI>);

typedef J2meAppRepoFindByIdC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Pointer<J2meAppItemInfoFFI>);
typedef J2meAppRepoFindByIdDart = bool Function(ffi.Pointer<ffi.Void>, int, ffi.Pointer<J2meAppItemInfoFFI>);

typedef J2meAppRepoFindByPathC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Pointer<J2meAppItemInfoFFI>);
typedef J2meAppRepoFindByPathDart = bool Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Pointer<J2meAppItemInfoFFI>);

typedef J2meAppRepoDeleteC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Int32);
typedef J2meAppRepoDeleteDart = bool Function(ffi.Pointer<ffi.Void>, int);

typedef J2meAppInstallerCheckJarC = ffi.Int32 Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>,
    ffi.Pointer<Utf8>, ffi.Size,
    ffi.Pointer<Utf8>, ffi.Size,
    ffi.Pointer<Utf8>, ffi.Size);
typedef J2meAppInstallerCheckJarDart = int Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>,
    ffi.Pointer<Utf8>, int,
    ffi.Pointer<Utf8>, int,
    ffi.Pointer<Utf8>, int);

typedef J2meAppInstallerInstallC = ffi.Int32 Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Bool,
    ffi.Pointer<Utf8>, ffi.Size);
typedef J2meAppInstallerInstallDart = int Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, bool,
    ffi.Pointer<Utf8>, int);

typedef J2meAppInstallerUninstallC = ffi.Bool Function(
    ffi.Pointer<ffi.Void>, ffi.Int32,
    ffi.Pointer<Utf8>, ffi.Size);
typedef J2meAppInstallerUninstallDart = bool Function(
    ffi.Pointer<ffi.Void>, int,
    ffi.Pointer<Utf8>, int);

typedef J2meAppLaunchC = ffi.Bool Function(ffi.Pointer<ffi.Void>, ffi.Int32);
typedef J2meAppLaunchDart = bool Function(ffi.Pointer<ffi.Void>, int);

typedef J2meAppSpawnC = ffi.Pointer<ffi.Void> Function(ffi.Pointer<ffi.Void>, ffi.Int32, ffi.Int32);
typedef J2meAppSpawnDart = ffi.Pointer<ffi.Void> Function(ffi.Pointer<ffi.Void>, int, int);

// Section 22: JSR-82 Mobile Bluetooth & RFCOMM/L2CAP Multiplayer
typedef J2meBtIsPowerOnC = ffi.Bool Function();
typedef J2meBtIsPowerOnDart = bool Function();

typedef J2meBtSetPowerOnC = ffi.Void Function(ffi.Bool);
typedef J2meBtSetPowerOnDart = void Function(bool);

typedef J2meBtGetLocalAddressC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Size);
typedef J2meBtGetLocalAddressDart = void Function(ffi.Pointer<Utf8>, int);

typedef J2meBtSetLocalAddressC = ffi.Void Function(ffi.Pointer<Utf8>);
typedef J2meBtSetLocalAddressDart = void Function(ffi.Pointer<Utf8>);

typedef J2meBtGetLocalNameC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Size);
typedef J2meBtGetLocalNameDart = void Function(ffi.Pointer<Utf8>, int);

typedef J2meBtSetLocalNameC = ffi.Void Function(ffi.Pointer<Utf8>);
typedef J2meBtSetLocalNameDart = void Function(ffi.Pointer<Utf8>);

typedef J2meBtGetDiscoverableC = ffi.Int32 Function();
typedef J2meBtGetDiscoverableDart = int Function();

typedef J2meBtSetDiscoverableC = ffi.Bool Function(ffi.Int32);
typedef J2meBtSetDiscoverableDart = bool Function(int);

typedef J2meBtGetPropertyC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meBtGetPropertyDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2meBtOpenBtsppServerC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meBtOpenBtsppServerDart = int Function(ffi.Pointer<Utf8>);

typedef J2meBtBtsppAcceptC = ffi.UintPtr Function(ffi.UintPtr, ffi.Int32);
typedef J2meBtBtsppAcceptDart = int Function(int, int);

typedef J2meBtBtsppServerCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meBtBtsppServerCloseDart = void Function(int);

typedef J2meBtOpenBtsppClientC = ffi.UintPtr Function(ffi.Pointer<Utf8>, ffi.Int32);
typedef J2meBtOpenBtsppClientDart = int Function(ffi.Pointer<Utf8>, int);

typedef J2meBtBtsppReadC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size, ffi.Int32);
typedef J2meBtBtsppReadDart = int Function(int, ffi.Pointer<ffi.Uint8>, int, int);

typedef J2meBtBtsppWriteC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meBtBtsppWriteDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meBtBtsppAvailableC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meBtBtsppAvailableDart = int Function(int);

typedef J2meBtBtsppCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meBtBtsppCloseDart = void Function(int);

typedef J2meBtOpenBtl2capServerC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meBtOpenBtl2capServerDart = int Function(ffi.Pointer<Utf8>);

typedef J2meBtBtl2capAcceptC = ffi.UintPtr Function(ffi.UintPtr, ffi.Int32);
typedef J2meBtBtl2capAcceptDart = int Function(int, int);

typedef J2meBtBtl2capServerCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meBtBtl2capServerCloseDart = void Function(int);

typedef J2meBtOpenBtl2capClientC = ffi.UintPtr Function(ffi.Pointer<Utf8>, ffi.Int32);
typedef J2meBtOpenBtl2capClientDart = int Function(ffi.Pointer<Utf8>, int);

typedef J2meBtBtl2capSendC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meBtBtl2capSendDart = bool Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meBtBtl2capReceiveC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size, ffi.Int32);
typedef J2meBtBtl2capReceiveDart = int Function(int, ffi.Pointer<ffi.Uint8>, int, int);

typedef J2meBtBtl2capReadyC = ffi.Bool Function(ffi.UintPtr);
typedef J2meBtBtl2capReadyDart = bool Function(int);

typedef J2meBtBtl2capCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meBtBtl2capCloseDart = void Function(int);

// Section 23: Vodafone VSCL & Carrier OEM Extensions
typedef J2meVodafoneSpriteCreateC = ffi.UintPtr Function(ffi.Int32, ffi.Int32);
typedef J2meVodafoneSpriteCreateDart = int Function(int, int);

typedef J2meVodafoneSpriteDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meVodafoneSpriteDestroyDart = void Function(int);

typedef J2meVodafoneSpriteSetPaletteC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Uint32);
typedef J2meVodafoneSpriteSetPaletteDart = void Function(int, int, int);

typedef J2meVodafoneSpriteGetPaletteC = ffi.Uint32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meVodafoneSpriteGetPaletteDart = int Function(int, int);

typedef J2meVodafoneSpriteSetPatternC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meVodafoneSpriteSetPatternDart = void Function(int, int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meVodafoneSpriteCreateCommandC = ffi.Int16 Function(ffi.UintPtr, ffi.Int32, ffi.Bool, ffi.Int32, ffi.Bool, ffi.Bool, ffi.Int32);
typedef J2meVodafoneSpriteCreateCommandDart = int Function(int, int, bool, int, bool, bool, int);

typedef J2meVodafoneSpriteCreateFbC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32);
typedef J2meVodafoneSpriteCreateFbDart = void Function(int, int, int);

typedef J2meVodafoneSpriteDisposeFbC = ffi.Void Function(ffi.UintPtr);
typedef J2meVodafoneSpriteDisposeFbDart = void Function(int);

typedef J2meVodafoneSpriteDrawCharC = ffi.Void Function(ffi.UintPtr, ffi.Int16, ffi.Int16, ffi.Int16);
typedef J2meVodafoneSpriteDrawCharDart = void Function(int, int, int, int);

typedef J2meVodafoneSpriteCopyAreaC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meVodafoneSpriteCopyAreaDart = void Function(int, int, int, int, int, int, int);

typedef J2meVodafoneSpriteDrawFbC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Uint32>, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meVodafoneSpriteDrawFbDart = void Function(int, ffi.Pointer<ffi.Uint32>, int, int, int, int);

typedef J2meVodafoneSpriteGetFbC = ffi.Pointer<ffi.Uint32> Function(ffi.UintPtr, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2meVodafoneSpriteGetFbDart = ffi.Pointer<ffi.Uint32> Function(int, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

typedef J2meVodafoneDeviceGetStateC = ffi.Int32 Function(ffi.Int32);
typedef J2meVodafoneDeviceGetStateDart = int Function(int);

typedef J2meVodafoneDeviceIsActiveC = ffi.Bool Function(ffi.Int32);
typedef J2meVodafoneDeviceIsActiveDart = bool Function(int);

typedef J2meVodafoneDeviceSetActiveC = ffi.Bool Function(ffi.Int32, ffi.Bool);
typedef J2meVodafoneDeviceSetActiveDart = bool Function(int, bool);

typedef J2meVodafoneDeviceBlinkC = ffi.Void Function(ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meVodafoneDeviceBlinkDart = void Function(int, int, int);

typedef J2meVodafoneDeviceGetKeystatesC = ffi.Uint32 Function();
typedef J2meVodafoneDeviceGetKeystatesDart = int Function();

typedef J2meVodafoneDeviceSetKeystatesMaskC = ffi.Void Function(ffi.Uint32);
typedef J2meVodafoneDeviceSetKeystatesMaskDart = void Function(int);

typedef J2meVodafoneDeviceKeyEventC = ffi.Void Function(ffi.Int32, ffi.Bool);
typedef J2meVodafoneDeviceKeyEventDart = void Function(int, bool);

typedef J2meVodafoneEncodeOffscreenC = ffi.Int32 Function(ffi.Pointer<ffi.Uint32>, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meVodafoneEncodeOffscreenDart = int Function(ffi.Pointer<ffi.Uint32>, int, int, int, int, int, int, int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meCarrierKddiGetKeyStateC = ffi.Int32 Function(ffi.Bool);
typedef J2meCarrierKddiGetKeyStateDart = int Function(bool);

typedef J2meCarrierMotorolaFunlightSetColorC = ffi.Void Function(ffi.Int32, ffi.Uint32);
typedef J2meCarrierMotorolaFunlightSetColorDart = void Function(int, int);

typedef J2meCarrierMotorolaFunlightGetColorC = ffi.Uint32 Function(ffi.Int32);
typedef J2meCarrierMotorolaFunlightGetColorDart = int Function(int);

typedef J2meCarrierSonyAccelSetC = ffi.Void Function(ffi.Float, ffi.Float, ffi.Float);
typedef J2meCarrierSonyAccelSetDart = void Function(double, double, double);

typedef J2meCarrierSonyAccelGetC = ffi.Void Function(ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);
typedef J2meCarrierSonyAccelGetDart = void Function(ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);

typedef J2meCarrierSprintPlayClipC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Int32);
typedef J2meCarrierSprintPlayClipDart = void Function(ffi.Pointer<Utf8>, int);

typedef J2meCarrierSprintStopC = ffi.Void Function();
typedef J2meCarrierSprintStopDart = void Function();

typedef J2meCarrierSprintIsPlayingC = ffi.Bool Function();
typedef J2meCarrierSprintIsPlayingDart = bool Function();

// Section 24: JSR-179 Mobile Location API
typedef J2meLocationProviderGetStateC = ffi.Int32 Function();
typedef J2meLocationProviderGetStateDart = int Function();

typedef J2meLocationProviderSetStateC = ffi.Void Function(ffi.Int32);
typedef J2meLocationProviderSetStateDart = void Function(int);

typedef J2meLocationProviderUpdateHostLocationC = ffi.Void Function(ffi.Double, ffi.Double, ffi.Float, ffi.Float, ffi.Float, ffi.Float, ffi.Float);
typedef J2meLocationProviderUpdateHostLocationDart = void Function(double, double, double, double, double, double, double);

typedef J2meLocationProviderGetLastKnownC = ffi.Bool Function(ffi.Pointer<ffi.Double>, ffi.Pointer<ffi.Double>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Int64>);
typedef J2meLocationProviderGetLastKnownDart = bool Function(ffi.Pointer<ffi.Double>, ffi.Pointer<ffi.Double>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Int64>);

typedef J2meLocationProviderGetNmeaC = ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Size);
typedef J2meLocationProviderGetNmeaDart = int Function(ffi.Pointer<Utf8>, int);

typedef J2meLocationCoordinatesDistanceC = ffi.Float Function(ffi.Double, ffi.Double, ffi.Double, ffi.Double);
typedef J2meLocationCoordinatesDistanceDart = double Function(double, double, double, double);

typedef J2meLocationCoordinatesAzimuthC = ffi.Float Function(ffi.Double, ffi.Double, ffi.Double, ffi.Double);
typedef J2meLocationCoordinatesAzimuthDart = double Function(double, double, double, double);

typedef J2meLocationCoordinatesConvertToStringC = ffi.Bool Function(ffi.Double, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meLocationCoordinatesConvertToStringDart = bool Function(double, int, ffi.Pointer<Utf8>, int);

typedef J2meLocationCoordinatesConvertFromStringC = ffi.Double Function(ffi.Pointer<Utf8>);
typedef J2meLocationCoordinatesConvertFromStringDart = double Function(ffi.Pointer<Utf8>);

typedef J2meLocationOrientationGetC = ffi.Void Function(ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Bool>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);
typedef J2meLocationOrientationGetDart = void Function(ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Bool>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);

typedef J2meLocationOrientationSetC = ffi.Void Function(ffi.Float, ffi.Bool, ffi.Float, ffi.Float);
typedef J2meLocationOrientationSetDart = void Function(double, bool, double, double);

typedef J2meLocationLandmarkStoreCreateC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2meLocationLandmarkStoreCreateDart = bool Function(ffi.Pointer<Utf8>);

typedef J2meLocationLandmarkStoreDeleteC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2meLocationLandmarkStoreDeleteDart = bool Function(ffi.Pointer<Utf8>);

typedef J2meLocationLandmarkStoreAddLandmarkC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Double, ffi.Double, ffi.Float, ffi.Pointer<Utf8>);
typedef J2meLocationLandmarkStoreAddLandmarkDart = void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, double, double, double, ffi.Pointer<Utf8>);

typedef J2meLocationLandmarkStoreGetCountC = ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2meLocationLandmarkStoreGetCountDart = int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

// Section 25: JSR-256 Mobile Sensor API
typedef J2meSensorGetCountC = ffi.Int32 Function();
typedef J2meSensorGetCountDart = int Function();

typedef J2meSensorGetUrlC = ffi.Bool Function(ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meSensorGetUrlDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meSensorGetQuantityC = ffi.Bool Function(ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meSensorGetQuantityDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meSensorGetContextTypeC = ffi.Bool Function(ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meSensorGetContextTypeDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meSensorFindC = ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<ffi.Int32>, ffi.Int32);
typedef J2meSensorFindDart = int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<ffi.Int32>, int);

typedef J2meSensorOpenC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meSensorOpenDart = int Function(ffi.Pointer<Utf8>);

typedef J2meSensorCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meSensorCloseDart = void Function(int);

typedef J2meSensorGetStateC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meSensorGetStateDart = int Function(int);

typedef J2meSensorGetChannelCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meSensorGetChannelCountDart = int Function(int);

typedef J2meSensorGetChannelNameC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meSensorGetChannelNameDart = bool Function(int, int, ffi.Pointer<Utf8>, int);

typedef J2meSensorGetDataC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<ffi.Double>, ffi.Int32);
typedef J2meSensorGetDataDart = int Function(int, int, ffi.Pointer<ffi.Double>, int);

typedef J2meSensorUpdate3DC = ffi.Void Function(ffi.Double, ffi.Double, ffi.Double);
typedef J2meSensorUpdate3DDart = void Function(double, double, double);

typedef J2meSensorUpdate1DC = ffi.Void Function(ffi.Double);
typedef J2meSensorUpdate1DDart = void Function(double);

// Section 26: JSR-75 PIM (Personal Information Management)
typedef J2mePimInitC = ffi.Void Function(ffi.Pointer<Utf8>);
typedef J2mePimInitDart = void Function(ffi.Pointer<Utf8>);

typedef J2mePimListCountC = ffi.Int32 Function(ffi.Int32);
typedef J2mePimListCountDart = int Function(int);

typedef J2mePimListGetNameC = ffi.Bool Function(ffi.Int32, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePimListGetNameDart = bool Function(int, int, ffi.Pointer<Utf8>, int);

typedef J2mePimOpenListC = ffi.UintPtr Function(ffi.Int32, ffi.Int32, ffi.Pointer<Utf8>);
typedef J2mePimOpenListDart = int Function(int, int, ffi.Pointer<Utf8>);

typedef J2mePimCloseListC = ffi.Void Function(ffi.UintPtr);
typedef J2mePimCloseListDart = void Function(int);

typedef J2mePimListGetItemCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2mePimListGetItemCountDart = int Function(int);

typedef J2mePimListGetItemC = ffi.UintPtr Function(ffi.UintPtr, ffi.Int32);
typedef J2mePimListGetItemDart = int Function(int, int);

typedef J2mePimListRemoveItemC = ffi.Bool Function(ffi.UintPtr, ffi.UintPtr);
typedef J2mePimListRemoveItemDart = bool Function(int, int);

typedef J2mePimContactCreateC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2mePimContactCreateDart = int Function(int);

typedef J2mePimContactSetNameC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2mePimContactSetNameDart = bool Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2mePimContactGetFormattedNameC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePimContactGetFormattedNameDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2mePimContactAddTelC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>);
typedef J2mePimContactAddTelDart = bool Function(int, int, ffi.Pointer<Utf8>);

typedef J2mePimContactGetTelCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2mePimContactGetTelCountDart = int Function(int);

typedef J2mePimContactGetTelC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<ffi.Int32>);
typedef J2mePimContactGetTelDart = bool Function(int, int, ffi.Pointer<Utf8>, int, ffi.Pointer<ffi.Int32>);

typedef J2mePimContactAddEmailC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>);
typedef J2mePimContactAddEmailDart = bool Function(int, int, ffi.Pointer<Utf8>);

typedef J2mePimContactGetEmailCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2mePimContactGetEmailCountDart = int Function(int);

typedef J2mePimContactGetEmailC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<ffi.Int32>);
typedef J2mePimContactGetEmailDart = bool Function(int, int, ffi.Pointer<Utf8>, int, ffi.Pointer<ffi.Int32>);

typedef J2mePimContactSetAddressC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2mePimContactSetAddressDart = bool Function(int, int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2mePimEventCreateC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2mePimEventCreateDart = int Function(int);

typedef J2mePimEventSetDetailsC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Int64, ffi.Int64, ffi.Int32);
typedef J2mePimEventSetDetailsDart = bool Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int, int, int);

typedef J2mePimEventGetDetailsC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<ffi.Int64>, ffi.Pointer<ffi.Int64>);
typedef J2mePimEventGetDetailsDart = bool Function(int, ffi.Pointer<Utf8>, int, ffi.Pointer<Utf8>, int, ffi.Pointer<ffi.Int64>, ffi.Pointer<ffi.Int64>);

typedef J2mePimEventSetRepeatRuleC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int64);
typedef J2mePimEventSetRepeatRuleDart = bool Function(int, int, int, int, int);

typedef J2mePimTodoCreateC = ffi.UintPtr Function(ffi.UintPtr);
typedef J2mePimTodoCreateDart = int Function(int);

typedef J2mePimTodoSetDetailsC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Int32, ffi.Bool, ffi.Int64, ffi.Int64);
typedef J2mePimTodoSetDetailsDart = bool Function(int, ffi.Pointer<Utf8>, int, bool, int, int);

typedef J2mePimTodoGetDetailsC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Bool>, ffi.Pointer<ffi.Int64>);
typedef J2mePimTodoGetDetailsDart = bool Function(int, ffi.Pointer<Utf8>, int, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Bool>, ffi.Pointer<ffi.Int64>);

typedef J2mePimItemCommitC = ffi.Void Function(ffi.UintPtr);
typedef J2mePimItemCommitDart = void Function(int);

typedef J2mePimItemAddCategoryC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2mePimItemAddCategoryDart = void Function(int, ffi.Pointer<Utf8>);

typedef J2mePimItemGetCategoryCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2mePimItemGetCategoryCountDart = int Function(int);

typedef J2mePimItemGetCategoryC = ffi.Bool Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePimItemGetCategoryDart = bool Function(int, int, ffi.Pointer<Utf8>, int);

typedef J2mePimExportSerialC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePimExportSerialDart = int Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2mePimImportSerialC = ffi.UintPtr Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2mePimImportSerialDart = int Function(int, ffi.Pointer<Utf8>);

typedef J2mePimSaveAllC = ffi.Void Function();
typedef J2mePimSaveAllDart = void Function();

// Section 27: JSR-234 AMMS (Advanced Multimedia Supplements)
typedef J2meAmmsSpectatorSetLocationC = ffi.Void Function(ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meAmmsSpectatorSetLocationDart = void Function(int, int, int);

typedef J2meAmmsSpectatorGetLocationC = ffi.Void Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2meAmmsSpectatorGetLocationDart = void Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

typedef J2meAmmsSpectatorSetOrientationC = ffi.Void Function(ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meAmmsSpectatorSetOrientationDart = void Function(int, int, int);

typedef J2meAmmsSpectatorGetOrientationC = ffi.Void Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2meAmmsSpectatorGetOrientationDart = void Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

typedef J2meAmmsSoundSourceCreateC = ffi.UintPtr Function();
typedef J2meAmmsSoundSourceCreateDart = int Function();

typedef J2meAmmsSoundSourceDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meAmmsSoundSourceDestroyDart = void Function(int);

typedef J2meAmmsSoundSourceSetLocationC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meAmmsSoundSourceSetLocationDart = void Function(int, int, int, int);

typedef J2meAmmsSoundSourceGetLocationC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2meAmmsSoundSourceGetLocationDart = void Function(int, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

typedef J2meAmmsSoundSourceSetVelocityC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meAmmsSoundSourceSetVelocityDart = void Function(int, int, int, int);

typedef J2meAmmsSoundSourceSetAttenuationC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32, ffi.Bool, ffi.Int32);
typedef J2meAmmsSoundSourceSetAttenuationDart = void Function(int, int, int, bool, int);

typedef J2meAmmsSoundSourceEvaluateC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);
typedef J2meAmmsSoundSourceEvaluateDart = bool Function(int, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);

typedef J2meAmmsEffectModuleCreateC = ffi.UintPtr Function();
typedef J2meAmmsEffectModuleCreateDart = int Function();

typedef J2meAmmsEffectModuleDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meAmmsEffectModuleDestroyDart = void Function(int);

typedef J2meAmmsReverbSetLevelC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meAmmsReverbSetLevelDart = void Function(int, int);

typedef J2meAmmsReverbGetLevelC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meAmmsReverbGetLevelDart = int Function(int);

typedef J2meAmmsReverbSetPresetC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2meAmmsReverbSetPresetDart = void Function(int, ffi.Pointer<Utf8>);

typedef J2meAmmsReverbGetPresetC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meAmmsReverbGetPresetDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meAmmsEqualizerSetBandLevelC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Int32);
typedef J2meAmmsEqualizerSetBandLevelDart = void Function(int, int, int);

typedef J2meAmmsEqualizerGetBandLevelC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meAmmsEqualizerGetBandLevelDart = int Function(int, int);

typedef J2meAmmsEqualizerGetBandCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meAmmsEqualizerGetBandCountDart = int Function(int);

typedef J2meAmmsEqualizerSetPresetC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2meAmmsEqualizerSetPresetDart = void Function(int, ffi.Pointer<Utf8>);

typedef J2meAmmsPanSetC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meAmmsPanSetDart = void Function(int, int);

typedef J2meAmmsPanGetC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meAmmsPanGetDart = int Function(int);

typedef J2meAmmsCameraSetRotationC = ffi.Void Function(ffi.Int32);
typedef J2meAmmsCameraSetRotationDart = void Function(int);

typedef J2meAmmsCameraGetRotationC = ffi.Int32 Function();
typedef J2meAmmsCameraGetRotationDart = int Function();

typedef J2meAmmsCameraSetExposureModeC = ffi.Void Function(ffi.Pointer<Utf8>);
typedef J2meAmmsCameraSetExposureModeDart = void Function(ffi.Pointer<Utf8>);

typedef J2meAmmsCameraGetExposureModeC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Size);
typedef J2meAmmsCameraGetExposureModeDart = bool Function(ffi.Pointer<Utf8>, int);

typedef J2meAmmsFlashSetModeC = ffi.Void Function(ffi.Int32);
typedef J2meAmmsFlashSetModeDart = void Function(int);

typedef J2meAmmsFlashGetModeC = ffi.Int32 Function();
typedef J2meAmmsFlashGetModeDart = int Function();

typedef J2meAmmsZoomSetDigitalC = ffi.Void Function(ffi.Int32);
typedef J2meAmmsZoomSetDigitalDart = void Function(int);

typedef J2meAmmsZoomGetDigitalC = ffi.Int32 Function();
typedef J2meAmmsZoomGetDigitalDart = int Function();

typedef J2meAmmsImageTransformSetCropC = ffi.Void Function(ffi.Int32, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meAmmsImageTransformSetCropDart = void Function(int, int, int, int);

typedef J2meAmmsImageTransformSetTargetC = ffi.Void Function(ffi.Int32, ffi.Int32);
typedef J2meAmmsImageTransformSetTargetDart = void Function(int, int);

// Section 28: MIDP 2.0 PushRegistry & CommConnection
typedef J2mePushRegisterConnectionC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2mePushRegisterConnectionDart = void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2mePushUnregisterConnectionC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2mePushUnregisterConnectionDart = bool Function(ffi.Pointer<Utf8>);

typedef J2mePushListConnectionsC = ffi.Int32 Function(ffi.Bool, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePushListConnectionsDart = int Function(bool, ffi.Pointer<Utf8>, int);

typedef J2mePushGetMIDletC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePushGetMIDletDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2mePushGetFilterC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePushGetFilterDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2mePushRegisterAlarmC = ffi.Int64 Function(ffi.Pointer<Utf8>, ffi.Int64);
typedef J2mePushRegisterAlarmDart = int Function(ffi.Pointer<Utf8>, int);

typedef J2mePushNotifyInboundC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2mePushNotifyInboundDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2mePushCheckAlarmsC = ffi.Int32 Function(ffi.Int64, ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePushCheckAlarmsDart = int Function(int, ffi.Pointer<Utf8>, int);

typedef J2mePushSaveC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2mePushSaveDart = bool Function(ffi.Pointer<Utf8>);

typedef J2mePushLoadC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2mePushLoadDart = bool Function(ffi.Pointer<Utf8>);

typedef J2meCommOpenC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meCommOpenDart = int Function(ffi.Pointer<Utf8>);

typedef J2meCommCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meCommCloseDart = void Function(int);

typedef J2meCommGetBaudRateC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meCommGetBaudRateDart = int Function(int);

typedef J2meCommSetBaudRateC = ffi.Int32 Function(ffi.UintPtr, ffi.Int32);
typedef J2meCommSetBaudRateDart = int Function(int, int);

typedef J2meCommWriteC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meCommWriteDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meCommReadC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meCommReadDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meCommAvailableC = ffi.Size Function(ffi.UintPtr);
typedef J2meCommAvailableDart = int Function(int);

// Section 29: PKI Security, SSL & HTTPS
typedef J2meCertCreateC = ffi.UintPtr Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Int64, ffi.Int64, ffi.Pointer<Utf8>);
typedef J2meCertCreateDart = int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int, int, ffi.Pointer<Utf8>);

typedef J2meCertDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meCertDestroyDart = void Function(int);

typedef J2meCertGetStringC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meCertGetStringDart = bool Function(int, ffi.Pointer<Utf8>, int);

typedef J2meCertGetTimeC = ffi.Int64 Function(ffi.UintPtr);
typedef J2meCertGetTimeDart = int Function(int);

typedef J2meCertValidateC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Int64);
typedef J2meCertValidateDart = int Function(int, ffi.Pointer<Utf8>, int);

typedef J2meSslOpenC = ffi.UintPtr Function(ffi.Pointer<Utf8>, ffi.Bool);
typedef J2meSslOpenDart = int Function(ffi.Pointer<Utf8>, bool);

typedef J2meSslCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meSslCloseDart = void Function(int);

typedef J2meSslIsOpenC = ffi.Bool Function(ffi.UintPtr);
typedef J2meSslIsOpenDart = bool Function(int);

typedef J2meSslGetPortC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meSslGetPortDart = int Function(int);

typedef J2meSslWriteC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meSslWriteDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meSslReadC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meSslReadDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meSslAvailableC = ffi.Size Function(ffi.UintPtr);
typedef J2meSslAvailableDart = int Function(int);

typedef J2meSslFeedInputC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meSslFeedInputDart = void Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meSslGetSecurityInfoC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<Utf8>, ffi.Size, ffi.Pointer<ffi.UintPtr>);
typedef J2meSslGetSecurityInfoDart = bool Function(int, ffi.Pointer<Utf8>, int, ffi.Pointer<Utf8>, int, ffi.Pointer<Utf8>, int, ffi.Pointer<ffi.UintPtr>);

typedef J2meHttpsOpenC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meHttpsOpenDart = int Function(ffi.Pointer<Utf8>);

typedef J2meHttpsCloseC = ffi.Void Function(ffi.UintPtr);
typedef J2meHttpsCloseDart = void Function(int);

typedef J2meHttpsIsOpenC = ffi.Bool Function(ffi.UintPtr);
typedef J2meHttpsIsOpenDart = bool Function(int);

typedef J2meHttpsGetHeaderFieldC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meHttpsGetHeaderFieldDart = bool Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2meHttpsSetResponseHeaderC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2meHttpsSetResponseHeaderDart = void Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2meHttpsSetMethodC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2meHttpsSetMethodDart = void Function(int, ffi.Pointer<Utf8>);

typedef J2meHttpsSetRequestPropC = ffi.Void Function(ffi.UintPtr, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2meHttpsSetRequestPropDart = void Function(int, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2meHttpsGetCodeC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meHttpsGetCodeDart = int Function(int);

typedef J2meHttpsGetPortC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meHttpsGetPortDart = int Function(int);

typedef J2meHttpsFeedRespC = ffi.Void Function(ffi.UintPtr, ffi.Int32, ffi.Pointer<Utf8>);
typedef J2meHttpsFeedRespDart = void Function(int, int, ffi.Pointer<Utf8>);

typedef J2meHttpsReadC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meHttpsReadDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

typedef J2meHttpsWriteC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meHttpsWriteDart = int Function(int, ffi.Pointer<ffi.Uint8>, int);

// Section 30: 3D Binary Asset Loaders (M3G & Micro3D)
typedef J2me3dIdentifyFormatC = ffi.Int32 Function(ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2me3dIdentifyFormatDart = int Function(ffi.Pointer<ffi.Uint8>, int);

typedef J2meM3gLoadMemoryC = ffi.UintPtr Function(ffi.Pointer<ffi.Uint8>, ffi.Size, ffi.Pointer<ffi.Size>, ffi.Pointer<ffi.Size>);
typedef J2meM3gLoadMemoryDart = int Function(ffi.Pointer<ffi.Uint8>, int, ffi.Pointer<ffi.Size>, ffi.Pointer<ffi.Size>);

typedef J2meM3gSceneDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meM3gSceneDestroyDart = void Function(int);

typedef J2meMicro3dLoadFigureC = ffi.UintPtr Function(ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meMicro3dLoadFigureDart = int Function(ffi.Pointer<ffi.Uint8>, int);

typedef J2meMicro3dFigureDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meMicro3dFigureDestroyDart = void Function(int);

typedef J2meMicro3dFigureGetCountsC = ffi.Bool Function(ffi.UintPtr, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2meMicro3dFigureGetCountsDart = bool Function(int, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

typedef J2meMicro3dLoadActionTableC = ffi.UintPtr Function(ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meMicro3dLoadActionTableDart = int Function(ffi.Pointer<ffi.Uint8>, int);

typedef J2meMicro3dActionTableDestroyC = ffi.Void Function(ffi.UintPtr);
typedef J2meMicro3dActionTableDestroyDart = void Function(int);

typedef J2meMicro3dActionTableGetCountC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meMicro3dActionTableGetCountDart = int Function(int);

// Section 31: LCDUI Font Engine, System Properties & PCM WAV Player
// Font Engine
typedef J2meFontGetDefaultC = ffi.UintPtr Function();
typedef J2meFontGetDefaultDart = int Function();

typedef J2meFontGetC = ffi.UintPtr Function(ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meFontGetDart = int Function(int, int, int);

typedef J2meFontGetIntC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meFontGetIntDart = int Function(int);

typedef J2meFontCharWidthC = ffi.Int32 Function(ffi.UintPtr, ffi.Uint8);
typedef J2meFontCharWidthDart = int Function(int, int);

typedef J2meFontStringWidthC = ffi.Int32 Function(ffi.UintPtr, ffi.Pointer<Utf8>);
typedef J2meFontStringWidthDart = int Function(int, ffi.Pointer<Utf8>);

typedef J2meGraphicsSetFontC = ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.UintPtr);
typedef J2meGraphicsSetFontDart = void Function(ffi.Pointer<ffi.Void>, int);

typedef J2meGraphicsGetFontC = ffi.UintPtr Function(ffi.Pointer<ffi.Void>);
typedef J2meGraphicsGetFontDart = int Function(ffi.Pointer<ffi.Void>);

typedef J2meGraphicsDrawStringC = ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meGraphicsDrawStringDart = void Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, int, int, int);

typedef J2meGraphicsDrawCharC = ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Uint8, ffi.Int32, ffi.Int32, ffi.Int32);
typedef J2meGraphicsDrawCharDart = void Function(ffi.Pointer<ffi.Void>, int, int, int, int);

// System Properties Manager
typedef J2meSystemGetPropertyC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Size);
typedef J2meSystemGetPropertyDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int);

typedef J2meSystemSetPropertyC = ffi.Void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef J2meSystemSetPropertyDart = void Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef J2meSystemHasPropertyC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2meSystemHasPropertyDart = bool Function(ffi.Pointer<Utf8>);

typedef J2meSystemResetPropertiesC = ffi.Void Function();
typedef J2meSystemResetPropertiesDart = void Function();

typedef J2meSystemGetCountC = ffi.Int32 Function();
typedef J2meSystemGetCountDart = int Function();

typedef J2meSystemLoadPropertiesC = ffi.Bool Function(ffi.Pointer<Utf8>);
typedef J2meSystemLoadPropertiesDart = bool Function(ffi.Pointer<Utf8>);

// PCM WAV Player
typedef J2meWavCreateMemoryC = ffi.UintPtr Function(ffi.Pointer<ffi.Uint8>, ffi.Size);
typedef J2meWavCreateMemoryDart = int Function(ffi.Pointer<ffi.Uint8>, int);

typedef J2meWavCreateFileC = ffi.UintPtr Function(ffi.Pointer<Utf8>);
typedef J2meWavCreateFileDart = int Function(ffi.Pointer<Utf8>);

typedef J2meWavActionC = ffi.Void Function(ffi.UintPtr);
typedef J2meWavActionDart = void Function(int);

typedef J2meWavSetIntC = ffi.Void Function(ffi.UintPtr, ffi.Int32);
typedef J2meWavSetIntDart = void Function(int, int);

typedef J2meWavGetIntC = ffi.Int32 Function(ffi.UintPtr);
typedef J2meWavGetIntDart = int Function(int);

typedef J2meWavGetTimeC = ffi.Int64 Function(ffi.UintPtr);
typedef J2meWavGetTimeDart = int Function(int);

typedef J2meWavSetTimeC = ffi.Void Function(ffi.UintPtr, ffi.Int64);
typedef J2meWavSetTimeDart = void Function(int, int);

typedef J2meWavRenderPcmC = ffi.Size Function(ffi.UintPtr, ffi.Pointer<ffi.Int16>, ffi.Size);
typedef J2meWavRenderPcmDart = int Function(int, ffi.Pointer<ffi.Int16>, int);

typedef J2mePlatformPickFileC = ffi.Bool Function(ffi.Pointer<Utf8>, ffi.Size);
typedef J2mePlatformPickFileDart = bool Function(ffi.Pointer<Utf8>, int);

typedef J2mePlatformSetWindowSizeC = ffi.Bool Function(ffi.Int32, ffi.Int32);
typedef J2mePlatformSetWindowSizeDart = bool Function(int, int);

typedef J2mePlatformGetWindowSizeC = ffi.Bool Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);
typedef J2mePlatformGetWindowSizeDart = bool Function(ffi.Pointer<ffi.Int32>, ffi.Pointer<ffi.Int32>);

class J2meBindings {
  late final ffi.DynamicLibrary _dylib;

  ffi.DynamicLibrary get library => _dylib;

  // Core Lifecycle
  late final J2meCreateDart coreCreate;
  late final J2meDestroyDart coreDestroy;
  late final J2meLoadJarDart coreLoadJar;
  late final J2meLoadJarFileDart coreLoadJarFile;
  late final J2meActionDart coreStart;
  late final J2meActionDart corePause;
  late final J2meActionDart coreResume;
  late final J2meActionDart coreStop;

  // Framebuffer
  late final J2meLockFBDart coreLockFramebuffer;
  late final J2meActionDart coreUnlockFramebuffer;
  late final J2meCopyFrameDart coreCopyFrameRgba;
  late final J2meSetDimsDart coreSetScreenDimensions;

  // Input
  late final J2meSendKeyDart coreSendKey;
  // Form / TextBox input dialog
  late final J2meScreenSerialDart coreScreenSerial;
  late final J2meScreenGetDart coreScreenGet;
  late final J2meScreenSubmitDart coreScreenSubmit;
  late final J2meScreenSerialDart coreCanvasCommandsVersion;
  late final J2meScreenGetDart coreCanvasCommands;
  late final J2meSendKeyIdxDart coreCanvasCommand;
  late final J2meSendTouchDart coreSendTouch;

  // Info & Performance
  late final J2meGetStringDart coreGetAppTitle;
  late final J2meGetStringDart coreGetAppVendor;
  late final J2meGetStringDart coreGetAppVersion;
  late final J2meGetFpsDart coreGetFpsLimit;
  late final J2meSetFpsDart coreSetFpsLimit;
  late final void Function(ffi.Pointer<ffi.Void>, bool) coreSetBackground;

  // Audio & Haptics
  late final J2mePlayToneDart corePlayTone;
  late final J2mePlayMidiDart corePlayMidi;
  late final J2meActionDart coreStopMidi;
  late final J2meSetVolumeDart coreSetVolume;
  late final J2meDeviceVibrateDart coreDeviceVibrate;
  late final J2meActionDart coreDeviceStopVibration;

  // Keymapper
  late final J2meKeymapSetLayoutDart coreKeymapSetLayout;
  late final J2meKeymapGetLayoutDart coreKeymapGetLayout;

  // Profiles & Presets
  late final J2meProfileCreateDefaultDart coreProfileCreateDefault;
  late final J2meProfileLoadDart coreProfileLoad;
  late final J2meProfileSaveDart coreProfileSave;
  late final J2meProfileGetIntDart coreProfileGetInt;
  late final J2meProfileSetIntDart coreProfileSetInt;
  late final J2meProfileGetStringDart coreProfileGetString;
  late final J2meProfileSetStringDart coreProfileSetString;
  late final J2meProfileDestroyDart coreProfileDestroy;
  late final J2meApplyProfileDart coreApplyProfile;
  late final J2meGetPresetResolutionCountDart coreGetPresetResolutionCount;
  late final J2meGetPresetResolutionDart coreGetPresetResolution;

  // Java CLDC VM
  late final J2meVmCreateDart coreVmCreate;
  late final J2meVmLoadClassDart coreVmLoadClass;
  late final J2meVmInvokeStaticIntDart coreVmInvokeStaticInt;
  late final J2meVmDestroyDart coreVmDestroy;

  // Section 16: TiledLayer & LayerManager
  late final J2meTiledLayerCreateDart coreTiledLayerCreate;
  late final J2meTiledLayerSetCellDart coreTiledLayerSetCell;
  late final J2meTiledLayerGetCellDart coreTiledLayerGetCell;
  late final J2meTiledLayerFillCellsDart coreTiledLayerFillCells;
  late final J2meTiledLayerCreateAnimDart coreTiledLayerCreateAnimatedTile;
  late final J2meTiledLayerSetAnimDart coreTiledLayerSetAnimatedTile;
  late final J2meTiledLayerGetAnimDart coreTiledLayerGetAnimatedTile;
  late final J2meTiledLayerDestroyDart coreTiledLayerDestroy;

  late final J2meLayerManagerCreateDart coreLayerManagerCreate;
  late final J2meLayerManagerAppendDart coreLayerManagerAppend;
  late final J2meLayerManagerInsertDart coreLayerManagerInsert;
  late final J2meLayerManagerGetSizeDart coreLayerManagerGetSize;
  late final J2meLayerManagerRemoveDart coreLayerManagerRemove;
  late final J2meLayerManagerSetViewDart coreLayerManagerSetViewWindow;
  late final J2meLayerManagerPaintDart coreLayerManagerPaint;
  late final J2meLayerManagerDestroyDart coreLayerManagerDestroy;

  // Section 17: Speed Multiplier & Screenshot PNG
  late final J2meSetSpeedMultDart coreSetSpeedMultiplier;
  late final J2meGetSpeedMultDart coreGetSpeedMultiplier;
  late final J2meCaptureScreenshotDart coreCaptureScreenshotPng;

  // Section 18: GCF Datagram UDP Networking
  late final J2meDatagramCreateDart coreDatagramCreate;
  late final J2meDatagramGetAddressDart coreDatagramGetAddress;
  late final J2meDatagramSetAddressDart coreDatagramSetAddress;
  late final J2meDatagramGetLengthDart coreDatagramGetLength;
  late final J2meDatagramSetLengthDart coreDatagramSetLength;
  late final J2meDatagramGetOffsetDart coreDatagramGetOffset;
  late final J2meDatagramGetDataDart coreDatagramGetData;
  late final J2meDatagramSetDataDart coreDatagramSetData;
  late final J2meDatagramResetDart coreDatagramReset;
  late final J2meDatagramWriteDart coreDatagramWrite;
  late final J2meDatagramReadDart coreDatagramRead;
  late final J2meDatagramDestroyDart coreDatagramDestroy;

  late final J2meDatagramConnOpenDart coreDatagramConnOpen;
  late final J2meDatagramConnSendDart coreDatagramConnSend;
  late final J2meDatagramConnReceiveDart coreDatagramConnReceive;
  late final J2meDatagramConnGetPortDart coreDatagramConnGetLocalPort;
  late final J2meDatagramConnCloseDart coreDatagramConnClose;

  // Section 19: M3G Animation & MorphingMesh
  late final J2meM3gVBCreateDart coreM3gVBCreate;
  late final J2meM3gVBSetCoordsDart coreM3gVBSetPositions;
  late final J2meM3gVBSetCoordsDart coreM3gVBSetNormals;
  late final J2meM3gVBDestroyDart coreM3gVBDestroy;

  late final J2meM3gKeyframeSeqCreateDart coreM3gKeyframeSeqCreate;
  late final J2meM3gKeyframeSeqSetDurationDart coreM3gKeyframeSeqSetDuration;
  late final J2meM3gKeyframeSeqGetDurationDart coreM3gKeyframeSeqGetDuration;
  late final J2meM3gKeyframeSeqSetRepeatModeDart coreM3gKeyframeSeqSetRepeatMode;
  late final J2meM3gKeyframeSeqGetRepeatModeDart coreM3gKeyframeSeqGetRepeatMode;
  late final J2meM3gKeyframeSeqSetKeyframeDart coreM3gKeyframeSeqSetKeyframe;
  late final J2meM3gKeyframeSeqSampleDart coreM3gKeyframeSeqSample;
  late final J2meM3gKeyframeSeqDestroyDart coreM3gKeyframeSeqDestroy;

  late final J2meM3gAnimCtrlCreateDart coreM3gAnimCtrlCreate;
  late final J2meM3gAnimCtrlSetActiveIntervalDart coreM3gAnimCtrlSetActiveInterval;
  late final J2meM3gAnimCtrlGetActiveIntervalDart coreM3gAnimCtrlGetActiveIntervalStart;
  late final J2meM3gAnimCtrlGetActiveIntervalDart coreM3gAnimCtrlGetActiveIntervalEnd;
  late final J2meM3gAnimCtrlSetSpeedDart coreM3gAnimCtrlSetSpeed;
  late final J2meM3gAnimCtrlGetSpeedDart coreM3gAnimCtrlGetSpeed;
  late final J2meM3gAnimCtrlSetPositionDart coreM3gAnimCtrlSetPosition;
  late final J2meM3gAnimCtrlGetPositionDart coreM3gAnimCtrlGetPosition;
  late final J2meM3gAnimCtrlSetWeightDart coreM3gAnimCtrlSetWeight;
  late final J2meM3gAnimCtrlGetWeightDart coreM3gAnimCtrlGetWeight;
  late final J2meM3gAnimCtrlDestroyDart coreM3gAnimCtrlDestroy;

  late final J2meM3gAnimTrackCreateDart coreM3gAnimTrackCreate;
  late final J2meM3gAnimTrackSetCtrlDart coreM3gAnimTrackSetController;
  late final J2meM3gAnimTrackGetCtrlDart coreM3gAnimTrackGetController;
  late final J2meM3gAnimTrackGetSeqDart coreM3gAnimTrackGetSequence;
  late final J2meM3gAnimTrackGetPropDart coreM3gAnimTrackGetTargetProperty;
  late final J2meM3gAnimTrackDestroyDart coreM3gAnimTrackDestroy;

  late final J2meM3gMorphMeshCreateDart coreM3gMorphMeshCreate;
  late final J2meM3gMorphMeshSetWeightsDart coreM3gMorphMeshSetWeights;
  late final J2meM3gMorphMeshGetWeightsDart coreM3gMorphMeshGetWeights;
  late final J2meM3gMorphMeshMorphDart coreM3gMorphMeshMorph;
  late final J2meM3gMeshAddAnimTrackDart coreM3gMeshAddAnimationTrack;
  late final J2meM3gMeshAnimateDart coreM3gMeshAnimate;
  late final J2meM3gMeshGetPosDart coreM3gMeshGetPosition;
  late final J2meM3gMeshGetVertPosDart coreM3gMeshGetVertexPosition;
  late final J2meM3gMeshDestroyDart coreM3gMeshDestroy;

  // Section 20: M3G SkinnedMesh & Bone Skeleton
  late final J2meM3gNodeCreateDart coreM3gNodeCreate;
  late final J2meM3gNodeSetTranslationDart coreM3gNodeSetTranslation;
  late final J2meM3gNodeGetTranslationDart coreM3gNodeGetTranslation;
  late final J2meM3gNodeSetOrientationDart coreM3gNodeSetOrientation;
  late final J2meM3gNodeGetOrientationDart coreM3gNodeGetOrientation;
  late final J2meM3gNodeSetScaleDart coreM3gNodeSetScale;
  late final J2meM3gNodeGetScaleDart coreM3gNodeGetScale;
  late final J2meM3gNodeGetGlobalTransformDart coreM3gNodeGetGlobalTransform;
  late final J2meM3gNodeAddAnimTrackDart coreM3gNodeAddAnimationTrack;
  late final J2meM3gNodeAnimateDart coreM3gNodeAnimate;
  late final J2meM3gNodeDestroyDart coreM3gNodeDestroy;

  late final J2meM3gGroupCreateDart coreM3gGroupCreate;
  late final J2meM3gGroupAddChildDart coreM3gGroupAddChild;
  late final J2meM3gGroupRemoveChildDart coreM3gGroupRemoveChild;
  late final J2meM3gGroupGetChildCountDart coreM3gGroupGetChildCount;
  late final J2meM3gGroupGetChildDart coreM3gGroupGetChild;
  late final J2meM3gGroupAnimateDart coreM3gGroupAnimate;
  late final J2meM3gGroupDestroyDart coreM3gGroupDestroy;

  late final J2meM3gSkinnedMeshCreateDart coreM3gSkinnedMeshCreate;
  late final J2meM3gSkinnedMeshAddTransformDart coreM3gSkinnedMeshAddTransform;
  late final J2meM3gSkinnedMeshGetBoneCountDart coreM3gSkinnedMeshGetBoneCount;
  late final J2meM3gSkinnedMeshGetBoneDart coreM3gSkinnedMeshGetBone;
  late final J2meM3gSkinnedMeshGetSkeletonDart coreM3gSkinnedMeshGetSkeleton;
  late final J2meM3gSkinnedMeshSkinDart coreM3gSkinnedMeshSkin;
  late final J2meM3gSkinnedMeshAnimateDart coreM3gSkinnedMeshAnimate;
  late final J2meM3gSkinnedMeshGetVertPosDart coreM3gSkinnedMeshGetVertexPosition;
  late final J2meM3gSkinnedMeshGetVertNormDart coreM3gSkinnedMeshGetVertexNormal;
  late final J2meM3gSkinnedMeshDestroyDart coreM3gSkinnedMeshDestroy;

  // Section 21: App Management & Installer
  late final J2meAppRepoGetCountDart appRepoGetCount;
  late final J2meAppRepoGetItemDart appRepoGetItem;
  late final J2meAppRepoFindByIdDart appRepoFindById;
  late final J2meAppRepoFindByPathDart appRepoFindByPath;
  late final J2meAppRepoDeleteDart appRepoDelete;
  late final J2meAppInstallerCheckJarDart appInstallerCheckJar;
  late final J2meAppInstallerInstallDart appInstallerInstall;
  late final J2meAppInstallerUninstallDart appInstallerUninstall;
  late final J2meAppLaunchDart appLaunch;
  late final J2meAppSpawnDart appSpawn;

  // Section 22: JSR-82 Mobile Bluetooth & RFCOMM/L2CAP Multiplayer
  late final J2meBtIsPowerOnDart btIsPowerOn;
  late final J2meBtSetPowerOnDart btSetPowerOn;
  late final J2meBtGetLocalAddressDart btGetLocalAddress;
  late final J2meBtSetLocalAddressDart btSetLocalAddress;
  late final J2meBtGetLocalNameDart btGetLocalName;
  late final J2meBtSetLocalNameDart btSetLocalName;
  late final J2meBtGetDiscoverableDart btGetDiscoverable;
  late final J2meBtSetDiscoverableDart btSetDiscoverable;
  late final J2meBtGetPropertyDart btGetProperty;

  late final J2meBtOpenBtsppServerDart btOpenBtsppServer;
  late final J2meBtBtsppAcceptDart btBtsppAccept;
  late final J2meBtBtsppServerCloseDart btBtsppServerClose;
  late final J2meBtOpenBtsppClientDart btOpenBtsppClient;
  late final J2meBtBtsppReadDart btBtsppRead;
  late final J2meBtBtsppWriteDart btBtsppWrite;
  late final J2meBtBtsppAvailableDart btBtsppAvailable;
  late final J2meBtBtsppCloseDart btBtsppClose;

  late final J2meBtOpenBtl2capServerDart btOpenBtl2capServer;
  late final J2meBtBtl2capAcceptDart btBtl2capAccept;
  late final J2meBtBtl2capServerCloseDart btBtl2capServerClose;
  late final J2meBtOpenBtl2capClientDart btOpenBtl2capClient;
  late final J2meBtBtl2capSendDart btBtl2capSend;
  late final J2meBtBtl2capReceiveDart btBtl2capReceive;
  late final J2meBtBtl2capReadyDart btBtl2capReady;
  late final J2meBtBtl2capCloseDart btBtl2capClose;

  // Section 23: Vodafone VSCL & Carrier OEM Extensions
  late final J2meVodafoneSpriteCreateDart vodafoneSpriteCreate;
  late final J2meVodafoneSpriteDestroyDart vodafoneSpriteDestroy;
  late final J2meVodafoneSpriteSetPaletteDart vodafoneSpriteSetPalette;
  late final J2meVodafoneSpriteGetPaletteDart vodafoneSpriteGetPalette;
  late final J2meVodafoneSpriteSetPatternDart vodafoneSpriteSetPattern;
  late final J2meVodafoneSpriteCreateCommandDart vodafoneSpriteCreateCommand;
  late final J2meVodafoneSpriteCreateFbDart vodafoneSpriteCreateFramebuffer;
  late final J2meVodafoneSpriteDisposeFbDart vodafoneSpriteDisposeFramebuffer;
  late final J2meVodafoneSpriteDrawCharDart vodafoneSpriteDrawChar;
  late final J2meVodafoneSpriteCopyAreaDart vodafoneSpriteCopyArea;
  late final J2meVodafoneSpriteDrawFbDart vodafoneSpriteDrawFramebuffer;
  late final J2meVodafoneSpriteGetFbDart vodafoneSpriteGetFramebuffer;

  late final J2meVodafoneDeviceGetStateDart vodafoneDeviceGetState;
  late final J2meVodafoneDeviceIsActiveDart vodafoneDeviceIsActive;
  late final J2meVodafoneDeviceSetActiveDart vodafoneDeviceSetActive;
  late final J2meVodafoneDeviceBlinkDart vodafoneDeviceBlink;
  late final J2meVodafoneDeviceGetKeystatesDart vodafoneDeviceGetKeystates;
  late final J2meVodafoneDeviceSetKeystatesMaskDart vodafoneDeviceSetKeystatesMask;
  late final J2meVodafoneDeviceKeyEventDart vodafoneDeviceKeyEvent;

  late final J2meVodafoneEncodeOffscreenDart vodafoneEncodeOffscreen;

  late final J2meCarrierKddiGetKeyStateDart carrierKddiGetKeyState;
  late final J2meCarrierMotorolaFunlightSetColorDart carrierMotorolaFunlightSetColor;
  late final J2meCarrierMotorolaFunlightGetColorDart carrierMotorolaFunlightGetColor;
  late final J2meCarrierSonyAccelSetDart carrierSonyAccelSet;
  late final J2meCarrierSonyAccelGetDart carrierSonyAccelGet;
  late final J2meCarrierSprintPlayClipDart carrierSprintPlayClip;
  late final J2meCarrierSprintStopDart carrierSprintStop;
  late final J2meCarrierSprintIsPlayingDart carrierSprintIsPlaying;

  // Section 24: JSR-179 Mobile Location API
  late final J2meLocationProviderGetStateDart locationProviderGetState;
  late final J2meLocationProviderSetStateDart locationProviderSetState;
  late final J2meLocationProviderUpdateHostLocationDart locationProviderUpdateHostLocation;
  late final J2meLocationProviderGetLastKnownDart locationProviderGetLastKnown;
  late final J2meLocationProviderGetNmeaDart locationProviderGetNmea;

  late final J2meLocationCoordinatesDistanceDart locationCoordinatesDistance;
  late final J2meLocationCoordinatesAzimuthDart locationCoordinatesAzimuth;
  late final J2meLocationCoordinatesConvertToStringDart locationCoordinatesConvertToString;
  late final J2meLocationCoordinatesConvertFromStringDart locationCoordinatesConvertFromString;

  late final J2meLocationOrientationGetDart locationOrientationGet;
  late final J2meLocationOrientationSetDart locationOrientationSet;

  late final J2meLocationLandmarkStoreCreateDart locationLandmarkStoreCreate;
  late final J2meLocationLandmarkStoreDeleteDart locationLandmarkStoreDelete;
  late final J2meLocationLandmarkStoreAddLandmarkDart locationLandmarkStoreAddLandmark;
  late final J2meLocationLandmarkStoreGetCountDart locationLandmarkStoreGetCount;

  // Section 25: JSR-256 Mobile Sensor API
  late final J2meSensorGetCountDart sensorGetCount;
  late final J2meSensorGetUrlDart sensorGetUrl;
  late final J2meSensorGetQuantityDart sensorGetQuantity;
  late final J2meSensorGetContextTypeDart sensorGetContextType;
  late final J2meSensorFindDart sensorFind;

  late final J2meSensorOpenDart sensorOpen;
  late final J2meSensorCloseDart sensorClose;
  late final J2meSensorGetStateDart sensorGetState;
  late final J2meSensorGetChannelCountDart sensorGetChannelCount;
  late final J2meSensorGetChannelNameDart sensorGetChannelName;
  late final J2meSensorGetDataDart sensorGetData;

  late final J2meSensorUpdate3DDart sensorUpdateAccelerometer;
  late final J2meSensorUpdate1DDart sensorUpdateAmbientLight;
  late final J2meSensorUpdate3DDart sensorUpdateMagneticField;
  late final J2meSensorUpdate3DDart sensorUpdateOrientation;
  late final J2meSensorUpdate1DDart sensorUpdateTemperature;

  // Section 26: JSR-75 PIM (Personal Information Management)
  late final J2mePimInitDart pimInit;
  late final J2mePimListCountDart pimListCount;
  late final J2mePimListGetNameDart pimListGetName;
  late final J2mePimOpenListDart pimOpenList;
  late final J2mePimCloseListDart pimCloseList;
  late final J2mePimListGetItemCountDart pimListGetItemCount;
  late final J2mePimListGetItemDart pimListGetItem;
  late final J2mePimListRemoveItemDart pimListRemoveItem;

  late final J2mePimContactCreateDart pimContactCreate;
  late final J2mePimContactSetNameDart pimContactSetName;
  late final J2mePimContactGetFormattedNameDart pimContactGetFormattedName;
  late final J2mePimContactAddTelDart pimContactAddTel;
  late final J2mePimContactGetTelCountDart pimContactGetTelCount;
  late final J2mePimContactGetTelDart pimContactGetTel;
  late final J2mePimContactAddEmailDart pimContactAddEmail;
  late final J2mePimContactGetEmailCountDart pimContactGetEmailCount;
  late final J2mePimContactGetEmailDart pimContactGetEmail;
  late final J2mePimContactSetAddressDart pimContactSetAddress;

  late final J2mePimEventCreateDart pimEventCreate;
  late final J2mePimEventSetDetailsDart pimEventSetDetails;
  late final J2mePimEventGetDetailsDart pimEventGetDetails;
  late final J2mePimEventSetRepeatRuleDart pimEventSetRepeatRule;

  late final J2mePimTodoCreateDart pimTodoCreate;
  late final J2mePimTodoSetDetailsDart pimTodoSetDetails;
  late final J2mePimTodoGetDetailsDart pimTodoGetDetails;

  late final J2mePimItemCommitDart pimItemCommit;
  late final J2mePimItemAddCategoryDart pimItemAddCategory;
  late final J2mePimItemGetCategoryCountDart pimItemGetCategoryCount;
  late final J2mePimItemGetCategoryDart pimItemGetCategory;

  late final J2mePimExportSerialDart pimExportSerial;
  late final J2mePimImportSerialDart pimImportSerial;
  late final J2mePimSaveAllDart pimSaveAll;
 
  // Section 27: JSR-234 AMMS
  late final J2meAmmsSpectatorSetLocationDart ammsSpectatorSetLocation;
  late final J2meAmmsSpectatorGetLocationDart ammsSpectatorGetLocation;
  late final J2meAmmsSpectatorSetOrientationDart ammsSpectatorSetOrientation;
  late final J2meAmmsSpectatorGetOrientationDart ammsSpectatorGetOrientation;

  late final J2meAmmsSoundSourceCreateDart ammsSoundSourceCreate;
  late final J2meAmmsSoundSourceDestroyDart ammsSoundSourceDestroy;
  late final J2meAmmsSoundSourceSetLocationDart ammsSoundSourceSetLocation;
  late final J2meAmmsSoundSourceGetLocationDart ammsSoundSourceGetLocation;
  late final J2meAmmsSoundSourceSetVelocityDart ammsSoundSourceSetVelocity;
  late final J2meAmmsSoundSourceSetAttenuationDart ammsSoundSourceSetAttenuation;
  late final J2meAmmsSoundSourceEvaluateDart ammsSoundSourceEvaluate;

  late final J2meAmmsEffectModuleCreateDart ammsEffectModuleCreate;
  late final J2meAmmsEffectModuleDestroyDart ammsEffectModuleDestroy;
  late final J2meAmmsReverbSetLevelDart ammsReverbSetLevel;
  late final J2meAmmsReverbGetLevelDart ammsReverbGetLevel;
  late final J2meAmmsReverbSetPresetDart ammsReverbSetPreset;
  late final J2meAmmsReverbGetPresetDart ammsReverbGetPreset;
  late final J2meAmmsEqualizerSetBandLevelDart ammsEqualizerSetBandLevel;
  late final J2meAmmsEqualizerGetBandLevelDart ammsEqualizerGetBandLevel;
  late final J2meAmmsEqualizerGetBandCountDart ammsEqualizerGetBandCount;
  late final J2meAmmsEqualizerSetPresetDart ammsEqualizerSetPreset;
  late final J2meAmmsPanSetDart ammsPanSet;
  late final J2meAmmsPanGetDart ammsPanGet;

  late final J2meAmmsCameraSetRotationDart ammsCameraSetRotation;
  late final J2meAmmsCameraGetRotationDart ammsCameraGetRotation;
  late final J2meAmmsCameraSetExposureModeDart ammsCameraSetExposureMode;
  late final J2meAmmsCameraGetExposureModeDart ammsCameraGetExposureMode;
  late final J2meAmmsFlashSetModeDart ammsFlashSetMode;
  late final J2meAmmsFlashGetModeDart ammsFlashGetMode;
  late final J2meAmmsZoomSetDigitalDart ammsZoomSetDigital;
  late final J2meAmmsZoomGetDigitalDart ammsZoomGetDigital;
  late final J2meAmmsImageTransformSetCropDart ammsImageTransformSetCrop;
  late final J2meAmmsImageTransformSetTargetDart ammsImageTransformSetTarget;

  // Section 28: MIDP 2.0 PushRegistry & CommConnection
  late final J2mePushRegisterConnectionDart pushRegisterConnection;
  late final J2mePushUnregisterConnectionDart pushUnregisterConnection;
  late final J2mePushListConnectionsDart pushListConnections;
  late final J2mePushGetMIDletDart pushGetMIDlet;
  late final J2mePushGetFilterDart pushGetFilter;
  late final J2mePushRegisterAlarmDart pushRegisterAlarm;
  late final J2mePushNotifyInboundDart pushNotifyInbound;
  late final J2mePushCheckAlarmsDart pushCheckAlarms;
  late final J2mePushSaveDart pushSave;
  late final J2mePushLoadDart pushLoad;

  late final J2meCommOpenDart commOpen;
  late final J2meCommCloseDart commClose;
  late final J2meCommGetBaudRateDart commGetBaudRate;
  late final J2meCommSetBaudRateDart commSetBaudRate;
  late final J2meCommWriteDart commWrite;
  late final J2meCommReadDart commRead;
  late final J2meCommAvailableDart commAvailable;

  // Section 29: PKI Security, SSL & HTTPS
  late final J2meCertCreateDart certCreate;
  late final J2meCertDestroyDart certDestroy;
  late final J2meCertGetStringDart certGetSubject;
  late final J2meCertGetStringDart certGetIssuer;
  late final J2meCertGetStringDart certGetType;
  late final J2meCertGetStringDart certGetVersion;
  late final J2meCertGetStringDart certGetSigAlg;
  late final J2meCertGetTimeDart certGetNotBefore;
  late final J2meCertGetTimeDart certGetNotAfter;
  late final J2meCertGetStringDart certGetSerial;
  late final J2meCertValidateDart certValidate;

  late final J2meSslOpenDart sslOpen;
  late final J2meSslCloseDart sslClose;
  late final J2meSslIsOpenDart sslIsOpen;
  late final J2meSslGetPortDart sslGetPort;
  late final J2meSslWriteDart sslWrite;
  late final J2meSslReadDart sslRead;
  late final J2meSslAvailableDart sslAvailable;
  late final J2meSslFeedInputDart sslFeedInput;
  late final J2meSslGetSecurityInfoDart sslGetSecurityInfo;

  late final J2meHttpsOpenDart httpsOpen;
  late final J2meHttpsCloseDart httpsClose;
  late final J2meHttpsIsOpenDart httpsIsOpen;
  late final J2meHttpsSetMethodDart httpsSetMethod;
  late final J2meHttpsSetRequestPropDart httpsSetRequestProperty;
  late final J2meHttpsGetCodeDart httpsGetResponseCode;
  late final J2meHttpsGetPortDart httpsGetPort;
  late final J2meHttpsSetResponseHeaderDart httpsSetResponseHeader;
  late final J2meHttpsGetHeaderFieldDart httpsGetHeaderField;
  late final J2meHttpsFeedRespDart httpsFeedResponse;
  late final J2meHttpsReadDart httpsRead;
  late final J2meHttpsWriteDart httpsWrite;

  // Section 30: 3D Binary Asset Loaders (M3G & Micro3D)
  late final J2me3dIdentifyFormatDart identify3dFormat;
  late final J2meM3gLoadMemoryDart m3gLoadMemory;
  late final J2meM3gSceneDestroyDart m3gSceneDestroy;
  late final J2meMicro3dLoadFigureDart micro3dLoadFigure;
  late final J2meMicro3dFigureDestroyDart micro3dFigureDestroy;
  late final J2meMicro3dFigureGetCountsDart micro3dFigureGetCounts;
  late final J2meMicro3dLoadActionTableDart micro3dLoadActionTable;
  late final J2meMicro3dActionTableDestroyDart micro3dActionTableDestroy;
  late final J2meMicro3dActionTableGetCountDart micro3dActionTableGetCount;

  // Section 31: LCDUI Font Engine, System Properties & PCM WAV Player
  late final J2meFontGetDefaultDart fontGetDefault;
  late final J2meFontGetDart fontGet;
  late final J2meFontGetIntDart fontGetHeight;
  late final J2meFontGetIntDart fontGetBaseline;
  late final J2meFontCharWidthDart fontCharWidth;
  late final J2meFontStringWidthDart fontStringWidth;
  late final J2meFontGetIntDart fontGetFace;
  late final J2meFontGetIntDart fontGetStyle;
  late final J2meFontGetIntDart fontGetSize;
  late final J2meGraphicsSetFontDart graphicsSetFont;
  late final J2meGraphicsGetFontDart graphicsGetFont;
  late final J2meGraphicsDrawStringDart graphicsDrawString;
  late final J2meGraphicsDrawCharDart graphicsDrawChar;

  late final J2meSystemGetPropertyDart systemGetProperty;
  late final J2meSystemSetPropertyDart systemSetProperty;
  late final J2meSystemHasPropertyDart systemHasProperty;
  late final J2meSystemResetPropertiesDart systemResetProperties;
  late final J2meSystemGetCountDart systemGetPropertyCount;
  late final J2meSystemLoadPropertiesDart systemLoadProperties;

  late final J2meWavCreateMemoryDart wavCreateMemory;
  late final J2meWavCreateFileDart wavCreateFile;
  late final J2meWavActionDart wavStart;
  late final J2meWavActionDart wavStop;
  late final J2meWavSetIntDart wavSetLoop;
  late final J2meWavSetIntDart wavSetVolume;
  late final J2meWavGetIntDart wavGetVolume;
  late final J2meWavGetTimeDart wavGetDurationUs;
  late final J2meWavGetTimeDart wavGetMediaTimeUs;
  late final J2meWavSetTimeDart wavSetMediaTimeUs;
  late final J2meWavRenderPcmDart wavRenderPcm;
  late final J2meWavActionDart wavDestroy;
  late final J2mePlatformPickFileDart platformPickFile;
  late final J2mePlatformSetWindowSizeDart platformSetWindowSize;
  late final J2mePlatformGetWindowSizeDart platformGetWindowSize;

  static J2meBindings? _instance;

  static J2meBindings get instance {
    _instance ??= J2meBindings._init();
    return _instance!;
  }

  J2meBindings._init() {
    if (Platform.isWindows) {
      // Bundled next to ui_app.exe by windows/CMakeLists.txt
      _dylib = ffi.DynamicLibrary.open("j2me_core.dll");
    } else if (Platform.isAndroid) {
      _dylib = ffi.DynamicLibrary.open("libj2me_core.so");
    } else if (Platform.isLinux) {
      _dylib = ffi.DynamicLibrary.open("libj2me_core.so");
    } else if (Platform.isMacOS) {
      try {
        _dylib = ffi.DynamicLibrary.open("libj2me_core.dylib");
      } catch (_) {
        _dylib = ffi.DynamicLibrary.process();
      }
    } else {
      // iOS: J2meCore.framework from the CocoaPods build (ios/Podfile) is linked into the app
      try {
        _dylib = ffi.DynamicLibrary.open("${File(Platform.resolvedExecutable).parent.path}/Frameworks/J2meCore.framework/J2meCore");
      } catch (_) {
        _dylib = ffi.DynamicLibrary.process();
      }
    }

    coreCreate = _dylib.lookupFunction<J2meCreateC, J2meCreateDart>('j2me_core_create');
    coreSetBackground = _dylib.lookupFunction<ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Bool), void Function(ffi.Pointer<ffi.Void>, bool)>('j2me_core_set_background');
    coreDestroy = _dylib.lookupFunction<J2meDestroyC, J2meDestroyDart>('j2me_core_destroy');
    coreLoadJar = _dylib.lookupFunction<J2meLoadJarC, J2meLoadJarDart>('j2me_core_load_jar');
    coreLoadJarFile = _dylib.lookupFunction<J2meLoadJarFileC, J2meLoadJarFileDart>('j2me_core_load_jar_file');
    coreStart = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_start');
    corePause = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_pause');
    coreResume = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_resume');
    coreStop = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_stop');

    coreLockFramebuffer = _dylib.lookupFunction<J2meLockFBC, J2meLockFBDart>('j2me_core_lock_framebuffer');
    coreUnlockFramebuffer = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_unlock_framebuffer');
    coreCopyFrameRgba = _dylib.lookupFunction<J2meCopyFrameC, J2meCopyFrameDart>('j2me_core_copy_frame_rgba');
    coreSetScreenDimensions = _dylib.lookupFunction<J2meSetDimsC, J2meSetDimsDart>('j2me_core_set_screen_dimensions');

    coreSendKey = _dylib.lookupFunction<J2meSendKeyC, J2meSendKeyDart>('j2me_core_send_key');
    coreScreenSerial = _dylib.lookupFunction<J2meScreenSerialC, J2meScreenSerialDart>('j2me_core_screen_serial');
    coreScreenGet = _dylib.lookupFunction<J2meScreenGetC, J2meScreenGetDart>('j2me_core_screen_get');
    coreScreenSubmit = _dylib.lookupFunction<J2meScreenSubmitC, J2meScreenSubmitDart>('j2me_core_screen_submit');
    coreCanvasCommandsVersion = _dylib.lookupFunction<J2meScreenSerialC, J2meScreenSerialDart>('j2me_core_canvas_commands_version');
    coreCanvasCommands = _dylib.lookupFunction<J2meScreenGetC, J2meScreenGetDart>('j2me_core_canvas_commands');
    coreCanvasCommand = _dylib.lookupFunction<J2meSendKeyIdxC, J2meSendKeyIdxDart>('j2me_core_canvas_command');
    coreSendTouch = _dylib.lookupFunction<J2meSendTouchC, J2meSendTouchDart>('j2me_core_send_touch');

    coreGetAppTitle = _dylib.lookupFunction<J2meGetStringC, J2meGetStringDart>('j2me_core_get_app_title');
    coreGetAppVendor = _dylib.lookupFunction<J2meGetStringC, J2meGetStringDart>('j2me_core_get_app_vendor');
    coreGetAppVersion = _dylib.lookupFunction<J2meGetStringC, J2meGetStringDart>('j2me_core_get_app_version');
    coreGetFpsLimit = _dylib.lookupFunction<J2meGetFpsC, J2meGetFpsDart>('j2me_core_get_fps_limit');
    coreSetFpsLimit = _dylib.lookupFunction<J2meSetFpsC, J2meSetFpsDart>('j2me_core_set_fps_limit');

    corePlayTone = _dylib.lookupFunction<J2mePlayToneC, J2mePlayToneDart>('j2me_core_play_tone');
    corePlayMidi = _dylib.lookupFunction<J2mePlayMidiC, J2mePlayMidiDart>('j2me_core_play_midi');
    coreStopMidi = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_stop_midi');
    coreSetVolume = _dylib.lookupFunction<J2meSetVolumeC, J2meSetVolumeDart>('j2me_core_set_volume');
    coreDeviceVibrate = _dylib.lookupFunction<J2meDeviceVibrateC, J2meDeviceVibrateDart>('j2me_core_device_vibrate');
    coreDeviceStopVibration = _dylib.lookupFunction<J2meActionC, J2meActionDart>('j2me_core_device_stop_vibration');

    coreKeymapSetLayout = _dylib.lookupFunction<J2meKeymapSetLayoutC, J2meKeymapSetLayoutDart>('j2me_core_keymap_set_layout');
    coreKeymapGetLayout = _dylib.lookupFunction<J2meKeymapGetLayoutC, J2meKeymapGetLayoutDart>('j2me_core_keymap_get_layout');

    coreProfileCreateDefault = _dylib.lookupFunction<J2meProfileCreateDefaultC, J2meProfileCreateDefaultDart>('j2me_core_profile_create_default');
    coreProfileLoad = _dylib.lookupFunction<J2meProfileLoadC, J2meProfileLoadDart>('j2me_core_profile_load');
    coreProfileSave = _dylib.lookupFunction<J2meProfileSaveC, J2meProfileSaveDart>('j2me_core_profile_save');
    coreProfileGetInt = _dylib.lookupFunction<J2meProfileGetIntC, J2meProfileGetIntDart>('j2me_core_profile_get_int');
    coreProfileSetInt = _dylib.lookupFunction<J2meProfileSetIntC, J2meProfileSetIntDart>('j2me_core_profile_set_int');
    coreProfileGetString = _dylib.lookupFunction<J2meProfileGetStringC, J2meProfileGetStringDart>('j2me_core_profile_get_string');
    coreProfileSetString = _dylib.lookupFunction<J2meProfileSetStringC, J2meProfileSetStringDart>('j2me_core_profile_set_string');
    coreProfileDestroy = _dylib.lookupFunction<J2meProfileDestroyC, J2meProfileDestroyDart>('j2me_core_profile_destroy');
    coreApplyProfile = _dylib.lookupFunction<J2meApplyProfileC, J2meApplyProfileDart>('j2me_core_apply_profile');
    coreGetPresetResolutionCount = _dylib.lookupFunction<J2meGetPresetResolutionCountC, J2meGetPresetResolutionCountDart>('j2me_core_get_preset_resolution_count');
    coreGetPresetResolution = _dylib.lookupFunction<J2meGetPresetResolutionC, J2meGetPresetResolutionDart>('j2me_core_get_preset_resolution');

    coreVmCreate = _dylib.lookupFunction<J2meVmCreateC, J2meVmCreateDart>('j2me_core_vm_create');
    coreVmLoadClass = _dylib.lookupFunction<J2meVmLoadClassC, J2meVmLoadClassDart>('j2me_core_vm_load_class');
    coreVmInvokeStaticInt = _dylib.lookupFunction<J2meVmInvokeStaticIntC, J2meVmInvokeStaticIntDart>('j2me_core_vm_invoke_static_int');
    coreVmDestroy = _dylib.lookupFunction<J2meVmDestroyC, J2meVmDestroyDart>('j2me_core_vm_destroy');

    // Section 16: TiledLayer & LayerManager
    coreTiledLayerCreate = _dylib.lookupFunction<J2meTiledLayerCreateC, J2meTiledLayerCreateDart>('j2me_core_tiled_layer_create');
    coreTiledLayerSetCell = _dylib.lookupFunction<J2meTiledLayerSetCellC, J2meTiledLayerSetCellDart>('j2me_core_tiled_layer_set_cell');
    coreTiledLayerGetCell = _dylib.lookupFunction<J2meTiledLayerGetCellC, J2meTiledLayerGetCellDart>('j2me_core_tiled_layer_get_cell');
    coreTiledLayerFillCells = _dylib.lookupFunction<J2meTiledLayerFillCellsC, J2meTiledLayerFillCellsDart>('j2me_core_tiled_layer_fill_cells');
    coreTiledLayerCreateAnimatedTile = _dylib.lookupFunction<J2meTiledLayerCreateAnimC, J2meTiledLayerCreateAnimDart>('j2me_core_tiled_layer_create_animated_tile');
    coreTiledLayerSetAnimatedTile = _dylib.lookupFunction<J2meTiledLayerSetAnimC, J2meTiledLayerSetAnimDart>('j2me_core_tiled_layer_set_animated_tile');
    coreTiledLayerGetAnimatedTile = _dylib.lookupFunction<J2meTiledLayerGetAnimC, J2meTiledLayerGetAnimDart>('j2me_core_tiled_layer_get_animated_tile');
    coreTiledLayerDestroy = _dylib.lookupFunction<J2meTiledLayerDestroyC, J2meTiledLayerDestroyDart>('j2me_core_tiled_layer_destroy');

    coreLayerManagerCreate = _dylib.lookupFunction<J2meLayerManagerCreateC, J2meLayerManagerCreateDart>('j2me_core_layer_manager_create');
    coreLayerManagerAppend = _dylib.lookupFunction<J2meLayerManagerAppendC, J2meLayerManagerAppendDart>('j2me_core_layer_manager_append');
    coreLayerManagerInsert = _dylib.lookupFunction<J2meLayerManagerInsertC, J2meLayerManagerInsertDart>('j2me_core_layer_manager_insert');
    coreLayerManagerGetSize = _dylib.lookupFunction<J2meLayerManagerGetSizeC, J2meLayerManagerGetSizeDart>('j2me_core_layer_manager_get_size');
    coreLayerManagerRemove = _dylib.lookupFunction<J2meLayerManagerRemoveC, J2meLayerManagerRemoveDart>('j2me_core_layer_manager_remove');
    coreLayerManagerSetViewWindow = _dylib.lookupFunction<J2meLayerManagerSetViewC, J2meLayerManagerSetViewDart>('j2me_core_layer_manager_set_view_window');
    coreLayerManagerPaint = _dylib.lookupFunction<J2meLayerManagerPaintC, J2meLayerManagerPaintDart>('j2me_core_layer_manager_paint');
    coreLayerManagerDestroy = _dylib.lookupFunction<J2meLayerManagerDestroyC, J2meLayerManagerDestroyDart>('j2me_core_layer_manager_destroy');

    // Section 17: Speed Multiplier & Screenshot PNG
    coreSetSpeedMultiplier = _dylib.lookupFunction<J2meSetSpeedMultC, J2meSetSpeedMultDart>('j2me_core_set_speed_multiplier');
    coreGetSpeedMultiplier = _dylib.lookupFunction<J2meGetSpeedMultC, J2meGetSpeedMultDart>('j2me_core_get_speed_multiplier');
    coreCaptureScreenshotPng = _dylib.lookupFunction<J2meCaptureScreenshotC, J2meCaptureScreenshotDart>('j2me_core_capture_screenshot_png');

    // Section 18: GCF Datagram UDP Networking
    coreDatagramCreate = _dylib.lookupFunction<J2meDatagramCreateC, J2meDatagramCreateDart>('j2me_core_datagram_create');
    coreDatagramGetAddress = _dylib.lookupFunction<J2meDatagramGetAddressC, J2meDatagramGetAddressDart>('j2me_core_datagram_get_address');
    coreDatagramSetAddress = _dylib.lookupFunction<J2meDatagramSetAddressC, J2meDatagramSetAddressDart>('j2me_core_datagram_set_address');
    coreDatagramGetLength = _dylib.lookupFunction<J2meDatagramGetLengthC, J2meDatagramGetLengthDart>('j2me_core_datagram_get_length');
    coreDatagramSetLength = _dylib.lookupFunction<J2meDatagramSetLengthC, J2meDatagramSetLengthDart>('j2me_core_datagram_set_length');
    coreDatagramGetOffset = _dylib.lookupFunction<J2meDatagramGetOffsetC, J2meDatagramGetOffsetDart>('j2me_core_datagram_get_offset');
    coreDatagramGetData = _dylib.lookupFunction<J2meDatagramGetDataC, J2meDatagramGetDataDart>('j2me_core_datagram_get_data');
    coreDatagramSetData = _dylib.lookupFunction<J2meDatagramSetDataC, J2meDatagramSetDataDart>('j2me_core_datagram_set_data');
    coreDatagramReset = _dylib.lookupFunction<J2meDatagramResetC, J2meDatagramResetDart>('j2me_core_datagram_reset');
    coreDatagramWrite = _dylib.lookupFunction<J2meDatagramWriteC, J2meDatagramWriteDart>('j2me_core_datagram_write');
    coreDatagramRead = _dylib.lookupFunction<J2meDatagramReadC, J2meDatagramReadDart>('j2me_core_datagram_read');
    coreDatagramDestroy = _dylib.lookupFunction<J2meDatagramDestroyC, J2meDatagramDestroyDart>('j2me_core_datagram_destroy');

    coreDatagramConnOpen = _dylib.lookupFunction<J2meDatagramConnOpenC, J2meDatagramConnOpenDart>('j2me_core_datagram_conn_open');
    coreDatagramConnSend = _dylib.lookupFunction<J2meDatagramConnSendC, J2meDatagramConnSendDart>('j2me_core_datagram_conn_send');
    coreDatagramConnReceive = _dylib.lookupFunction<J2meDatagramConnReceiveC, J2meDatagramConnReceiveDart>('j2me_core_datagram_conn_receive');
    coreDatagramConnGetLocalPort = _dylib.lookupFunction<J2meDatagramConnGetPortC, J2meDatagramConnGetPortDart>('j2me_core_datagram_conn_get_local_port');
    coreDatagramConnClose = _dylib.lookupFunction<J2meDatagramConnCloseC, J2meDatagramConnCloseDart>('j2me_core_datagram_conn_close');

    // Section 19: M3G Animation & MorphingMesh
    coreM3gVBCreate = _dylib.lookupFunction<J2meM3gVBCreateC, J2meM3gVBCreateDart>('j2me_core_m3g_vertexbuffer_create');
    coreM3gVBSetPositions = _dylib.lookupFunction<J2meM3gVBSetCoordsC, J2meM3gVBSetCoordsDart>('j2me_core_m3g_vertexbuffer_set_positions');
    coreM3gVBSetNormals = _dylib.lookupFunction<J2meM3gVBSetCoordsC, J2meM3gVBSetCoordsDart>('j2me_core_m3g_vertexbuffer_set_normals');
    coreM3gVBDestroy = _dylib.lookupFunction<J2meM3gVBDestroyC, J2meM3gVBDestroyDart>('j2me_core_m3g_vertexbuffer_destroy');

    coreM3gKeyframeSeqCreate = _dylib.lookupFunction<J2meM3gKeyframeSeqCreateC, J2meM3gKeyframeSeqCreateDart>('j2me_core_m3g_keyframesequence_create');
    coreM3gKeyframeSeqSetDuration = _dylib.lookupFunction<J2meM3gKeyframeSeqSetDurationC, J2meM3gKeyframeSeqSetDurationDart>('j2me_core_m3g_keyframesequence_set_duration');
    coreM3gKeyframeSeqGetDuration = _dylib.lookupFunction<J2meM3gKeyframeSeqGetDurationC, J2meM3gKeyframeSeqGetDurationDart>('j2me_core_m3g_keyframesequence_get_duration');
    coreM3gKeyframeSeqSetRepeatMode = _dylib.lookupFunction<J2meM3gKeyframeSeqSetRepeatModeC, J2meM3gKeyframeSeqSetRepeatModeDart>('j2me_core_m3g_keyframesequence_set_repeat_mode');
    coreM3gKeyframeSeqGetRepeatMode = _dylib.lookupFunction<J2meM3gKeyframeSeqGetRepeatModeC, J2meM3gKeyframeSeqGetRepeatModeDart>('j2me_core_m3g_keyframesequence_get_repeat_mode');
    coreM3gKeyframeSeqSetKeyframe = _dylib.lookupFunction<J2meM3gKeyframeSeqSetKeyframeC, J2meM3gKeyframeSeqSetKeyframeDart>('j2me_core_m3g_keyframesequence_set_keyframe');
    coreM3gKeyframeSeqSample = _dylib.lookupFunction<J2meM3gKeyframeSeqSampleC, J2meM3gKeyframeSeqSampleDart>('j2me_core_m3g_keyframesequence_sample');
    coreM3gKeyframeSeqDestroy = _dylib.lookupFunction<J2meM3gKeyframeSeqDestroyC, J2meM3gKeyframeSeqDestroyDart>('j2me_core_m3g_keyframesequence_destroy');

    coreM3gAnimCtrlCreate = _dylib.lookupFunction<J2meM3gAnimCtrlCreateC, J2meM3gAnimCtrlCreateDart>('j2me_core_m3g_animcontroller_create');
    coreM3gAnimCtrlSetActiveInterval = _dylib.lookupFunction<J2meM3gAnimCtrlSetActiveIntervalC, J2meM3gAnimCtrlSetActiveIntervalDart>('j2me_core_m3g_animcontroller_set_active_interval');
    coreM3gAnimCtrlGetActiveIntervalStart = _dylib.lookupFunction<J2meM3gAnimCtrlGetActiveIntervalC, J2meM3gAnimCtrlGetActiveIntervalDart>('j2me_core_m3g_animcontroller_get_active_interval_start');
    coreM3gAnimCtrlGetActiveIntervalEnd = _dylib.lookupFunction<J2meM3gAnimCtrlGetActiveIntervalC, J2meM3gAnimCtrlGetActiveIntervalDart>('j2me_core_m3g_animcontroller_get_active_interval_end');
    coreM3gAnimCtrlSetSpeed = _dylib.lookupFunction<J2meM3gAnimCtrlSetSpeedC, J2meM3gAnimCtrlSetSpeedDart>('j2me_core_m3g_animcontroller_set_speed');
    coreM3gAnimCtrlGetSpeed = _dylib.lookupFunction<J2meM3gAnimCtrlGetSpeedC, J2meM3gAnimCtrlGetSpeedDart>('j2me_core_m3g_animcontroller_get_speed');
    coreM3gAnimCtrlSetPosition = _dylib.lookupFunction<J2meM3gAnimCtrlSetPositionC, J2meM3gAnimCtrlSetPositionDart>('j2me_core_m3g_animcontroller_set_position');
    coreM3gAnimCtrlGetPosition = _dylib.lookupFunction<J2meM3gAnimCtrlGetPositionC, J2meM3gAnimCtrlGetPositionDart>('j2me_core_m3g_animcontroller_get_position');
    coreM3gAnimCtrlSetWeight = _dylib.lookupFunction<J2meM3gAnimCtrlSetWeightC, J2meM3gAnimCtrlSetWeightDart>('j2me_core_m3g_animcontroller_set_weight');
    coreM3gAnimCtrlGetWeight = _dylib.lookupFunction<J2meM3gAnimCtrlGetWeightC, J2meM3gAnimCtrlGetWeightDart>('j2me_core_m3g_animcontroller_get_weight');
    coreM3gAnimCtrlDestroy = _dylib.lookupFunction<J2meM3gAnimCtrlDestroyC, J2meM3gAnimCtrlDestroyDart>('j2me_core_m3g_animcontroller_destroy');

    coreM3gAnimTrackCreate = _dylib.lookupFunction<J2meM3gAnimTrackCreateC, J2meM3gAnimTrackCreateDart>('j2me_core_m3g_animtrack_create');
    coreM3gAnimTrackSetController = _dylib.lookupFunction<J2meM3gAnimTrackSetCtrlC, J2meM3gAnimTrackSetCtrlDart>('j2me_core_m3g_animtrack_set_controller');
    coreM3gAnimTrackGetController = _dylib.lookupFunction<J2meM3gAnimTrackGetCtrlC, J2meM3gAnimTrackGetCtrlDart>('j2me_core_m3g_animtrack_get_controller');
    coreM3gAnimTrackGetSequence = _dylib.lookupFunction<J2meM3gAnimTrackGetSeqC, J2meM3gAnimTrackGetSeqDart>('j2me_core_m3g_animtrack_get_sequence');
    coreM3gAnimTrackGetTargetProperty = _dylib.lookupFunction<J2meM3gAnimTrackGetPropC, J2meM3gAnimTrackGetPropDart>('j2me_core_m3g_animtrack_get_target_property');
    coreM3gAnimTrackDestroy = _dylib.lookupFunction<J2meM3gAnimTrackDestroyC, J2meM3gAnimTrackDestroyDart>('j2me_core_m3g_animtrack_destroy');

    coreM3gMorphMeshCreate = _dylib.lookupFunction<J2meM3gMorphMeshCreateC, J2meM3gMorphMeshCreateDart>('j2me_core_m3g_morphing_mesh_create');
    coreM3gMorphMeshSetWeights = _dylib.lookupFunction<J2meM3gMorphMeshSetWeightsC, J2meM3gMorphMeshSetWeightsDart>('j2me_core_m3g_morphing_mesh_set_weights');
    coreM3gMorphMeshGetWeights = _dylib.lookupFunction<J2meM3gMorphMeshGetWeightsC, J2meM3gMorphMeshGetWeightsDart>('j2me_core_m3g_morphing_mesh_get_weights');
    coreM3gMorphMeshMorph = _dylib.lookupFunction<J2meM3gMorphMeshMorphC, J2meM3gMorphMeshMorphDart>('j2me_core_m3g_morphing_mesh_morph');
    coreM3gMeshAddAnimationTrack = _dylib.lookupFunction<J2meM3gMeshAddAnimTrackC, J2meM3gMeshAddAnimTrackDart>('j2me_core_m3g_mesh_add_animation_track');
    coreM3gMeshAnimate = _dylib.lookupFunction<J2meM3gMeshAnimateC, J2meM3gMeshAnimateDart>('j2me_core_m3g_mesh_animate');
    coreM3gMeshGetPosition = _dylib.lookupFunction<J2meM3gMeshGetPosC, J2meM3gMeshGetPosDart>('j2me_core_m3g_mesh_get_position');
    coreM3gMeshGetVertexPosition = _dylib.lookupFunction<J2meM3gMeshGetVertPosC, J2meM3gMeshGetVertPosDart>('j2me_core_m3g_mesh_get_vertex_position');
    coreM3gMeshDestroy = _dylib.lookupFunction<J2meM3gMeshDestroyC, J2meM3gMeshDestroyDart>('j2me_core_m3g_mesh_destroy');

    // Section 20: M3G SkinnedMesh & Bone Skeleton
    coreM3gNodeCreate = _dylib.lookupFunction<J2meM3gNodeCreateC, J2meM3gNodeCreateDart>('j2me_core_m3g_node_create');
    coreM3gNodeSetTranslation = _dylib.lookupFunction<J2meM3gNodeSetTranslationC, J2meM3gNodeSetTranslationDart>('j2me_core_m3g_node_set_translation');
    coreM3gNodeGetTranslation = _dylib.lookupFunction<J2meM3gNodeGetTranslationC, J2meM3gNodeGetTranslationDart>('j2me_core_m3g_node_get_translation');
    coreM3gNodeSetOrientation = _dylib.lookupFunction<J2meM3gNodeSetOrientationC, J2meM3gNodeSetOrientationDart>('j2me_core_m3g_node_set_orientation');
    coreM3gNodeGetOrientation = _dylib.lookupFunction<J2meM3gNodeGetOrientationC, J2meM3gNodeGetOrientationDart>('j2me_core_m3g_node_get_orientation');
    coreM3gNodeSetScale = _dylib.lookupFunction<J2meM3gNodeSetScaleC, J2meM3gNodeSetScaleDart>('j2me_core_m3g_node_set_scale');
    coreM3gNodeGetScale = _dylib.lookupFunction<J2meM3gNodeGetScaleC, J2meM3gNodeGetScaleDart>('j2me_core_m3g_node_get_scale');
    coreM3gNodeGetGlobalTransform = _dylib.lookupFunction<J2meM3gNodeGetGlobalTransformC, J2meM3gNodeGetGlobalTransformDart>('j2me_core_m3g_node_get_global_transform');
    coreM3gNodeAddAnimationTrack = _dylib.lookupFunction<J2meM3gNodeAddAnimTrackC, J2meM3gNodeAddAnimTrackDart>('j2me_core_m3g_node_add_animation_track');
    coreM3gNodeAnimate = _dylib.lookupFunction<J2meM3gNodeAnimateC, J2meM3gNodeAnimateDart>('j2me_core_m3g_node_animate');
    coreM3gNodeDestroy = _dylib.lookupFunction<J2meM3gNodeDestroyC, J2meM3gNodeDestroyDart>('j2me_core_m3g_node_destroy');

    coreM3gGroupCreate = _dylib.lookupFunction<J2meM3gGroupCreateC, J2meM3gGroupCreateDart>('j2me_core_m3g_group_create');
    coreM3gGroupAddChild = _dylib.lookupFunction<J2meM3gGroupAddChildC, J2meM3gGroupAddChildDart>('j2me_core_m3g_group_add_child');
    coreM3gGroupRemoveChild = _dylib.lookupFunction<J2meM3gGroupRemoveChildC, J2meM3gGroupRemoveChildDart>('j2me_core_m3g_group_remove_child');
    coreM3gGroupGetChildCount = _dylib.lookupFunction<J2meM3gGroupGetChildCountC, J2meM3gGroupGetChildCountDart>('j2me_core_m3g_group_get_child_count');
    coreM3gGroupGetChild = _dylib.lookupFunction<J2meM3gGroupGetChildC, J2meM3gGroupGetChildDart>('j2me_core_m3g_group_get_child');
    coreM3gGroupAnimate = _dylib.lookupFunction<J2meM3gGroupAnimateC, J2meM3gGroupAnimateDart>('j2me_core_m3g_group_animate');
    coreM3gGroupDestroy = _dylib.lookupFunction<J2meM3gGroupDestroyC, J2meM3gGroupDestroyDart>('j2me_core_m3g_group_destroy');

    coreM3gSkinnedMeshCreate = _dylib.lookupFunction<J2meM3gSkinnedMeshCreateC, J2meM3gSkinnedMeshCreateDart>('j2me_core_m3g_skinned_mesh_create');
    coreM3gSkinnedMeshAddTransform = _dylib.lookupFunction<J2meM3gSkinnedMeshAddTransformC, J2meM3gSkinnedMeshAddTransformDart>('j2me_core_m3g_skinned_mesh_add_transform');
    coreM3gSkinnedMeshGetBoneCount = _dylib.lookupFunction<J2meM3gSkinnedMeshGetBoneCountC, J2meM3gSkinnedMeshGetBoneCountDart>('j2me_core_m3g_skinned_mesh_get_bone_count');
    coreM3gSkinnedMeshGetBone = _dylib.lookupFunction<J2meM3gSkinnedMeshGetBoneC, J2meM3gSkinnedMeshGetBoneDart>('j2me_core_m3g_skinned_mesh_get_bone');
    coreM3gSkinnedMeshGetSkeleton = _dylib.lookupFunction<J2meM3gSkinnedMeshGetSkeletonC, J2meM3gSkinnedMeshGetSkeletonDart>('j2me_core_m3g_skinned_mesh_get_skeleton');
    coreM3gSkinnedMeshSkin = _dylib.lookupFunction<J2meM3gSkinnedMeshSkinC, J2meM3gSkinnedMeshSkinDart>('j2me_core_m3g_skinned_mesh_skin');
    coreM3gSkinnedMeshAnimate = _dylib.lookupFunction<J2meM3gSkinnedMeshAnimateC, J2meM3gSkinnedMeshAnimateDart>('j2me_core_m3g_skinned_mesh_animate');
    coreM3gSkinnedMeshGetVertexPosition = _dylib.lookupFunction<J2meM3gSkinnedMeshGetVertPosC, J2meM3gSkinnedMeshGetVertPosDart>('j2me_core_m3g_skinned_mesh_get_vertex_position');
    coreM3gSkinnedMeshGetVertexNormal = _dylib.lookupFunction<J2meM3gSkinnedMeshGetVertNormC, J2meM3gSkinnedMeshGetVertNormDart>('j2me_core_m3g_skinned_mesh_get_vertex_normal');
    coreM3gSkinnedMeshDestroy = _dylib.lookupFunction<J2meM3gSkinnedMeshDestroyC, J2meM3gSkinnedMeshDestroyDart>('j2me_core_m3g_skinned_mesh_destroy');

    // Section 21: App Management, Repository & Installer
    appRepoGetCount = _dylib.lookupFunction<J2meAppRepoGetCountC, J2meAppRepoGetCountDart>('j2me_core_app_repo_get_count');
    appRepoGetItem = _dylib.lookupFunction<J2meAppRepoGetItemC, J2meAppRepoGetItemDart>('j2me_core_app_repo_get_item');
    appRepoFindById = _dylib.lookupFunction<J2meAppRepoFindByIdC, J2meAppRepoFindByIdDart>('j2me_core_app_repo_find_by_id');
    appRepoFindByPath = _dylib.lookupFunction<J2meAppRepoFindByPathC, J2meAppRepoFindByPathDart>('j2me_core_app_repo_find_by_path');
    appRepoDelete = _dylib.lookupFunction<J2meAppRepoDeleteC, J2meAppRepoDeleteDart>('j2me_core_app_repo_delete');
    appInstallerCheckJar = _dylib.lookupFunction<J2meAppInstallerCheckJarC, J2meAppInstallerCheckJarDart>('j2me_core_app_installer_check_jar');
    appInstallerInstall = _dylib.lookupFunction<J2meAppInstallerInstallC, J2meAppInstallerInstallDart>('j2me_core_app_installer_install');
    appInstallerUninstall = _dylib.lookupFunction<J2meAppInstallerUninstallC, J2meAppInstallerUninstallDart>('j2me_core_app_installer_uninstall');
    appLaunch = _dylib.lookupFunction<J2meAppLaunchC, J2meAppLaunchDart>('j2me_core_app_launch');
    appSpawn = _dylib.lookupFunction<J2meAppSpawnC, J2meAppSpawnDart>('j2me_core_app_spawn');

    // Section 22: JSR-82 Mobile Bluetooth & RFCOMM/L2CAP Multiplayer
    btIsPowerOn = _dylib.lookupFunction<J2meBtIsPowerOnC, J2meBtIsPowerOnDart>('j2me_core_bluetooth_is_power_on');
    btSetPowerOn = _dylib.lookupFunction<J2meBtSetPowerOnC, J2meBtSetPowerOnDart>('j2me_core_bluetooth_set_power_on');
    btGetLocalAddress = _dylib.lookupFunction<J2meBtGetLocalAddressC, J2meBtGetLocalAddressDart>('j2me_core_bluetooth_get_local_address');
    btSetLocalAddress = _dylib.lookupFunction<J2meBtSetLocalAddressC, J2meBtSetLocalAddressDart>('j2me_core_bluetooth_set_local_address');
    btGetLocalName = _dylib.lookupFunction<J2meBtGetLocalNameC, J2meBtGetLocalNameDart>('j2me_core_bluetooth_get_local_name');
    btSetLocalName = _dylib.lookupFunction<J2meBtSetLocalNameC, J2meBtSetLocalNameDart>('j2me_core_bluetooth_set_local_name');
    btGetDiscoverable = _dylib.lookupFunction<J2meBtGetDiscoverableC, J2meBtGetDiscoverableDart>('j2me_core_bluetooth_get_discoverable');
    btSetDiscoverable = _dylib.lookupFunction<J2meBtSetDiscoverableC, J2meBtSetDiscoverableDart>('j2me_core_bluetooth_set_discoverable');
    btGetProperty = _dylib.lookupFunction<J2meBtGetPropertyC, J2meBtGetPropertyDart>('j2me_core_bluetooth_get_property');

    btOpenBtsppServer = _dylib.lookupFunction<J2meBtOpenBtsppServerC, J2meBtOpenBtsppServerDart>('j2me_core_bluetooth_open_btspp_server');
    btBtsppAccept = _dylib.lookupFunction<J2meBtBtsppAcceptC, J2meBtBtsppAcceptDart>('j2me_core_bluetooth_btspp_accept');
    btBtsppServerClose = _dylib.lookupFunction<J2meBtBtsppServerCloseC, J2meBtBtsppServerCloseDart>('j2me_core_bluetooth_btspp_server_close');
    btOpenBtsppClient = _dylib.lookupFunction<J2meBtOpenBtsppClientC, J2meBtOpenBtsppClientDart>('j2me_core_bluetooth_open_btspp_client');
    btBtsppRead = _dylib.lookupFunction<J2meBtBtsppReadC, J2meBtBtsppReadDart>('j2me_core_bluetooth_btspp_read');
    btBtsppWrite = _dylib.lookupFunction<J2meBtBtsppWriteC, J2meBtBtsppWriteDart>('j2me_core_bluetooth_btspp_write');
    btBtsppAvailable = _dylib.lookupFunction<J2meBtBtsppAvailableC, J2meBtBtsppAvailableDart>('j2me_core_bluetooth_btspp_available');
    btBtsppClose = _dylib.lookupFunction<J2meBtBtsppCloseC, J2meBtBtsppCloseDart>('j2me_core_bluetooth_btspp_close');

    btOpenBtl2capServer = _dylib.lookupFunction<J2meBtOpenBtl2capServerC, J2meBtOpenBtl2capServerDart>('j2me_core_bluetooth_open_btl2cap_server');
    btBtl2capAccept = _dylib.lookupFunction<J2meBtBtl2capAcceptC, J2meBtBtl2capAcceptDart>('j2me_core_bluetooth_btl2cap_accept');
    btBtl2capServerClose = _dylib.lookupFunction<J2meBtBtl2capServerCloseC, J2meBtBtl2capServerCloseDart>('j2me_core_bluetooth_btl2cap_server_close');
    btOpenBtl2capClient = _dylib.lookupFunction<J2meBtOpenBtl2capClientC, J2meBtOpenBtl2capClientDart>('j2me_core_bluetooth_open_btl2cap_client');
    btBtl2capSend = _dylib.lookupFunction<J2meBtBtl2capSendC, J2meBtBtl2capSendDart>('j2me_core_bluetooth_btl2cap_send');
    btBtl2capReceive = _dylib.lookupFunction<J2meBtBtl2capReceiveC, J2meBtBtl2capReceiveDart>('j2me_core_bluetooth_btl2cap_receive');
    btBtl2capReady = _dylib.lookupFunction<J2meBtBtl2capReadyC, J2meBtBtl2capReadyDart>('j2me_core_bluetooth_btl2cap_ready');
    btBtl2capClose = _dylib.lookupFunction<J2meBtBtl2capCloseC, J2meBtBtl2capCloseDart>('j2me_core_bluetooth_btl2cap_close');

    // Section 23: Vodafone VSCL & Carrier OEM Extensions
    vodafoneSpriteCreate = _dylib.lookupFunction<J2meVodafoneSpriteCreateC, J2meVodafoneSpriteCreateDart>('j2me_core_vodafone_sprite_create');
    vodafoneSpriteDestroy = _dylib.lookupFunction<J2meVodafoneSpriteDestroyC, J2meVodafoneSpriteDestroyDart>('j2me_core_vodafone_sprite_destroy');
    vodafoneSpriteSetPalette = _dylib.lookupFunction<J2meVodafoneSpriteSetPaletteC, J2meVodafoneSpriteSetPaletteDart>('j2me_core_vodafone_sprite_set_palette');
    vodafoneSpriteGetPalette = _dylib.lookupFunction<J2meVodafoneSpriteGetPaletteC, J2meVodafoneSpriteGetPaletteDart>('j2me_core_vodafone_sprite_get_palette');
    vodafoneSpriteSetPattern = _dylib.lookupFunction<J2meVodafoneSpriteSetPatternC, J2meVodafoneSpriteSetPatternDart>('j2me_core_vodafone_sprite_set_pattern');
    vodafoneSpriteCreateCommand = _dylib.lookupFunction<J2meVodafoneSpriteCreateCommandC, J2meVodafoneSpriteCreateCommandDart>('j2me_core_vodafone_sprite_create_command');
    vodafoneSpriteCreateFramebuffer = _dylib.lookupFunction<J2meVodafoneSpriteCreateFbC, J2meVodafoneSpriteCreateFbDart>('j2me_core_vodafone_sprite_create_framebuffer');
    vodafoneSpriteDisposeFramebuffer = _dylib.lookupFunction<J2meVodafoneSpriteDisposeFbC, J2meVodafoneSpriteDisposeFbDart>('j2me_core_vodafone_sprite_dispose_framebuffer');
    vodafoneSpriteDrawChar = _dylib.lookupFunction<J2meVodafoneSpriteDrawCharC, J2meVodafoneSpriteDrawCharDart>('j2me_core_vodafone_sprite_draw_char');
    vodafoneSpriteCopyArea = _dylib.lookupFunction<J2meVodafoneSpriteCopyAreaC, J2meVodafoneSpriteCopyAreaDart>('j2me_core_vodafone_sprite_copy_area');
    vodafoneSpriteDrawFramebuffer = _dylib.lookupFunction<J2meVodafoneSpriteDrawFbC, J2meVodafoneSpriteDrawFbDart>('j2me_core_vodafone_sprite_draw_framebuffer');
    vodafoneSpriteGetFramebuffer = _dylib.lookupFunction<J2meVodafoneSpriteGetFbC, J2meVodafoneSpriteGetFbDart>('j2me_core_vodafone_sprite_get_framebuffer');

    vodafoneDeviceGetState = _dylib.lookupFunction<J2meVodafoneDeviceGetStateC, J2meVodafoneDeviceGetStateDart>('j2me_core_vodafone_device_get_state');
    vodafoneDeviceIsActive = _dylib.lookupFunction<J2meVodafoneDeviceIsActiveC, J2meVodafoneDeviceIsActiveDart>('j2me_core_vodafone_device_is_active');
    vodafoneDeviceSetActive = _dylib.lookupFunction<J2meVodafoneDeviceSetActiveC, J2meVodafoneDeviceSetActiveDart>('j2me_core_vodafone_device_set_active');
    vodafoneDeviceBlink = _dylib.lookupFunction<J2meVodafoneDeviceBlinkC, J2meVodafoneDeviceBlinkDart>('j2me_core_vodafone_device_blink');
    vodafoneDeviceGetKeystates = _dylib.lookupFunction<J2meVodafoneDeviceGetKeystatesC, J2meVodafoneDeviceGetKeystatesDart>('j2me_core_vodafone_device_get_keystates');
    vodafoneDeviceSetKeystatesMask = _dylib.lookupFunction<J2meVodafoneDeviceSetKeystatesMaskC, J2meVodafoneDeviceSetKeystatesMaskDart>('j2me_core_vodafone_device_set_keystates_mask');
    vodafoneDeviceKeyEvent = _dylib.lookupFunction<J2meVodafoneDeviceKeyEventC, J2meVodafoneDeviceKeyEventDart>('j2me_core_vodafone_device_key_event');

    vodafoneEncodeOffscreen = _dylib.lookupFunction<J2meVodafoneEncodeOffscreenC, J2meVodafoneEncodeOffscreenDart>('j2me_core_vodafone_encode_offscreen');

    carrierKddiGetKeyState = _dylib.lookupFunction<J2meCarrierKddiGetKeyStateC, J2meCarrierKddiGetKeyStateDart>('j2me_core_carrier_kddi_get_keystate');
    carrierMotorolaFunlightSetColor = _dylib.lookupFunction<J2meCarrierMotorolaFunlightSetColorC, J2meCarrierMotorolaFunlightSetColorDart>('j2me_core_carrier_motorola_funlight_set_color');
    carrierMotorolaFunlightGetColor = _dylib.lookupFunction<J2meCarrierMotorolaFunlightGetColorC, J2meCarrierMotorolaFunlightGetColorDart>('j2me_core_carrier_motorola_funlight_get_color');
    carrierSonyAccelSet = _dylib.lookupFunction<J2meCarrierSonyAccelSetC, J2meCarrierSonyAccelSetDart>('j2me_core_carrier_sony_accel_set');
    carrierSonyAccelGet = _dylib.lookupFunction<J2meCarrierSonyAccelGetC, J2meCarrierSonyAccelGetDart>('j2me_core_carrier_sony_accel_get');
    carrierSprintPlayClip = _dylib.lookupFunction<J2meCarrierSprintPlayClipC, J2meCarrierSprintPlayClipDart>('j2me_core_carrier_sprint_play_clip');
    carrierSprintStop = _dylib.lookupFunction<J2meCarrierSprintStopC, J2meCarrierSprintStopDart>('j2me_core_carrier_sprint_stop');
    carrierSprintIsPlaying = _dylib.lookupFunction<J2meCarrierSprintIsPlayingC, J2meCarrierSprintIsPlayingDart>('j2me_core_carrier_sprint_is_playing');

    // Section 24: JSR-179 Mobile Location API
    locationProviderGetState = _dylib.lookupFunction<J2meLocationProviderGetStateC, J2meLocationProviderGetStateDart>('j2me_core_location_provider_get_state');
    locationProviderSetState = _dylib.lookupFunction<J2meLocationProviderSetStateC, J2meLocationProviderSetStateDart>('j2me_core_location_provider_set_state');
    locationProviderUpdateHostLocation = _dylib.lookupFunction<J2meLocationProviderUpdateHostLocationC, J2meLocationProviderUpdateHostLocationDart>('j2me_core_location_provider_update_host_location');
    locationProviderGetLastKnown = _dylib.lookupFunction<J2meLocationProviderGetLastKnownC, J2meLocationProviderGetLastKnownDart>('j2me_core_location_provider_get_last_known');
    locationProviderGetNmea = _dylib.lookupFunction<J2meLocationProviderGetNmeaC, J2meLocationProviderGetNmeaDart>('j2me_core_location_provider_get_nmea');

    locationCoordinatesDistance = _dylib.lookupFunction<J2meLocationCoordinatesDistanceC, J2meLocationCoordinatesDistanceDart>('j2me_core_location_coordinates_distance');
    locationCoordinatesAzimuth = _dylib.lookupFunction<J2meLocationCoordinatesAzimuthC, J2meLocationCoordinatesAzimuthDart>('j2me_core_location_coordinates_azimuth');
    locationCoordinatesConvertToString = _dylib.lookupFunction<J2meLocationCoordinatesConvertToStringC, J2meLocationCoordinatesConvertToStringDart>('j2me_core_location_coordinates_convert_to_string');
    locationCoordinatesConvertFromString = _dylib.lookupFunction<J2meLocationCoordinatesConvertFromStringC, J2meLocationCoordinatesConvertFromStringDart>('j2me_core_location_coordinates_convert_from_string');

    locationOrientationGet = _dylib.lookupFunction<J2meLocationOrientationGetC, J2meLocationOrientationGetDart>('j2me_core_location_orientation_get');
    locationOrientationSet = _dylib.lookupFunction<J2meLocationOrientationSetC, J2meLocationOrientationSetDart>('j2me_core_location_orientation_set');

    locationLandmarkStoreCreate = _dylib.lookupFunction<J2meLocationLandmarkStoreCreateC, J2meLocationLandmarkStoreCreateDart>('j2me_core_location_landmark_store_create');
    locationLandmarkStoreDelete = _dylib.lookupFunction<J2meLocationLandmarkStoreDeleteC, J2meLocationLandmarkStoreDeleteDart>('j2me_core_location_landmark_store_delete');
    locationLandmarkStoreAddLandmark = _dylib.lookupFunction<J2meLocationLandmarkStoreAddLandmarkC, J2meLocationLandmarkStoreAddLandmarkDart>('j2me_core_location_landmark_store_add_landmark');
    locationLandmarkStoreGetCount = _dylib.lookupFunction<J2meLocationLandmarkStoreGetCountC, J2meLocationLandmarkStoreGetCountDart>('j2me_core_location_landmark_store_get_count');

    // Section 25: JSR-256 Mobile Sensor API
    sensorGetCount = _dylib.lookupFunction<J2meSensorGetCountC, J2meSensorGetCountDart>('j2me_core_sensor_get_count');
    sensorGetUrl = _dylib.lookupFunction<J2meSensorGetUrlC, J2meSensorGetUrlDart>('j2me_core_sensor_get_url');
    sensorGetQuantity = _dylib.lookupFunction<J2meSensorGetQuantityC, J2meSensorGetQuantityDart>('j2me_core_sensor_get_quantity');
    sensorGetContextType = _dylib.lookupFunction<J2meSensorGetContextTypeC, J2meSensorGetContextTypeDart>('j2me_core_sensor_get_context_type');
    sensorFind = _dylib.lookupFunction<J2meSensorFindC, J2meSensorFindDart>('j2me_core_sensor_find');

    sensorOpen = _dylib.lookupFunction<J2meSensorOpenC, J2meSensorOpenDart>('j2me_core_sensor_open');
    sensorClose = _dylib.lookupFunction<J2meSensorCloseC, J2meSensorCloseDart>('j2me_core_sensor_close');
    sensorGetState = _dylib.lookupFunction<J2meSensorGetStateC, J2meSensorGetStateDart>('j2me_core_sensor_get_state');
    sensorGetChannelCount = _dylib.lookupFunction<J2meSensorGetChannelCountC, J2meSensorGetChannelCountDart>('j2me_core_sensor_get_channel_count');
    sensorGetChannelName = _dylib.lookupFunction<J2meSensorGetChannelNameC, J2meSensorGetChannelNameDart>('j2me_core_sensor_get_channel_name');
    sensorGetData = _dylib.lookupFunction<J2meSensorGetDataC, J2meSensorGetDataDart>('j2me_core_sensor_get_data');

    sensorUpdateAccelerometer = _dylib.lookupFunction<J2meSensorUpdate3DC, J2meSensorUpdate3DDart>('j2me_core_sensor_update_accelerometer');
    sensorUpdateAmbientLight = _dylib.lookupFunction<J2meSensorUpdate1DC, J2meSensorUpdate1DDart>('j2me_core_sensor_update_ambient_light');
    sensorUpdateMagneticField = _dylib.lookupFunction<J2meSensorUpdate3DC, J2meSensorUpdate3DDart>('j2me_core_sensor_update_magnetic_field');
    sensorUpdateOrientation = _dylib.lookupFunction<J2meSensorUpdate3DC, J2meSensorUpdate3DDart>('j2me_core_sensor_update_orientation');
    sensorUpdateTemperature = _dylib.lookupFunction<J2meSensorUpdate1DC, J2meSensorUpdate1DDart>('j2me_core_sensor_update_temperature');

    // Section 26: JSR-75 PIM
    pimInit = _dylib.lookupFunction<J2mePimInitC, J2mePimInitDart>('j2me_core_pim_init');
    pimListCount = _dylib.lookupFunction<J2mePimListCountC, J2mePimListCountDart>('j2me_core_pim_list_count');
    pimListGetName = _dylib.lookupFunction<J2mePimListGetNameC, J2mePimListGetNameDart>('j2me_core_pim_list_get_name');
    pimOpenList = _dylib.lookupFunction<J2mePimOpenListC, J2mePimOpenListDart>('j2me_core_pim_open_list');
    pimCloseList = _dylib.lookupFunction<J2mePimCloseListC, J2mePimCloseListDart>('j2me_core_pim_close_list');
    pimListGetItemCount = _dylib.lookupFunction<J2mePimListGetItemCountC, J2mePimListGetItemCountDart>('j2me_core_pim_list_get_item_count');
    pimListGetItem = _dylib.lookupFunction<J2mePimListGetItemC, J2mePimListGetItemDart>('j2me_core_pim_list_get_item');
    pimListRemoveItem = _dylib.lookupFunction<J2mePimListRemoveItemC, J2mePimListRemoveItemDart>('j2me_core_pim_list_remove_item');

    pimContactCreate = _dylib.lookupFunction<J2mePimContactCreateC, J2mePimContactCreateDart>('j2me_core_pim_contact_create');
    pimContactSetName = _dylib.lookupFunction<J2mePimContactSetNameC, J2mePimContactSetNameDart>('j2me_core_pim_contact_set_name');
    pimContactGetFormattedName = _dylib.lookupFunction<J2mePimContactGetFormattedNameC, J2mePimContactGetFormattedNameDart>('j2me_core_pim_contact_get_formatted_name');
    pimContactAddTel = _dylib.lookupFunction<J2mePimContactAddTelC, J2mePimContactAddTelDart>('j2me_core_pim_contact_add_tel');
    pimContactGetTelCount = _dylib.lookupFunction<J2mePimContactGetTelCountC, J2mePimContactGetTelCountDart>('j2me_core_pim_contact_get_tel_count');
    pimContactGetTel = _dylib.lookupFunction<J2mePimContactGetTelC, J2mePimContactGetTelDart>('j2me_core_pim_contact_get_tel');
    pimContactAddEmail = _dylib.lookupFunction<J2mePimContactAddEmailC, J2mePimContactAddEmailDart>('j2me_core_pim_contact_add_email');
    pimContactGetEmailCount = _dylib.lookupFunction<J2mePimContactGetEmailCountC, J2mePimContactGetEmailCountDart>('j2me_core_pim_contact_get_email_count');
    pimContactGetEmail = _dylib.lookupFunction<J2mePimContactGetEmailC, J2mePimContactGetEmailDart>('j2me_core_pim_contact_get_email');
    pimContactSetAddress = _dylib.lookupFunction<J2mePimContactSetAddressC, J2mePimContactSetAddressDart>('j2me_core_pim_contact_set_address');

    pimEventCreate = _dylib.lookupFunction<J2mePimEventCreateC, J2mePimEventCreateDart>('j2me_core_pim_event_create');
    pimEventSetDetails = _dylib.lookupFunction<J2mePimEventSetDetailsC, J2mePimEventSetDetailsDart>('j2me_core_pim_event_set_details');
    pimEventGetDetails = _dylib.lookupFunction<J2mePimEventGetDetailsC, J2mePimEventGetDetailsDart>('j2me_core_pim_event_get_details');
    pimEventSetRepeatRule = _dylib.lookupFunction<J2mePimEventSetRepeatRuleC, J2mePimEventSetRepeatRuleDart>('j2me_core_pim_event_set_repeat_rule');

    pimTodoCreate = _dylib.lookupFunction<J2mePimTodoCreateC, J2mePimTodoCreateDart>('j2me_core_pim_todo_create');
    pimTodoSetDetails = _dylib.lookupFunction<J2mePimTodoSetDetailsC, J2mePimTodoSetDetailsDart>('j2me_core_pim_todo_set_details');
    pimTodoGetDetails = _dylib.lookupFunction<J2mePimTodoGetDetailsC, J2mePimTodoGetDetailsDart>('j2me_core_pim_todo_get_details');

    pimItemCommit = _dylib.lookupFunction<J2mePimItemCommitC, J2mePimItemCommitDart>('j2me_core_pim_item_commit');
    pimItemAddCategory = _dylib.lookupFunction<J2mePimItemAddCategoryC, J2mePimItemAddCategoryDart>('j2me_core_pim_item_add_category');
    pimItemGetCategoryCount = _dylib.lookupFunction<J2mePimItemGetCategoryCountC, J2mePimItemGetCategoryCountDart>('j2me_core_pim_item_get_category_count');
    pimItemGetCategory = _dylib.lookupFunction<J2mePimItemGetCategoryC, J2mePimItemGetCategoryDart>('j2me_core_pim_item_get_category');

    pimExportSerial = _dylib.lookupFunction<J2mePimExportSerialC, J2mePimExportSerialDart>('j2me_core_pim_export_serial');
    pimImportSerial = _dylib.lookupFunction<J2mePimImportSerialC, J2mePimImportSerialDart>('j2me_core_pim_import_serial');
    pimSaveAll = _dylib.lookupFunction<J2mePimSaveAllC, J2mePimSaveAllDart>('j2me_core_pim_save_all');

    // Section 27: JSR-234 AMMS
    ammsSpectatorSetLocation = _dylib.lookupFunction<J2meAmmsSpectatorSetLocationC, J2meAmmsSpectatorSetLocationDart>('j2me_core_amms_spectator_set_location');
    ammsSpectatorGetLocation = _dylib.lookupFunction<J2meAmmsSpectatorGetLocationC, J2meAmmsSpectatorGetLocationDart>('j2me_core_amms_spectator_get_location');
    ammsSpectatorSetOrientation = _dylib.lookupFunction<J2meAmmsSpectatorSetOrientationC, J2meAmmsSpectatorSetOrientationDart>('j2me_core_amms_spectator_set_orientation');
    ammsSpectatorGetOrientation = _dylib.lookupFunction<J2meAmmsSpectatorGetOrientationC, J2meAmmsSpectatorGetOrientationDart>('j2me_core_amms_spectator_get_orientation');

    ammsSoundSourceCreate = _dylib.lookupFunction<J2meAmmsSoundSourceCreateC, J2meAmmsSoundSourceCreateDart>('j2me_core_amms_sound_source_create');
    ammsSoundSourceDestroy = _dylib.lookupFunction<J2meAmmsSoundSourceDestroyC, J2meAmmsSoundSourceDestroyDart>('j2me_core_amms_sound_source_destroy');
    ammsSoundSourceSetLocation = _dylib.lookupFunction<J2meAmmsSoundSourceSetLocationC, J2meAmmsSoundSourceSetLocationDart>('j2me_core_amms_sound_source_set_location');
    ammsSoundSourceGetLocation = _dylib.lookupFunction<J2meAmmsSoundSourceGetLocationC, J2meAmmsSoundSourceGetLocationDart>('j2me_core_amms_sound_source_get_location');
    ammsSoundSourceSetVelocity = _dylib.lookupFunction<J2meAmmsSoundSourceSetVelocityC, J2meAmmsSoundSourceSetVelocityDart>('j2me_core_amms_sound_source_set_velocity');
    ammsSoundSourceSetAttenuation = _dylib.lookupFunction<J2meAmmsSoundSourceSetAttenuationC, J2meAmmsSoundSourceSetAttenuationDart>('j2me_core_amms_sound_source_set_attenuation');
    ammsSoundSourceEvaluate = _dylib.lookupFunction<J2meAmmsSoundSourceEvaluateC, J2meAmmsSoundSourceEvaluateDart>('j2me_core_amms_sound_source_evaluate');

    ammsEffectModuleCreate = _dylib.lookupFunction<J2meAmmsEffectModuleCreateC, J2meAmmsEffectModuleCreateDart>('j2me_core_amms_effect_module_create');
    ammsEffectModuleDestroy = _dylib.lookupFunction<J2meAmmsEffectModuleDestroyC, J2meAmmsEffectModuleDestroyDart>('j2me_core_amms_effect_module_destroy');
    ammsReverbSetLevel = _dylib.lookupFunction<J2meAmmsReverbSetLevelC, J2meAmmsReverbSetLevelDart>('j2me_core_amms_reverb_set_level');
    ammsReverbGetLevel = _dylib.lookupFunction<J2meAmmsReverbGetLevelC, J2meAmmsReverbGetLevelDart>('j2me_core_amms_reverb_get_level');
    ammsReverbSetPreset = _dylib.lookupFunction<J2meAmmsReverbSetPresetC, J2meAmmsReverbSetPresetDart>('j2me_core_amms_reverb_set_preset');
    ammsReverbGetPreset = _dylib.lookupFunction<J2meAmmsReverbGetPresetC, J2meAmmsReverbGetPresetDart>('j2me_core_amms_reverb_get_preset');
    ammsEqualizerSetBandLevel = _dylib.lookupFunction<J2meAmmsEqualizerSetBandLevelC, J2meAmmsEqualizerSetBandLevelDart>('j2me_core_amms_equalizer_set_band_level');
    ammsEqualizerGetBandLevel = _dylib.lookupFunction<J2meAmmsEqualizerGetBandLevelC, J2meAmmsEqualizerGetBandLevelDart>('j2me_core_amms_equalizer_get_band_level');
    ammsEqualizerGetBandCount = _dylib.lookupFunction<J2meAmmsEqualizerGetBandCountC, J2meAmmsEqualizerGetBandCountDart>('j2me_core_amms_equalizer_get_band_count');
    ammsEqualizerSetPreset = _dylib.lookupFunction<J2meAmmsEqualizerSetPresetC, J2meAmmsEqualizerSetPresetDart>('j2me_core_amms_equalizer_set_preset');
    ammsPanSet = _dylib.lookupFunction<J2meAmmsPanSetC, J2meAmmsPanSetDart>('j2me_core_amms_pan_set');
    ammsPanGet = _dylib.lookupFunction<J2meAmmsPanGetC, J2meAmmsPanGetDart>('j2me_core_amms_pan_get');

    ammsCameraSetRotation = _dylib.lookupFunction<J2meAmmsCameraSetRotationC, J2meAmmsCameraSetRotationDart>('j2me_core_amms_camera_set_rotation');
    ammsCameraGetRotation = _dylib.lookupFunction<J2meAmmsCameraGetRotationC, J2meAmmsCameraGetRotationDart>('j2me_core_amms_camera_get_rotation');
    ammsCameraSetExposureMode = _dylib.lookupFunction<J2meAmmsCameraSetExposureModeC, J2meAmmsCameraSetExposureModeDart>('j2me_core_amms_camera_set_exposure_mode');
    ammsCameraGetExposureMode = _dylib.lookupFunction<J2meAmmsCameraGetExposureModeC, J2meAmmsCameraGetExposureModeDart>('j2me_core_amms_camera_get_exposure_mode');
    ammsFlashSetMode = _dylib.lookupFunction<J2meAmmsFlashSetModeC, J2meAmmsFlashSetModeDart>('j2me_core_amms_flash_set_mode');
    ammsFlashGetMode = _dylib.lookupFunction<J2meAmmsFlashGetModeC, J2meAmmsFlashGetModeDart>('j2me_core_amms_flash_get_mode');
    ammsZoomSetDigital = _dylib.lookupFunction<J2meAmmsZoomSetDigitalC, J2meAmmsZoomSetDigitalDart>('j2me_core_amms_zoom_set_digital');
    ammsZoomGetDigital = _dylib.lookupFunction<J2meAmmsZoomGetDigitalC, J2meAmmsZoomGetDigitalDart>('j2me_core_amms_zoom_get_digital');
    ammsImageTransformSetCrop = _dylib.lookupFunction<J2meAmmsImageTransformSetCropC, J2meAmmsImageTransformSetCropDart>('j2me_core_amms_image_transform_set_crop');
    ammsImageTransformSetTarget = _dylib.lookupFunction<J2meAmmsImageTransformSetTargetC, J2meAmmsImageTransformSetTargetDart>('j2me_core_amms_image_transform_set_target');

    // Section 28: MIDP 2.0 PushRegistry & CommConnection
    pushRegisterConnection = _dylib.lookupFunction<J2mePushRegisterConnectionC, J2mePushRegisterConnectionDart>('j2me_core_push_register_connection');
    pushUnregisterConnection = _dylib.lookupFunction<J2mePushUnregisterConnectionC, J2mePushUnregisterConnectionDart>('j2me_core_push_unregister_connection');
    pushListConnections = _dylib.lookupFunction<J2mePushListConnectionsC, J2mePushListConnectionsDart>('j2me_core_push_list_connections');
    pushGetMIDlet = _dylib.lookupFunction<J2mePushGetMIDletC, J2mePushGetMIDletDart>('j2me_core_push_get_midlet');
    pushGetFilter = _dylib.lookupFunction<J2mePushGetFilterC, J2mePushGetFilterDart>('j2me_core_push_get_filter');
    pushRegisterAlarm = _dylib.lookupFunction<J2mePushRegisterAlarmC, J2mePushRegisterAlarmDart>('j2me_core_push_register_alarm');
    pushNotifyInbound = _dylib.lookupFunction<J2mePushNotifyInboundC, J2mePushNotifyInboundDart>('j2me_core_push_notify_inbound');
    pushCheckAlarms = _dylib.lookupFunction<J2mePushCheckAlarmsC, J2mePushCheckAlarmsDart>('j2me_core_push_check_alarms');
    pushSave = _dylib.lookupFunction<J2mePushSaveC, J2mePushSaveDart>('j2me_core_push_save');
    pushLoad = _dylib.lookupFunction<J2mePushLoadC, J2mePushLoadDart>('j2me_core_push_load');

    commOpen = _dylib.lookupFunction<J2meCommOpenC, J2meCommOpenDart>('j2me_core_comm_open');
    commClose = _dylib.lookupFunction<J2meCommCloseC, J2meCommCloseDart>('j2me_core_comm_close');
    commGetBaudRate = _dylib.lookupFunction<J2meCommGetBaudRateC, J2meCommGetBaudRateDart>('j2me_core_comm_get_baud_rate');
    commSetBaudRate = _dylib.lookupFunction<J2meCommSetBaudRateC, J2meCommSetBaudRateDart>('j2me_core_comm_set_baud_rate');
    commWrite = _dylib.lookupFunction<J2meCommWriteC, J2meCommWriteDart>('j2me_core_comm_write');
    commRead = _dylib.lookupFunction<J2meCommReadC, J2meCommReadDart>('j2me_core_comm_read');
    commAvailable = _dylib.lookupFunction<J2meCommAvailableC, J2meCommAvailableDart>('j2me_core_comm_available');

    // Section 29: PKI Security, SSL & HTTPS
    certCreate = _dylib.lookupFunction<J2meCertCreateC, J2meCertCreateDart>('j2me_core_cert_create');
    certDestroy = _dylib.lookupFunction<J2meCertDestroyC, J2meCertDestroyDart>('j2me_core_cert_destroy');
    certGetSubject = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_subject');
    certGetIssuer = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_issuer');
    certGetType = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_type');
    certGetVersion = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_version');
    certGetSigAlg = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_sig_alg');
    certGetNotBefore = _dylib.lookupFunction<J2meCertGetTimeC, J2meCertGetTimeDart>('j2me_core_cert_get_not_before');
    certGetNotAfter = _dylib.lookupFunction<J2meCertGetTimeC, J2meCertGetTimeDart>('j2me_core_cert_get_not_after');
    certGetSerial = _dylib.lookupFunction<J2meCertGetStringC, J2meCertGetStringDart>('j2me_core_cert_get_serial');
    certValidate = _dylib.lookupFunction<J2meCertValidateC, J2meCertValidateDart>('j2me_core_cert_validate');

    sslOpen = _dylib.lookupFunction<J2meSslOpenC, J2meSslOpenDart>('j2me_core_ssl_open');
    sslClose = _dylib.lookupFunction<J2meSslCloseC, J2meSslCloseDart>('j2me_core_ssl_close');
    sslIsOpen = _dylib.lookupFunction<J2meSslIsOpenC, J2meSslIsOpenDart>('j2me_core_ssl_is_open');
    sslGetPort = _dylib.lookupFunction<J2meSslGetPortC, J2meSslGetPortDart>('j2me_core_ssl_get_port');
    sslWrite = _dylib.lookupFunction<J2meSslWriteC, J2meSslWriteDart>('j2me_core_ssl_write');
    sslRead = _dylib.lookupFunction<J2meSslReadC, J2meSslReadDart>('j2me_core_ssl_read');
    sslAvailable = _dylib.lookupFunction<J2meSslAvailableC, J2meSslAvailableDart>('j2me_core_ssl_available');
    sslFeedInput = _dylib.lookupFunction<J2meSslFeedInputC, J2meSslFeedInputDart>('j2me_core_ssl_feed_input');
    sslGetSecurityInfo = _dylib.lookupFunction<J2meSslGetSecurityInfoC, J2meSslGetSecurityInfoDart>('j2me_core_ssl_get_security_info');

    httpsOpen = _dylib.lookupFunction<J2meHttpsOpenC, J2meHttpsOpenDart>('j2me_core_https_open');
    httpsClose = _dylib.lookupFunction<J2meHttpsCloseC, J2meHttpsCloseDart>('j2me_core_https_close');
    httpsIsOpen = _dylib.lookupFunction<J2meHttpsIsOpenC, J2meHttpsIsOpenDart>('j2me_core_https_is_open');
    httpsSetMethod = _dylib.lookupFunction<J2meHttpsSetMethodC, J2meHttpsSetMethodDart>('j2me_core_https_set_method');
    httpsSetRequestProperty = _dylib.lookupFunction<J2meHttpsSetRequestPropC, J2meHttpsSetRequestPropDart>('j2me_core_https_set_request_property');
    httpsGetResponseCode = _dylib.lookupFunction<J2meHttpsGetCodeC, J2meHttpsGetCodeDart>('j2me_core_https_get_response_code');
    httpsGetPort = _dylib.lookupFunction<J2meHttpsGetPortC, J2meHttpsGetPortDart>('j2me_core_https_get_port');
    httpsSetResponseHeader = _dylib.lookupFunction<J2meHttpsSetResponseHeaderC, J2meHttpsSetResponseHeaderDart>('j2me_core_https_set_response_header');
    httpsGetHeaderField = _dylib.lookupFunction<J2meHttpsGetHeaderFieldC, J2meHttpsGetHeaderFieldDart>('j2me_core_https_get_header_field');
    httpsFeedResponse = _dylib.lookupFunction<J2meHttpsFeedRespC, J2meHttpsFeedRespDart>('j2me_core_https_feed_response');
    httpsRead = _dylib.lookupFunction<J2meHttpsReadC, J2meHttpsReadDart>('j2me_core_https_read');
    httpsWrite = _dylib.lookupFunction<J2meHttpsWriteC, J2meHttpsWriteDart>('j2me_core_https_write');

    // Section 30: 3D Binary Asset Loaders (M3G & Micro3D)
    identify3dFormat = _dylib.lookupFunction<J2me3dIdentifyFormatC, J2me3dIdentifyFormatDart>('j2me_core_3d_identify_format');
    m3gLoadMemory = _dylib.lookupFunction<J2meM3gLoadMemoryC, J2meM3gLoadMemoryDart>('j2me_core_m3g_load_memory');
    m3gSceneDestroy = _dylib.lookupFunction<J2meM3gSceneDestroyC, J2meM3gSceneDestroyDart>('j2me_core_m3g_scene_destroy');
    micro3dLoadFigure = _dylib.lookupFunction<J2meMicro3dLoadFigureC, J2meMicro3dLoadFigureDart>('j2me_core_micro3d_load_figure');
    micro3dFigureDestroy = _dylib.lookupFunction<J2meMicro3dFigureDestroyC, J2meMicro3dFigureDestroyDart>('j2me_core_micro3d_figure_destroy');
    micro3dFigureGetCounts = _dylib.lookupFunction<J2meMicro3dFigureGetCountsC, J2meMicro3dFigureGetCountsDart>('j2me_core_micro3d_figure_get_counts');
    micro3dLoadActionTable = _dylib.lookupFunction<J2meMicro3dLoadActionTableC, J2meMicro3dLoadActionTableDart>('j2me_core_micro3d_load_action_table');
    micro3dActionTableDestroy = _dylib.lookupFunction<J2meMicro3dActionTableDestroyC, J2meMicro3dActionTableDestroyDart>('j2me_core_micro3d_action_table_destroy');
    micro3dActionTableGetCount = _dylib.lookupFunction<J2meMicro3dActionTableGetCountC, J2meMicro3dActionTableGetCountDart>('j2me_core_micro3d_action_table_get_count');

    // Section 31: LCDUI Font Engine, System Properties & PCM WAV Player
    fontGetDefault = _dylib.lookupFunction<J2meFontGetDefaultC, J2meFontGetDefaultDart>('j2me_core_font_get_default');
    fontGet = _dylib.lookupFunction<J2meFontGetC, J2meFontGetDart>('j2me_core_font_get');
    fontGetHeight = _dylib.lookupFunction<J2meFontGetIntC, J2meFontGetIntDart>('j2me_core_font_get_height');
    fontGetBaseline = _dylib.lookupFunction<J2meFontGetIntC, J2meFontGetIntDart>('j2me_core_font_get_baseline');
    fontCharWidth = _dylib.lookupFunction<J2meFontCharWidthC, J2meFontCharWidthDart>('j2me_core_font_char_width');
    fontStringWidth = _dylib.lookupFunction<J2meFontStringWidthC, J2meFontStringWidthDart>('j2me_core_font_string_width');
    fontGetFace = _dylib.lookupFunction<J2meFontGetIntC, J2meFontGetIntDart>('j2me_core_font_get_face');
    fontGetStyle = _dylib.lookupFunction<J2meFontGetIntC, J2meFontGetIntDart>('j2me_core_font_get_style');
    fontGetSize = _dylib.lookupFunction<J2meFontGetIntC, J2meFontGetIntDart>('j2me_core_font_get_size');
    graphicsSetFont = _dylib.lookupFunction<J2meGraphicsSetFontC, J2meGraphicsSetFontDart>('j2me_core_graphics_set_font');
    graphicsGetFont = _dylib.lookupFunction<J2meGraphicsGetFontC, J2meGraphicsGetFontDart>('j2me_core_graphics_get_font');
    graphicsDrawString = _dylib.lookupFunction<J2meGraphicsDrawStringC, J2meGraphicsDrawStringDart>('j2me_core_graphics_draw_string');
    graphicsDrawChar = _dylib.lookupFunction<J2meGraphicsDrawCharC, J2meGraphicsDrawCharDart>('j2me_core_graphics_draw_char');

    systemGetProperty = _dylib.lookupFunction<J2meSystemGetPropertyC, J2meSystemGetPropertyDart>('j2me_core_system_get_property');
    systemSetProperty = _dylib.lookupFunction<J2meSystemSetPropertyC, J2meSystemSetPropertyDart>('j2me_core_system_set_property');
    systemHasProperty = _dylib.lookupFunction<J2meSystemHasPropertyC, J2meSystemHasPropertyDart>('j2me_core_system_has_property');
    systemResetProperties = _dylib.lookupFunction<J2meSystemResetPropertiesC, J2meSystemResetPropertiesDart>('j2me_core_system_reset_properties');
    systemGetPropertyCount = _dylib.lookupFunction<J2meSystemGetCountC, J2meSystemGetCountDart>('j2me_core_system_get_property_count');
    systemLoadProperties = _dylib.lookupFunction<J2meSystemLoadPropertiesC, J2meSystemLoadPropertiesDart>('j2me_core_system_load_properties');

    wavCreateMemory = _dylib.lookupFunction<J2meWavCreateMemoryC, J2meWavCreateMemoryDart>('j2me_core_wav_create_memory');
    wavCreateFile = _dylib.lookupFunction<J2meWavCreateFileC, J2meWavCreateFileDart>('j2me_core_wav_create_file');
    wavStart = _dylib.lookupFunction<J2meWavActionC, J2meWavActionDart>('j2me_core_wav_start');
    wavStop = _dylib.lookupFunction<J2meWavActionC, J2meWavActionDart>('j2me_core_wav_stop');
    wavSetLoop = _dylib.lookupFunction<J2meWavSetIntC, J2meWavSetIntDart>('j2me_core_wav_set_loop');
    wavSetVolume = _dylib.lookupFunction<J2meWavSetIntC, J2meWavSetIntDart>('j2me_core_wav_set_volume');
    wavGetVolume = _dylib.lookupFunction<J2meWavGetIntC, J2meWavGetIntDart>('j2me_core_wav_get_volume');
    wavGetDurationUs = _dylib.lookupFunction<J2meWavGetTimeC, J2meWavGetTimeDart>('j2me_core_wav_get_duration_us');
    wavGetMediaTimeUs = _dylib.lookupFunction<J2meWavGetTimeC, J2meWavGetTimeDart>('j2me_core_wav_get_media_time_us');
    wavSetMediaTimeUs = _dylib.lookupFunction<J2meWavSetTimeC, J2meWavSetTimeDart>('j2me_core_wav_set_media_time_us');
    wavRenderPcm = _dylib.lookupFunction<J2meWavRenderPcmC, J2meWavRenderPcmDart>('j2me_core_wav_render_pcm');
    wavDestroy = _dylib.lookupFunction<J2meWavActionC, J2meWavActionDart>('j2me_core_wav_destroy');
    platformPickFile = _dylib.lookupFunction<J2mePlatformPickFileC, J2mePlatformPickFileDart>('j2me_core_platform_pick_file');
    platformSetWindowSize = _dylib.lookupFunction<J2mePlatformSetWindowSizeC, J2mePlatformSetWindowSizeDart>('j2me_core_platform_set_window_size');
    platformGetWindowSize = _dylib.lookupFunction<J2mePlatformGetWindowSizeC, J2mePlatformGetWindowSizeDart>('j2me_core_platform_get_window_size');
  }
}
