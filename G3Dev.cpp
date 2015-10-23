/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>;
#include <sys/types.h>;
#include <string.h>
#include <errno.h>

#define LOG_TAG "Vold"

#include <cutils/log.h>
#include <sysutils/NetlinkEvent.h>

#include "G3Dev.h"
#include "MiscManager.h"
#include "ResponseCode.h"
#include <unistd.h>

#define USBPATH  "/sys/class/usb_device/usbdev1.1/device/1-1"
#if PLATFORM_SDK_VERSION >= 16
#define LOGV(fmt,args...) ALOGV(fmt,##args)
#define LOGD(fmt,args...) ALOGD(fmt,##args)
#define LOGI(fmt,args...) ALOGI(fmt,##args)
#define LOGW(fmt,args...) ALOGW(fmt,##args)
#define LOGE(fmt,args...) ALOGE(fmt,##args)
#endif

static char modeswitch_cmd[256] = "";


G3Dev::G3Dev(MiscManager *mm):Misc(mm)
{
	
	//直接对netLinkEvent事件成员赋值
	//NetlinkEvent *evt = new NetlinkEvent();
	//evt->mAction=NetlinkEvent::NlActionAdd;
	//evt->mSubsystem="usb_storage";
	//this->handleUsb(evt);
}

G3Dev::~G3Dev() {
}

//***********************************
//* add by bonovo zbiao for UsbG3dev
//***********************************
int G3Dev::handleUsbG3dev(){
	int vid, pid, res;
	char* usbpath = "sys/class/usb_device/usbdev2.2";

	if(access(usbpath, F_OK) == 0){
		//SLOGD("the path sys/class/usb_device/usbdev2.2 is exist!\n");
		if((res = get_usb_id(usbpath, &vid, &pid)) != 0){
			SLOGD("=== USBModeSwitch get_tty_id error, res=%d\n", res);
			return -2;
		}
		char configure_file[2048];
		sprintf(configure_file, "/etc/usb_modeswitch.d/%04x_%04x", vid, pid);
		if(access(configure_file, 0) == 0)
		{
			char modeswitch_cmd[256] = "";
			// sprintf(modeswitch_cmd, "usb_modeswitch -W -I -c %s &", configure_file);
			sprintf(modeswitch_cmd, "/system/bin/usb_modeswitch.sh %s &", configure_file);
			SLOGD("=== USBModeSwitch: %s", modeswitch_cmd);
			system(modeswitch_cmd);
		}else{
			SLOGD("access %s error(%s)\n", configure_file, strerror(errno));
			return -3;
		}
	}
	else{
		SLOGD("access sys/class/usb_device/usbdev2.2 error(%s)\n", strerror(errno));
		return -1;
	}
	return 0;
}

int G3Dev::get_usb_id(char* usb_path, int *vid, int* pid)
{
	char linkto[1024]="";

	char pidpath[1024];
	FILE* fp = NULL;
	char buf[5] = "";

	sprintf(pidpath, "%s/device/idVendor", usb_path);
	SLOGD("=== USBModeSwitch Vendor ID Path: %s", pidpath);
	fp = fopen(pidpath, "r");
	if(fp == NULL)
		return -2;
	if(fread(buf, 1, 4, fp) != 4)
	{
		fclose(fp);
		return -2;
	}
	fclose(fp);
	*vid = atox(buf, 16);
	//char vidpath[1024]="/sys/class/usb_device/usbdev1.1/device/1-1";

	char vidpath[1024];
	sprintf(vidpath, "%s/device/idProduct", usb_path);
	SLOGD("=== USBModeSwitch Product ID path: %s", vidpath);
	fp = fopen(vidpath, "r");
	if(fp == NULL)
		return -3;
	if(fread(buf, 1, 4, fp) != 4)
	{
		fclose(fp);
		return -3;
	}
	fclose(fp);
	*pid =atox(buf, 16);

    return 0;
}
//***********************************

