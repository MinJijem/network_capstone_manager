#include "L3_FSMmain.h"
#include "L3_FSMevent.h"
#include "L3_msg.h"
#include "L3_timer.h"
#include "L3_LLinterface.h"
#include "protocol_parameters.h"
#include "mbed.h"
//#include <Windows.h>



//FSM state -------------------------------------------------
#define L3STATE_IDLE                0
#define TX                          1


//state variables
static uint8_t main_state = L3STATE_IDLE; //protocol state
static uint8_t prev_state = main_state;

//SDU (input)
static uint8_t originalWord[1030];
static uint8_t wordLen=0;

static uint8_t sdu[1030];

//serial port interface
static Serial pc(USBTX, USBRX);

//match
static uint8_t matchArr[256] = {0}; //매칭대기열
static uint8_t matchShuffledArr[256] = {0}; 
static int matchCnt=0;
static uint8_t mtchBackUpArr[256] = {0};
static uint8_t blackArr[256] = {0}; //black 원하는지 0,1로 저장함. 1이 yes.
static uint8_t mtchCpl;

static int txIndex=0;

//application event handler : generating SDU from keyboard input
static void L3service_processInputWord(void)
{
    char c = pc.getc();
    if (!L3_event_checkEventFlag(L3_event_dataToSend))
    {
        if (c == '\n' || c == '\r')
        {
            originalWord[wordLen++] = '\0';
            L3_event_setEventFlag(L3_event_dataToSend);
            debug_if(DBGMSG_L3,"word is ready! ::: %s\n", originalWord);
        }
        else
        {
            originalWord[wordLen++] = c;
            if (wordLen >= L3_MAXDATASIZE-1)
            {
                originalWord[wordLen++] = '\0';
                L3_event_setEventFlag(L3_event_dataToSend);
                pc.printf("\n max reached! word forced to be ready :::: %s\n", originalWord);
            }
        }
    }
}


void L3_initFSM()
{
    //initialize service layer
    pc.attach(&L3service_processInputWord, Serial::RxIrq);
}

void L3_FSMrun(void)
{   
    if (prev_state != main_state)
    {
        debug_if(DBGMSG_L3, "[L3] State transition from %i to %i\n", prev_state, main_state);
        prev_state = main_state;
    }

    //FSM should be implemented here! ---->>>>
    switch (main_state)
    {
        case L3STATE_IDLE: //IDLE state description
            
            if (L3_event_checkEventFlag(L3_event_msgRcvd)) //msgRcvd is matchRcvd
            {
                //Retrieving data info.
                uint8_t* dataPtr = L3_LLI_getMsgPtr();
                uint8_t size = L3_LLI_getSize();
                uint8_t id = L3_LLI_getSrcId();

                debug("\n -------------------------------------------------\nRCVD Match Req by %i, (length:%i,sentence:%s)\n -------------------------------------------------\n", 
                            id, size, dataPtr);
                
                //매칭 대기열에 추가
                matchArr[matchCnt] = id;
                matchCnt++;
                checkMatchCdtion();
                pc.printf("the number of matching user: %d\n",matchCnt);

                //blacklist
                blackListAdd(dataPtr, id);
                L3_event_clearEventFlag(L3_event_msgRcvd);
            }
            /*
            else if (L3_event_checkEventFlag(L3_event_matchStart)){
                uint8_t mtchCpl; //match complete
                mtchCpl = MATCHING(matchCnt);
                uint8_t user1=0, user2=0;

                cleanMATCH(mtchCpl);

                for(int i=0; i<mtchCpl; i+=2){
                    user1 = matchShuffledArr[i];
                    user2 = matchShuffledArr[i+1];

                    if(isBlackList(user1, user2)){
                        matchArr[matchCnt] = user1;
                        matchCnt++;
                        matchArr[matchCnt] = user2;
                        matchCnt++;
                        checkMatchCdtion();
                    }

                    else {
                        pc.printf("user1:%d,user2:%d",user1,user2);
                        L3_LLI_dataReqFunc(&user2, sizeof(uint8_t), user1);
                        //Sleep(1000);
                        pc.getc();
                        pc.getc();

                        L3_LLI_dataReqFunc(&user1, sizeof(uint8_t), user2);
                        
                        main_state = TX;

                        //user set back-up(for blacklist)
                        mtchBackUpArr[user1] = user2;
                        mtchBackUpArr[user2] = user1;
                        }
                    }
                    L3_event_clearEventFlag(L3_event_matchStart);
            }*/
            else if (L3_event_checkEventFlag(L3_event_matchStart)){
                mtchCpl = MATCHING(matchCnt);
                cleanMATCH(mtchCpl);
                
                L3_event_clearEventFlag(L3_event_matchStart);
                L3_event_setEventFlag(L3_event_matchingTx);
            }
            else if(L3_event_checkEventFlag(L3_event_matchingTx)){
                uint8_t user=0;
                uint8_t mate=0;
                
                if(txIndex<mtchCpl){
                    user = matchShuffledArr[txIndex];
                    if(txIndex%2==0){
                        mate = matchShuffledArr[txIndex+1];
                    }
                    else{
                        mate = matchShuffledArr[txIndex-1];
                    }

                    if(isBlackList(user, mate)){
                        matchArr[matchCnt] = user;
                        matchCnt++;
                    }
                    else {
                        pc.printf("user:%d,mate:%d\n",user,mate);
                        L3_LLI_dataReqFunc(&mate, sizeof(uint8_t), user);
                        
                        //user set back-up(for blacklist)
                        mtchBackUpArr[user] = mate;

                        main_state = TX;
                        
                        }
                    txIndex++;
                    break;

                }
                else{
                    L3_event_clearEventFlag(L3_event_matchingTx);
                    txIndex = 0;
                }
                
            }
            else{
                checkMatchCdtion();
            }
            break;
        case TX:
            if(L3_event_checkEventFlag(L3_event_dataSendCnf)){
                debug("\ndata sended!\n");
                main_state = L3STATE_IDLE;
                L3_event_clearEventFlag(L3_event_dataSendCnf);
            }
            break;
        default :
            break;
    }
}

