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
#define AVG_WINDOW      60 // Can be 20, 40, or 60 seconds
#define AVG_INTERVAL    10 // Can be 5, 10, 15, or 20 seconds


DigitalOut my_led(LED1);
DigitalOut led_slow_alarm(LED2);
DigitalOut led_fast_alarm(LED3);
DigitalOut send_pace(p5);
InterruptIn receive_sense(p6);

// Alarm indicators
bool slow_alarm = 0;
bool fast_alarm = 0;

// Defines for monitor
#define URL             100  //This is in bpm, not ms
#define RATE_ALARM      1
#define PACE_ALARM      2
#define PACE_THRESH     10  // Pretty arbitrary number

int RI = 1500; 
int VRP = 240;
bool hp_enable, hp;   
bool sense_received = 0;
bool pace_sent = 0;
time_t start_time;

/* pace_times is a subest of beat_times. I need all beats and paces to
 * calculate the average heartrate, but only the number of paces to determine 
 * if the too slow alarm needs to go off. 
 */
deque<time_t> beat_times;
deque<time_t> pace_times;

Thread pacemaker_thread;
Thread monitor_observe;
Thread monitor_calculate;


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
    
    if (alarm == RATE_ALARM) 
    {
        // SEND MQTT MESSAGE HERE
        for (i = 0; i < 3; i++) 
        {
            led_fast_alarm = 1;
            wait_ms(200);
            led_fast_alarm = 0;
        }
        
    } else if (alarm == PACE_ALARM) 
    {
           
        // SEND MQTT MESSAGE HERE
        for (i = 0; i < 3; i++) 
        {
            led_slow_alarm = 1;
            wait_ms(200);
            led_slow_alarm = 0;
        }
    }
} 


// This is a thread for the monitor which simply watches for new beats and
// paces and adds them to the queue(s) when they occur
void monitor_obs() 
{
    
    time_t new_time;
    
    // Monitor just runs forever
    while (1) {
    
        // Indicate if you saw a sense or a pace, then set the indicator back to
        // 0 so that the current signal is not counted more than once
        if (sense_received) {
            beat_times.push_back(time(NULL));
            sense_received = 0;
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
    
    int heart_rate;
    int num_paces;
    
    // I want to check the heart rate in bpm for the alarm,
    // so if my window time is smaller than 60 seconds I want to have
    // this window factor to scale it
    int window_factor = 60 / AVG_WINDOW;
    
    // The monitor needs to wait for at least one full AVG_WINDOW
    // before it starts to monitor the average heartrate or else 
    // it is going to give spurious low heartrate alarms
    wait_ms(AVG_WINDOW*1000);
    
    while(1) {
         
         // Get the current time and see if you need to prune any elements from
         // your lists
         current_time = time(NULL);
         prune_lists(current_time);
         
         // Find average heart rate and number of paces if it is time to,
         // then set any necessary alarms
            heart_rate = beat_times.size() / AVG_WINDOW * window_factor;
            num_paces = pace_times.size();
            if (heart_rate > URL) {
                monitor_alarm(RATE_ALARM);    
            } else if (num_paces > PACE_THRESH) {
                monitor_alarm(PACE_ALARM);    
            }
         
         // Wait until you need to calculate averages again
         wait_ms(AVG_INTERVAL * 1000);  
    }
}
    
// ISR to receive sense signals
void receive_sense_ISR()
{
    sense_received = 1;
}

void pacemaker()
{
    start_time = time(NULL);
    while(true)
    {
        while(!sense_received && difftime(time(NULL), start_time) < RI)
        ;
        
        if (sense_received)
        {
            sense_received = 0;
            RI = HRI;
            hp = true;
            start_time = time(NULL);
        }
        else
        {
            // TODO: Send signal to heart
            RI = LRI;
            hp = false;
            // TODO: Blink LED
            start_time = time(NULL);
        }
        Thread::wait(VRP);
        hp_enable = hp;
    }
}
    
int main() 
{
    // Enable the ISR to receive snse signals from heart simulator
    receive_sense.rise(&receive_sense_ISR);
    
    // Start both the threads - pacemaker and observer
    monitor_observe.start(monitor_obs);
    monitor_calculate.start(monitor_calc);
    pacemaker_thread.start(pacemaker);
}