int G3Dev::handleUsbEvent(NetlinkEvent *evt) {
 const char *devtype = evt->findParam("DEVTYPE");
    if( devtype!=NULL &&strcmp(devtype, "usb_device") )
        return 0;
	pid_t status ;

	int action = evt->getAction();
	if( action == NetlinkEvent::NlActionAdd )
	{
        const char *product = evt->findParam("PRODUCT");
        if(product!=NULL && product[0] != 0 && devtype[0] != 0 )
        {
            // 获取VID/PID
            int vid = 0;
            int pid = 0;
            char * next = (char*)product;
            vid = strtol(product, &next, 16);
		
			//char pre[]="sVe.GT";
            ++next;
            pid = strtol(next, NULL, 16);

            SLOGD("=== Current USBModeSwitch device: %04X/%04X ===", vid, pid);

            char configure_file[2048];
			
			
            sprintf(configure_file, "/etc/usb_modeswitch.d/%04x_%04x", vid, pid);
            if( access(configure_file, 0) == 0 )
            {
               // sprintf(modeswitch_cmd, "usb_modeswitch -W -I -c %s &", configure_file);
            	sprintf(modeswitch_cmd, "/system/bin/usb_modeswitch.sh %s &", configure_file);
                SLOGD("=== USBModeSwitch: %s", modeswitch_cmd);
                system(modeswitch_cmd);
            }
        }
	}
	
    return 0;
}
int G3Dev::handleScsiEvent(NetlinkEvent *evt) {
/*
    有一定概率在usb_device 这个event中执行的usb_modeswitch没有成功，作为补足
    手段，在SCSI这个event中再次执行usb_modeswitch
 */
    if(evt->getAction()==NetlinkEvent::NlActionAdd && modeswitch_cmd[0] != 0)
    {
        SLOGD("=== SCSI USBModeSwitch: %s", modeswitch_cmd);
        system(modeswitch_cmd);
        modeswitch_cmd[0] = 0;
    }

    return 0;
}
int G3Dev::handleUsb(){

 
 char configure_file[2048];
 int pid,vid;
 this->get_tty_id(USBPATH,&vid,& pid);
 sprintf(configure_file, "/etc/usb_modeswitch.d/%04x_%04x", vid,pid);
 if( access(configure_file, 0) == 0 )
 {
	sprintf(modeswitch_cmd, "/system/bin/usb_modeswitch.sh %s &", configure_file);
	SLOGD("=== USBModeSwitch Switch: %s", modeswitch_cmd);
	 system(modeswitch_cmd);
 
  }
    //***************************
    //* add by bonovo zbiao
    //***************************
    handleUsbG3dev();
    //***************************
 return 0;
 }
int G3Dev:: get_tty_id( char* tty_path, int *vid, int* pid)
{
   
	   char linkto[1024]="";
	   SLOGD("=== USBModeSwitch: Began finding the device path");
	   SLOGD("=== USBModeSwitch: The device path is: %s", tty_path);	  
	   		   
	   char pidpath[1024]="/sys/class/usb_device/usbdev1.1/device/1-1";
	   
		   FILE* fp = NULL;
		   char buf[5] = "";
		   strcat(pidpath, "/idVendor");
	   	   SLOGD("=== USBModeSwitch Vendor Path: %s", pidpath);
		   fp = fopen(pidpath, "r");
		   if(fp == NULL)
			   return -2;
		   if(fread(buf, 1, 4, fp) != 4)
		   {
			   fclose(fp);
			   return -2;
		   }
		   fclose(fp);
		   *vid = atox(buf, 16);
	   char vidpath[1024]="/sys/class/usb_device/usbdev1.1/device/1-1";
		
		   strcat(vidpath, "/idProduct");
	           SLOGD("=== USBModeSwitch Product Path: %s", vidpath);
		   fp = fopen(vidpath, "r");
		   if(fp == NULL)
			   return -3;
		   if(fread(buf, 1, 4, fp) != 4)
		   {
			   fclose(fp);
			   return -3;
		   }
		   fclose(fp);
		   *pid =atox(buf, 16);

    return 0;
}
#define EATCHAR(x, c) for (; *(x) == (c); (x)++) ; 
/* -1:error. */
int G3Dev:: atox( const char * line, int f_base )
{
    int base = 10;
    char max = '9';
    int v = 0;

    EATCHAR(line, ' ');
    if(*line == 0) return 0;
    
    if( line[1] == 'x' || line[1] == 'X' ){
        base = 16;
        max = 'f';      /* F*/
        line += 2;
    }
    else if(f_base==16)
    {
        base = 16;
        max = 'f';      /* F*/
    }
    
    if( base == 10 ) {
            while( *line >= '0' && *line <= max ) {
                        v *= base ;
                        v += *line-'0';
                        line++;
            }
    } else {
            while( *line >= '0' && *line <= max ) {
                        v *= base ;
                        if( *line >= 'a' )
                                v += *line-'a'+10;
                        else if ( *line >= 'A' )
                                v += *line-'A'+10;
                        else
                                v += *line-'0';
                        line++;
                }
    }
    return v;
}

