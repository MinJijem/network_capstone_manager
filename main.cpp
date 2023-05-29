#include "mbed.h"
#include "string.h"
#include "L2_FSMmain.h"
#include "L3_FSMmain.h"

//serial port interface
Serial pc(USBTX, USBRX);

//GLOBAL variables (DO NOT TOUCH!) ------------------------------------------

//source/destination ID
uint8_t myId=0;
//uint8_t input_destId=0;

//FSM operation implementation ------------------------------------------------
int main(void){

    //initialization
    pc.printf("------------------ Manager service starts! --------------------------\n");
        //source & destination ID setting
    pc.getc();
    pc.printf("endnode : %i\n", myId);

    //initialize lower layer stacks
    L2_initFSM(myId);
    L3_initFSM();
    
    while(1)
    {
        L2_FSMrun();
        L3_FSMrun();
    }
}