#include "mbed.h"

#include "rtos.h"
#include "time.h"

#define LRI     1100
#define HRI     1500

DigitalOut pace_signal_led(LED3);
DigitalOut pace_signal(p5);
InterruptIn receive_sense(p6);

int RI = 1500; 
int VRP = 240;
bool hp_enable, hp;   
bool sense_received = 0;
time_t start_time;
Serial pc(USBTX, USBRX);

Thread pacemaker_thread;
    
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
    start_time = time(NULL);
    while(true)
    {
        while(!sense_received && difftime(time(NULL), start_time) < RI)
        ;
        
        if (sense_received)
        {
            pc.printf("sense received\n");
            sense_received = 0;
            RI = HRI;
            hp = true;
            start_time = time(NULL);
        }
        else
        {
            // Send pace signal to heart
            pace_signal_led = 1;
            pace();
            
            pc.printf("paced\n");
            
            RI = LRI;
            hp = false;
            start_time = time(NULL);
            pace_signal_led = 0;
        }
        Thread::wait(VRP);
        hp_enable = hp;
    }
}
    
int main() 
{
    pc.baud(115200);
    
    // Enable the ISR to receive snse signals from heart simulator
    receive_sense.rise(&receive_sense_ISR);
    
    // Start both the threads - pacemaker and observer
    pacemaker_thread.start(pacemaker);
}
