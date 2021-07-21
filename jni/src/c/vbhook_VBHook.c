#include "vbhook_VBHook.h"
#include "VBoxCAPIGlue.h"
#include "VBoxCAPI_v6_1.h"
#include <stdio.h>
#include <stdlib.h>

JNIEXPORT jlong JNICALL Java_vbhook_VBHook_create_1vb_1client (JNIEnv* env, jobject obj) {
	IVirtualBoxClient* vboxClient = NULL;
	g_pVBoxFuncs->pfnClientInitialize(NULL, &vboxClient);
	return (jlong)vboxClient;
}

JNIEXPORT jlong JNICALL Java_vbhook_VBHook_create_1vb(JNIEnv* env, jobject obj, jlong lng) {
	IVirtualBoxClient* vboxClient = (IVirtualBoxClient*) lng;
	IVirtualBox* vbox = NULL;
	IVirtualBoxClient_get_VirtualBox(vboxClient, &vbox);
	return (jlong)vbox;
}

JNIEXPORT jlong JNICALL Java_vbhook_VBHook_create_1session(JNIEnv* env, jobject obj, jlong lng) {
	ISession* session = NULL;
	IVirtualBoxClient* vboxClient = (IVirtualBoxClient*) lng;
	IVirtualBoxClient_GetSession(vboxClient, &session);
	return (jlong) session;
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_free_1vm(JNIEnv* env, jobject obj, jlong lng) {
	if(lng) IMachine_Release((IMachine*)lng);
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_free_1vb(JNIEnv* env, jobject obj, jlong lng) {
	if(lng) IVirtualBox_Release((IVirtualBox*)lng);
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_free_1session(JNIEnv* env, jobject obj, jlong lng) {
	if(lng) ISession_Release((ISession*)lng);
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_free_1vb_1client(JNIEnv* env, jobject obj, jlong lng) {
	if(lng) IVirtualBoxClient_Release((IVirtualBoxClient*)lng);
}

JNIEXPORT jboolean JNICALL Java_vbhook_VBHook_init_1glue(JNIEnv* env, jobject obj, jstring vbox_home) {
	setenv("VBOX_APP_HOME", (*env)->GetStringUTFChars(env, vbox_home, NULL), 1);
	return !VBoxCGlueInit();
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_terminate_1glue(JNIEnv* env, jobject obj) {
	VBoxCGlueTerm();
}

JNIEXPORT jstring JNICALL Java_vbhook_VBHook_get_1vb_1version(JNIEnv* env, jobject obj) {
	char str[256];
	unsigned int ver = g_pVBoxFuncs->pfnGetVersion();
	sprintf(str, "%u.%u.%u", ver / 1000000, ver / 1000 % 1000, ver % 1000);
	return (*env)->NewStringUTF(env, str);
}

JNIEXPORT jlong JNICALL Java_vbhook_VBHook_find_1or_1create_1vm(JNIEnv* env, jobject obj, jlong vb, jstring name, jstring os_type){
	IMachine* vm = NULL;
	IVirtualBox* virtualbox = (IVirtualBox*)vb;
	PRUnichar* vbname = (PRUnichar*)(*env)->GetStringChars(env, name, NULL);
	PRUnichar* vbos_type = (PRUnichar*)(*env)->GetStringChars(env, os_type, NULL);

	IVirtualBox_FindMachine(virtualbox, vbname, &vm);
	if(!vm) {
		//create if not found
		BSTR empty; g_pVBoxFuncs->pfnUtf8ToUtf16("", &empty);
		BSTR options; g_pVBoxFuncs->pfnUtf8ToUtf16("forceOverwrite=1", &options);
		virtualbox->lpVtbl->CreateMachine(virtualbox, empty, vbname, 0, NULL, vbos_type, options, &vm);
		g_pVBoxFuncs->pfnUtf16Free(empty); g_pVBoxFuncs->pfnUtf16Free(options);
		IVirtualBox_RegisterMachine(virtualbox, vm);
	}
	return (long)vm;
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_vm_1values(JNIEnv* env, jobject obj, jlong session, jlong vb, jlong vm, jlong vram, jlong mem, jlong cpu, jstring hdd, jstring iso){
	ISession* sessionvb = (ISession*)session;
	IVirtualBox* vbvb = (IVirtualBox*)vb;
	IMachine* vmvb = (IMachine*)vm;
	
	IMachine_LockMachine(vmvb, sessionvb, LockType_Write);
	IMachine* edit = NULL;
	ISession_GetMachine(sessionvb, &edit);
	IMachine_SetMemorySize(edit, mem);
	IMachine_SetCPUCount(edit, cpu);
	
	IGraphicsAdapter* adapter = NULL;
	IMachine_GetGraphicsAdapter(edit, &adapter);
	IGraphicsAdapter_SetAccelerate2DVideoEnabled(adapter, PR_TRUE);
	IGraphicsAdapter_SetAccelerate3DEnabled(adapter, PR_FALSE);
	IGraphicsAdapter_SetVRAMSize(adapter, vram);
	IGraphicsAdapter_Release(adapter); //we're done with it
	
	BSTR storage_controller_name;
	g_pVBoxFuncs->pfnUtf8ToUtf16("IDE Controller", &storage_controller_name);

	IMachine_RemoveStorageController(edit, storage_controller_name);
	IStorageController* storage_controller = NULL;
	IMachine_AddStorageController(edit, storage_controller_name, StorageBus_IDE, &storage_controller);
	IStorageController_Release(storage_controller);
	
	if((*env)->GetStringLength(env, hdd) > 0) {
		IMedium* hdd_medium = NULL;
		IVirtualBox_OpenMedium(vbvb, (PRUnichar*)(*env)->GetStringChars(env, hdd, NULL), DeviceType_HardDisk, AccessMode_ReadWrite, PR_TRUE, &hdd_medium);
		IMachine_AttachDevice(edit, storage_controller_name, 0, 0, DeviceType_HardDisk, hdd_medium);
		IMedium_Release(hdd_medium);
	}
	
	if((*env)->GetStringLength(env, iso) > 0) {
		IMedium* iso_medium = NULL;
		IVirtualBox_OpenMedium(vbvb, (PRUnichar*)(*env)->GetStringChars(env, iso, NULL), DeviceType_DVD, AccessMode_ReadOnly, PR_TRUE, &iso_medium);
		IMachine_AttachDevice(edit, storage_controller_name, 1, 0, DeviceType_DVD, iso_medium);
		IMedium_Release(iso_medium);
	}else{
		IMachine_AttachDevice(edit, storage_controller_name, 1, 0, DeviceType_DVD, NULL);
	}

	IMachine_SaveSettings(edit);
	ISession_UnlockMachine(sessionvb);
	IMachine_Release(edit);
	g_pVBoxFuncs->pfnUtf16Clear(storage_controller_name);
}

JNIEXPORT void JNICALL Java_vbhook_VBHook_start_1vm(JNIEnv* env, jobject obj, jlong session, jlong vm) {
	ISession* sessionvb = (ISession*)session;
	IMachine* vmvb = (IMachine*)vm;
	
	BSTR headless; g_pVBoxFuncs->pfnUtf8ToUtf16("headless", &headless);

	IProgress* progress = NULL;
	vmvb->lpVtbl->LaunchVMProcess(vmvb, sessionvb, headless, 0, NULL, &progress);
	IProgress_WaitForCompletion(progress, -1);
	IProgress_Release(progress);

	g_pVBoxFuncs->pfnUtf16Free(headless);
}
