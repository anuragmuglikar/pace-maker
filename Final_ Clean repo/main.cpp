#define MQTTCLIENT_QOS2 1

#include "ESP8266Interface.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "TextLCD.h"
#include "mbed.h"
#include "time.h"

#include <string.h>
#include <stdlib.h>
#include <deque>

#define IAP_LOCATION 0x1FFF1FF1

// Serial port for debug
Serial pc(USBTX, USBRX);
#define LRI     1100
#define HRI     1500


// Heart monitor window averaging variables
#define AVG_WINDOW      20 // Can be 20, 40, or 60 seconds
#define AVG_INTERVAL    5 // Can be 5, 10, 15, or 20 seconds


DigitalOut pace_signal_led(LED1);
DigitalOut led_slow_alarm(LED2);
DigitalOut led_fast_alarm(LED3);


DigitalOut pace_signal(p19);
InterruptIn receive_sense(p5);

ESP8266Interface wifi(p28, p27);
// Alarm indicators
bool slow_alarm = 0;
bool fast_alarm = 0;

// Defines for monitor
#define URL             100 //This is in bpm, not ms
#define RATE_ALARM      1
#define PACE_ALARM      2
#define PACE_THRESH     10  // Pretty arbitrary number

int RI = 1500; 
int VRP = 240;
bool hp_enable, hp;   
bool sense_received = 0;
bool sense_flag = 0;
bool pace_sent = 0;
time_t start_time;
Timer timer;

/* pace_times is a subest of beat_times. I need all beats and paces to
 * calculate the average heartrate, but only the number of paces to determine 
 * if the too slow alarm needs to go off. 
 */
deque<time_t> beat_times;
deque<time_t> pace_times;

Thread pacemaker_thread;
Thread monitor_observe;
Thread monitor_calculate;


const char* hostname = "broker.hivemq.com";
int port = 1883;
char* topic = "sectopic";
char* rTopic = "sectopic";
MQTTNetwork network(&wifi);
MQTTNetwork mqttNetwork(network);
MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
MQTT::Message message;
char buf[100];

// This looks at the time difference in each element
// of both the pace and heartbeat queues and removes
// any which are too old, i.e. outside the averaging window time
void prune_lists(time_t new_time) 
{
    
    // Only look at the front of the queues because elements are added
    // already sorted
    while (difftime(new_time, beat_times.front()) > AVG_WINDOW) 
    {
        beat_times.pop_front(); 
    }
    
    while (difftime(new_time, pace_times.front()) > AVG_WINDOW)
    {
        pace_times.pop_front();    
    }
 
    
}

// Flash an LED for an alarm and send an MQTT message 
// to the cloud
void monitor_alarm(int alarm) 
{
    int i;
    Timer alarm_t;
    alarm_t.start();
    if (alarm == RATE_ALARM) 
    {
        // SEND MQTT MESSAGE HERE
        for (i = 0; i < 3; i++) 
        {
            
            led_fast_alarm = 1;
            while(alarm_t.read_ms() < 200*(i+1));
            led_fast_alarm = 0;
        }
        
        pc.printf("Too fast\r\n");
        
    } else if (alarm == PACE_ALARM) 
    {
           
        // SEND MQTT MESSAGE HERE
        for (i = 0; i < 3; i++) 
        {
            led_slow_alarm = 1;
            while(alarm_t.read_ms() < 200*(i + 1)) ;
            led_slow_alarm = 0;
        }
        pc.printf("Too slow\r\n");
    }
} 


// This is a thread for the monitor which simply watches for new beats and
// paces and adds them to the queue(s) when they occur
void monitor_obs() 
{
    
    time_t new_time;
    
    // Monitor just runs forever
    while (1) {
    
        // Wait until you see a sense or pace
        while (!sense_flag && !pace_sent) ;
    
        // Indicate if you saw a sense or a pace, then set the indicator back to
        // 0 so that the current signal is not counted more than once
        if (sense_flag) {
            beat_times.push_back(time(NULL));
            sense_flag = 0;
        }
        if (pace_sent) {
            new_time = time(NULL);
            beat_times.push_back(new_time);
            pace_times.push_back(new_time);
            pace_sent = 0;
        }
    
    }
}

