/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the aws_iot_config.h file and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "sdkTest/sub"
 *
 * If all the certs are correct, you should see the messages received by the application in a loop.
 *
 * The application takes in the certificate path, host name , port and the number of times the publish should happen.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <signal.h>
#include <memory.h>
#include <sys/time.h>
#include <limits.h>

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"
#include "MQTTClient.h"
#include "AC_DeviceService.h"

unsigned char g_u8MsgBuildBuffer[600];

char HostAddress[255] = AWS_IOT_MQTT_HOST;      /* 以前是域名，现在当ip串用 */
char ACHostAddr[255] = AWS_IOT_MQTT_HOST;

static AC_AccessPoint g_struNode;
static AC_ConnectInfo conn;

static char AC_Version[500] = {0};

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "../../certs";


//static unsigned int g_ConnectCount = 0;

int MQTTcallbackHandler(MQTTCallbackParams params) {

    unsigned short len = 0;
    char pubTopic[256] = {0};
    char deviceId[]= AC_DEVICEID;
	char domainName[] = AC_DOMAIN_NAME;
	char subdomainName[] = AC_SUBDOMAIN_NAME;
    AC_MessageHead *pstruMsg;

	INFO("Subscribe callback");
	INFO("%.*s\t%.*s",
			(int)params.TopicNameLen, params.pTopicName,
			(int)params.MessageParams.PayloadLen, (char*)params.MessageParams.pPayload);
    if (0 == params.MessageParams.PayloadLen)
    {
        /* maybe need to be modified */
        return GENERIC_ERROR;
    }
    pstruMsg = (AC_MessageHead *)params.MessageParams.pPayload;

    sprintf(pubTopic, "device/%s/%s/%s/out", domainName, subdomainName, deviceId);

	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
	char cPayload[100];

    ACEventBuildMsg(102, pstruMsg->MsgId, cPayload, &len, NULL, 0);
    
	Msg.pPayload = (void *) cPayload;
    Msg.PayloadLen = len;

	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = pubTopic;

    Params.MessageParams = Msg;

    aws_iot_mqtt_publish(&Params);
    
	return 0;
}

#define AC_MODULE_NAME            "Test-xxx"
#define AC_MODULE_VERSION         "MOD2016-05-13"
#define AC_DEV_VERSION            "MCU2016-05-13"
#define AC_HARDWARE_VERSION       "Hardware2016-05-13"

int MQTTReportVersion(void)
{
    AC_DevInfo *version;
    unsigned short len = 0;
    unsigned short realLen = 0;
    char pubTopic[256] = {0};
    char msgTmp[300] = {0};
    
    char deviceId[]= AC_DEVICEID;
	char domainName[] = AC_DOMAIN_NAME;
	char subdomainName[] = AC_SUBDOMAIN_NAME;

    printf("MQTTReportVersion\n");

	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
    
    version = (AC_DevInfo *)msgTmp;
    
    version->modTypeLen = strlen(AC_MODULE_NAME);
    version->modVersionLen = strlen(AC_MODULE_VERSION);
    version->devVersionLen = strlen(AC_DEV_VERSION);
    version->hardwareVersionLen = strlen(AC_HARDWARE_VERSION);

    memcpy(version->pInfo, AC_MODULE_NAME, version->modTypeLen);
    memcpy(&version->pInfo[version->modTypeLen], AC_MODULE_VERSION, version->modVersionLen);
    memcpy(&version->pInfo[version->modTypeLen + version->modVersionLen], AC_DEV_VERSION, version->devVersionLen);
    memcpy(&version->pInfo[version->modTypeLen + version->modVersionLen + version->devVersionLen], 
           AC_HARDWARE_VERSION, 
           version->hardwareVersionLen);

    len = sizeof(AC_DevInfo) + version->modTypeLen + version->modVersionLen + version->devVersionLen + version->hardwareVersionLen;

    printf("len is %d\n", len);
    printf("str is %s\n", version->pInfo);

	sprintf(pubTopic, "device/%s/%s/%s/out", domainName, subdomainName, deviceId);
	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = pubTopic;

    Msg.pPayload = (void *)AC_Version;
    
    ACEventBuildMsg(39, 0, msgTmp, &realLen, AC_Version, len);

	Msg.PayloadLen = realLen;
	Params.MessageParams = Msg;
	aws_iot_mqtt_publish(&Params);
}