void blackListAdd(uint8_t * word, uint8_t id){
    if(word[0] == 'y' || word[0] == 'Y'){
        blackArr[id] = 1;
        pc.printf("blacklist added: user id %i\n",id);
    }
    else{
        blackArr[id] = 0;
    }
}

int MATCHING(int size){
    //대기자수=size -> 배열 끝 index=size-1
    //except odd end user
    uint8_t temp_size = size;
    if(size%2==1){ 
        size--;
        matchCnt=1;
    }
    else{
        matchCnt=0;
    }
    if(size==2){
        for(int i=0;i<2;i++){
            matchShuffledArr[i] = matchArr[i];
        }
    }
    else{
    //shuffle
    srand(time(NULL));
    int ranNum;
    //int temp;
    for(int i=0;i<=size-1;i++){
        ranNum = rand() % (size - i) + i;
        //temp = matchArr[ranNum];
        //matchArr[ranNum] = matchArr[i];
        //matchArr[i] = temp;
        matchShuffledArr[ranNum] = matchArr[i];
        matchShuffledArr[i] = matchArr[ranNum];
    }
    }
    printArr(temp_size);
    pc.printf("the number of shuffled user: %d\n",size);
    return size;
}

void cleanMATCH(int num){
    int i=0;
    int end=num-1;
    if(matchCnt){
        i=1;
        end=num;
        matchArr[0] = matchArr[num];
    }
    while(i<=end){
        matchArr[i] = 0;
        i++;
    }
    printArr(num);
}

void printArr(int len){
    pc.printf("matchArray: [");
    for(int i=0;i<len;i++){
        printf("%d ",matchShuffledArr[i]);
    }
    printf("]\n");
}

int checkMatchCdtion(){
    if (matchCnt>=2 && matchCnt<4 && L3_timer_getTimerStatus()==0){
        L3_timer_startTimer();
    }
    if (matchCnt>=4 || L3_event_checkEventFlag(L3_event_Timeout)){
        L3_event_setEventFlag(L3_event_matchStart);
        L3_event_clearEventFlag(L3_event_Timeout);
    }
    else {
        return 0;
    }
}

int isBlackList(uint8_t user1, uint8_t user2){
    if(blackArr[int(user1)] && mtchBackUpArr[int(user1)]==user2){
        return 1;
    }
    if(blackArr[int(user2)] && mtchBackUpArr[int(user2)]==user1){
        return 1;
    }
    return 0;
}