void  monitor_calc() 
{
    time_t current_time;
    time_t wait_start = time(NULL);
    
    int heart_rate;
    int num_paces;
    //printf("calc...\n");
    // I want to check the heart rate in bpm for the alarm,
    // so if my window time is smaller than 60 seconds I want to have
    // this window factor to scale it
    int window_factor = 60 / AVG_WINDOW;
    
    // The monitor needs to wait for at least one full AVG_WINDOW
    // before it starts to monitor the average heartrate or else 
    // it is going to give spurious low heartrate alarms
    while (difftime(time(NULL),wait_start) < AVG_WINDOW);
    
    
    message.qos = MQTT::QOS0;
    while(1) {
          //pc.printf("In monitor calc loop\r\n");
         // Get the current time and see if you need to prune any elements from
         // your lists
         current_time = time(NULL);
         prune_lists(current_time);
            
         
         // Find average heart rate and number of paces if it is time to,
         // then set any necessary alarms
            heart_rate = beat_times.size() * window_factor;
            num_paces = pace_times.size();
            pc.printf("H.R = %d\r\n", heart_rate);
            pc.printf("N.P = %d\r\n", num_paces);
            sprintf(buf, "%d", heart_rate);
            message.retained = true;
            message.dup = false;
            message.payload = (void*)buf;
            message.payloadlen = strlen(buf);
            int rc = client.publish(topic, message);
            
            client.yield(50);
            if(rc == 0){
                printf("Success PUB%d\n", rc);
            }else{
                printf("Failed PUB%d\n", rc);
            }
            //printf("About to alarm\r\n");
            if (heart_rate > URL) {
                monitor_alarm(RATE_ALARM);    
            } else if (num_paces > PACE_THRESH) {
                monitor_alarm(PACE_ALARM);    
            }
         //printf("Alarm done\r\n");
         wait_start = time(NULL);
         // Wait until you need to calculate averages again
         while (difftime(time(NULL),wait_start) < AVG_INTERVAL); 
    }
}
    
// ISR to receive sense signals
void receive_sense_ISR()
{
    sense_received = 1;
}

void pace()
{
    pace_signal = 1;
    pace_signal = 0;

}

void pacemaker()
{
    //start_time = timer.read_ms();
    timer.reset();
    timer.start();
    while(true)
    {

        //while(!sense_received && ((timer.read_ms() - start_time) < RI))
        while(!sense_received && ((timer.read_ms()) < RI))
       ;

        if (sense_received)
        {
            //pc.printf("sense received\n");
            sense_received = 0;
            
            // Let monitor know there was a heartbeaat
            sense_flag = 1;
            
            RI = HRI;
            hp = true;
            //start_time = timer.read_ms();
            timer.reset();
        }
        else
        {
            // Send pace signal to heart
            pace_signal_led = 1;
            pace();
            
            // Indicate oace was sent for monitor
            pace_sent = 1;
            
            //pc.printf("paced - %d\n", (timer.read_ms() - start_time));
            
            RI = LRI;
            hp = false;
            //start_time = timer.read_ms();
            timer.reset();
            pace_signal_led = 0;
        }
        // Wait for VRP
        //while((timer.read_ms() - start_time) < VRP);
        while((timer.read_ms()) < VRP);
        hp_enable = hp;
    }
}
    
int main() 
{
    pc.baud(9600);
    // Enable the ISR to receive snse signals from heart simulator
    wifi.set_credentials(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD);
    wifi.connect();
    int rc = network.connect(hostname, port);
    pc.printf("Connecting to %s:%d\r\n", hostname, port);
    rc = mqttNetwork.connect(hostname, port);
    data.MQTTVersion = 3;
    data.clientID.cstring = "26013f37-13006018ae2a8ca752c368e3f5001e81";
    data.username.cstring = "mbed";
    data.password.cstring = "homework";
    if ((rc = client.connect(data)) != 0)
        pc.printf("rc from MQTT connect is %d\r\n", rc);
    receive_sense.rise(&receive_sense_ISR);
    
    // Start both the threads - pacemaker and observer
    monitor_observe.start(monitor_obs);
    monitor_calculate.start(monitor_calc);
    pacemaker_thread.start(pacemaker);
}
