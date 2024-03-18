#ifndef __MQTT_CCLINET_H__
#define __MQTT_CCLINET_H__

#ifdef  __cplusplus
extern "C" {
#endif
int MqttClient_Init(char *myapi_key, char *model, char *imagemodel);
int MqttClient_UnInit();
#ifdef  __cplusplus
}
#endif


#endif /* __MQTT_CCLINET_H__ */
/** @}*/