void disconnectCallbackHandler(void) 
{
	WARN("MQTT Disconnect");
	IoT_Error_t rc = NONE_ERROR;
	if(aws_iot_is_autoreconnect_enabled())
    {
		INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	}
    else
    {
		WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect();
		if(RECONNECT_SUCCESSFUL == rc)
        {
			WARN("Manual Reconnect Successful");
		}
        else
        {
			WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

IoT_Error_t MQTTConnectCloud(void)
{
    IoT_Error_t rc = NONE_ERROR;
    MQTTConnectParams connectParams = MQTTConnectParamsDefault;
	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
	char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
	char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

    unsigned char Password[100] = {0};
    char userName[100] = {0};
	char clientId[256] = {0};
    char deviceId[]= AC_DEVICEID;
	char domainName[] = AC_DOMAIN_NAME;
	char subdomainName[] = AC_SUBDOMAIN_NAME;
    time_t timestamp = time(NULL);
    int timeout = 3600;
    char chaccessKey[17] = {0};

    conn.connectTime = 0;
	getcwd(CurrentWD, sizeof(CurrentWD));
	sprintf(rootCA, "%s/%s/%s", CurrentWD, certDirectory, cafileName);
	sprintf(clientCRT, "%s/%s/%s", CurrentWD, certDirectory, clientCRTName);
	sprintf(clientKey, "%s/%s/%s", CurrentWD, certDirectory, clientKeyName);

	DEBUG("rootCA %s", rootCA);
	DEBUG("clientCRT %s", clientCRT);
	DEBUG("clientKey %s", clientKey);

    AC_Rand(chaccessKey);

    sprintf(userName, "%d#%s#%d", timeout, chaccessKey, timestamp);
	sprintf(clientId, "d:%s:%s:%s", domainName, subdomainName, deviceId);
    AC_GetPass(deviceId, timestamp, timeout, chaccessKey, Password);

	connectParams.KeepAliveInterval_sec = 10;
	connectParams.isCleansession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = clientId;
	connectParams.pUserName = userName;
	connectParams.pPassword = Password;
	connectParams.pHostURL = HostAddress;
	connectParams.port = port;
	connectParams.isWillMsgPresent = false;
	connectParams.pRootCALocation = rootCA;
	connectParams.pDeviceCertLocation = clientCRT;
	connectParams.pDevicePrivateKeyLocation = clientKey;
	connectParams.mqttCommandTimeout_ms = 2000;
	connectParams.tlsHandshakeTimeout_ms = 5000;
	connectParams.isSSLHostnameVerify = true; // ensure this is set to true for production
	connectParams.disconnectHandler = disconnectCallbackHandler;      

    do
    {
        INFO("Connecting... %s", HostAddress);
    	rc = aws_iot_mqtt_connect(&connectParams);
    	if (NONE_ERROR != rc) 
        {
    		ERROR("Error(%d) connecting to %s:%d", rc, connectParams.pHostURL, connectParams.port);
            conn.connectTime++;
    	}
        else
        {
            conn.connectTime = 0;
            break;
        }
    }while(conn.connectTime < AC_CONNECT_MAX_TIME);

    return rc;
}

void getAccessPointAction(void *pnode)
{
    if (NULL == pnode)
    {
        return;
    }
    unsigned int i;
    char delims[] = ":";
    char *result = NULL;
    
    memset(&g_struNode, 0, sizeof(g_struNode));
    memcpy(&g_struNode, pnode, sizeof(AC_AccessPoint));
    if (0 == g_struNode.totalNum)
    {
        return;
    }
    for (i = 0; i < g_struNode.totalNum; i++)
    {
        if (IPV4_TYPE == g_struNode.ap[i].type)
        {
            result = strtok(g_struNode.ap[i].ip, delims);
            if (NULL != result)
            {
                strcpy(HostAddress, result);
                conn.currentNumber = i;
                return;
            }
        }
    }
    
}

int getNextAccessPoint(void)
{
    unsigned int i = 0;
    char delims[] = ":";
    char *result = NULL;
    int flag = 0;
    if (conn.currentNumber == g_struNode.totalNum - 1)
    {
        return -1;
    }

    for (i = conn.currentNumber + 1; i <g_struNode.totalNum; i++)
    {
        if (IPV4_TYPE == g_struNode.ap[i].type)
        {
            result = strtok(g_struNode.ap[i].ip, delims);
            if (NULL != result)
            {
                strcpy(HostAddress, result);
                conn.currentNumber = i;
                flag = 1;
                break;
            }
        }        
    }
    if (1 == flag)
    {
        return 0;
    }
    else
    {
        return -2;
    }
}


/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */


/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

void parseInputArgsForConnectParams(int argc, char** argv) {
	int opt;

	while (-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) {
		switch (opt) {
		case 'h':
			strcpy(HostAddress, optarg);
			DEBUG("Host %s", optarg);
			break;
		case 'p':
			port = atoi(optarg);
			DEBUG("arg %s", optarg);
			break;
		case 'c':
			strcpy(certDirectory, optarg);
			DEBUG("cert root directory %s", optarg);
			break;
		case 'x':
			publishCount = atoi(optarg);
			DEBUG("publish %s times\n", optarg);
			break;
		case '?':
			if (optopt == 'c') {
				ERROR("Option -%c requires an argument.", optopt);
			} else if (isprint(optopt)) {
				WARN("Unknown option `-%c'.", optopt);
			} else {
				WARN("Unknown option character `\\x%x'.", optopt);
			}
			break;
		default:
			ERROR("Error in command line argument parsing");
			break;
		}
	}

}
int main(int argc, char** argv) {
	IoT_Error_t rc = NONE_ERROR;
	int32_t i = 0;
	int j = 0;
	int ret = 0;
    time_t timestamp = time(NULL);
    int timeout = 3600;
	bool infinitePublishFlag = true;
    unsigned short len = 0;
    char chaccessKey[17] = {0};
    unsigned char Password[100] = {0};
    char userName[100] = {0};
	char clientId[256] = {0};
	char subTopic[256] = {0};
	char pubTopic[256] = {0};
    char deviceId[]= AC_DEVICEID;
	char domainName[] = AC_DOMAIN_NAME;
	char subdomainName[] = AC_SUBDOMAIN_NAME;

	parseInputArgsForConnectParams(argc, argv);

	INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);
    
    iot_tls_set_host(ACHostAddr);
    AC_RegisterAccessCallBack(getAccessPointAction);

    AC_DeviceServiceInit(domainName, subdomainName, deviceId, "0-0-0");
    AC_GetAccessPoints();

    while (0 == strcmp(HostAddress, "dev.ablecloud.cn"))
    {
		INFO("-->equal");
		sleep(1);
    }

    printf("HostAddress is %s\n", HostAddress);
	
    do
    {
        ret = (int)MQTTConnectCloud();
        if (NONE_ERROR != ret)
        {
            ret = getNextAccessPoint();

            if (NONE_ERROR == ret)
            {
                continue;
            }
            else
            {
                /* delete file */
                AC_DelAccessPointFile();
                /* get access point again */
                ret = AC_GetAccessPoints();
            }
        }
        else 
        {
            break;
        }
            
    }while(NONE_ERROR == ret);

    /* report version */
    while (1)
    {
        MQTTReportVersion();
        sleep(1);
    }
    
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(true);
	if (NONE_ERROR != rc) {
		ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}
	sprintf(subTopic, "device/%s/%s/%s/in", domainName, subdomainName, deviceId);
	MQTTSubscribeParams subParams = MQTTSubscribeParamsDefault;
	subParams.mHandler = MQTTcallbackHandler;
	subParams.pTopic = subTopic;
	subParams.qos = QOS_0;

	if (NONE_ERROR == rc) {
		INFO("Subscribing...");
		rc = aws_iot_mqtt_subscribe(&subParams);
		if (NONE_ERROR != rc) {
			ERROR("Error subscribing,error is %d", rc);
		}
	}

	MQTTMessageParams Msg = MQTTMessageParamsDefault;
	Msg.qos = QOS_0;
	char cPayload[100];
	sprintf(cPayload, "%s : %d ", "hello from SDK", i);
	Msg.pPayload = (void *) cPayload;

	sprintf(pubTopic, "device/%s/%s/%s/out", domainName, subdomainName, deviceId);
	MQTTPublishParams Params = MQTTPublishParamsDefault;
	Params.pTopic = pubTopic;

	if (publishCount != 0) {
		infinitePublishFlag = false;
	}

	while ((NETWORK_ATTEMPTING_RECONNECT == rc || RECONNECT_SUCCESSFUL == rc || NONE_ERROR == rc)
			&& (publishCount > 0 || infinitePublishFlag)) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc){
			INFO("-->sleep");
			sleep(1);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}
		INFO("-->sleep");
		sleep(1);
        ACEventBuildMsg(210, 0, cPayload, &len, NULL, 0);
		//sprintf(cPayload, "%s : %d ", "hello from SDK", i++);
		Msg.PayloadLen = len;
		Params.MessageParams = Msg;
		rc = aws_iot_mqtt_publish(&Params);
		if (publishCount > 0) {
			publishCount--;
		}
	}

	if (NONE_ERROR != rc) {
		ERROR("An error occurred in the loop.\n");
	} else {
		INFO("Publish done\n");
	}

	return rc;
}

