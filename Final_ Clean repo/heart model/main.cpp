#include "mbed.h"

DigitalOut myled(LED1);
DigitalOut myled2(LED2);
#define LRI 1100
#define HRI 1500
#define VRP 240
#define PULSE_WIDTH 100
#define MINWAIT 100
#define MAXWAIT 1900
InterruptIn ventricleInt(p5);
DigitalOut beatSignal(p19);

Timer timeToBeat;
Timer timeVRP; 
Serial pc(USBTX, USBRX);

int random_beat = 1300;
bool receivedPace = false;
volatile enum {READY, IDLE}heartState;


void waitForVRP(){
      
    //pc.printf("vrp\n");
    timeVRP.stop();
    timeVRP.reset();
    timeVRP.start();
    while(timeVRP.read_ms() >= VRP);
    heartState = READY; 
}

void beat() {
    if(!receivedPace){
        beatSignal = 1;
        beatSignal = 0;    
        pc.printf("B\n");
        heartState = IDLE;
        //timeToBeat.detach();
    }else{
       
        receivedPace = false;   
        heartState = IDLE;
    }
    //waitForVRP();
    heartState = READY;
    
}

void paced() {
    if(heartState != IDLE){
    receivedPace = true;
     pc.printf("RP\n");
    heartState = IDLE;
    timeVRP.reset();
    }
}


int main() {
    ventricleInt.rise(&paced);
    while(1){
        //myled = 1;
        heartState = READY;
        random_beat = (rand() % (MAXWAIT - MINWAIT)) + MINWAIT;
        pc.printf("rNDM beat: %d\n", random_beat);
        timeToBeat.stop();
        timeToBeat.reset();
        timeToBeat.start();
        //pc.printf("W.R.B\n");
        while(!(timeToBeat.read_ms() >= random_beat  || receivedPace) && heartState != IDLE);
        beat();
    }

}
