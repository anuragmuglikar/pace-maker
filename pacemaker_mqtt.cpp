#include "mbed.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "wifiGETWrapper.h"

MQTT::Message message;

void messageArrived(MQTT::MessageData& md) {

    MQTT::Message &message = md.message;
    char* number;
    printf("Message arrived: qos %d , packet id: %d\n", message.qos, message.id);
    printf("Payload %.*s\n", message.payloadlen, (char*)message.payload);
    salted_id = strtok((char *)message.payload, "-");
    number = strtok(NULL, "\0");
    
}

void send_mqtt_message() {
    
        //Make changes only over here to msgbuf. Accept global variables and sprintf to msgbuf.
        sprintf(msgbuf, "Avg heartbeat: %d ; Alarm: %s", avg_heartbeat, Alarm);
        message.payload = (void*)msgbuf;
        message.payloadlen = strlen(msgbuf)+1;  
        ret = client.publish(topic, message);  
        
        if(ret != 0) {
            printf("Client publish failure %d\n", ret);    
        } else {
            printf("Client publish successful %d\n", ret);    
        }
        client.yield(1000);
     
        wait(4);
        printf("Timer observed value is %f\n", timer.read());   
    
}

int initConnection(const char * SSID, const char * password)
{
    printf("WiFi example\r\n\r\n");
    printf("\r\nConnecting...\r\n");
    
    int ret = wifi.connect(SSID, password);
    if (ret != 0) {
        printf("\r\nConnection error\r\n");
        return -1;
    }

    printf("Success\r\n\r\n");
    printf("MAC: %s\r\n", wifi.get_mac_address());
    printf("IP: %s\r\n", wifi.get_ip_address());
    printf("Netmask: %s\r\n", wifi.get_netmask());
    printf("Gateway: %s\r\n", wifi.get_gateway());
    printf("RSSI: %d\r\n\r\n", wifi.get_rssi());

}

int main(int argc, char* argv[]) {
    int ret;
    
    //Define host name/ identification
    //const char *hostname = "192.168.4.1";
    const char *hostname = "35.196.225.7";
    //const char *hostname = "192.168.43.138";
    char topic[100];
    char subscription_topic[100];
    char id[8];
    char deviceID[33];
    char uuid[45];
    int portnum = 1883;

    IAP iap;
    
    unsigned int iap_out;
    iap_out = iap.read_ID();
    
    int *iap_serial = (int *)malloc(sizeof(int) * 4);
    iap_serial = iap.read_serial();
    
 
   #ifdef DEBUG_PRINT
    printf("IAP output is %08x\n",iap_out);
    printf("IAP output is %08x\n", *(iap_serial));
    printf("IAP output is %08x\n", *(iap_serial+ 1));
    printf("IAP output is %08x\n", *(iap_serial+ 2));
    printf("IAP output is %08x\n", *(iap_serial+ 3));
    printf("size of int is %d\n", sizeof(int));
    #endif
    
    sprintf(id, "%08x", iap_out);
    sprintf(deviceID, "%08x%08x%08x%08x", *(iap_serial),*(iap_serial + 1),*(iap_serial+ 2),*(iap_serial+ 3));
    sprintf(uuid, "%8s-%32s", id, deviceID);
    printf("uuid: %s\n", uuid);
    sprintf(topic, "cis541/hw-mqtt/%s/data", uuid);
    printf("topic: %s\n", topic);
    sprintf(subscription_topic, "cis541/hw-mqtt/%s/echo", uuid);
    printf("subscribe topic: %s\n", subscription_topic);
    
 #ifdef MQTT   
    MQTTNetwork mqttnw(&wifi);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttnw);
#endif

    wifi.set_credentials(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    ret = wifi.connect();
    if(ret < 0) {
        printf("Error in connecting to AP \n");    
    }
    
        printf("IP address is %s \n", wifi.get_ip_address());    
        
 #ifdef MQTT   
    //Establish connection
    ret = mqttnw.connect(hostname, portnum);
    if(ret < 0) {
        printf("Error in connecting to mqttnw ret: %d\n", ret);    
    }
    
     MQTTPacket_connectData data = MQTTPacket_connectData_initializer;  
 //    connackData connData;
     data.clientID.cstring = uuid;
     data.username.cstring = "mbed";
     data.password.cstring = "homework";
    
    ret = client.connect(data);
    if(ret < 0) {
        printf("Error in connecting to MQTT server at %s, ret i %d\n", hostname, ret);    
    }
  
    ret = client.subscribe(subscription_topic, MQTT::QOS0, messageArrived);
    if(ret < 0) {
        printf("Client did not subscribe to topic %s, ret is %d\n", subscription_topic, ret);    
    }
    printf("client subscribe ret is %d\n", ret);
    
    int count = 0;
 
    message.qos = MQTT::QOS1;
    message.retained = false;
    message.dup = false;    

    send_mqtt_message();    
    client.unsubscribe(subscription_topic);
    client.disconnect();
#endif
  
#ifdef HTTP   
    GET(&wifi, hostname, "", portnum);
    printf("End of GET\n");
#endif 

    return 0;
}


