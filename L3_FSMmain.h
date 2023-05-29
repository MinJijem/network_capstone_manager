#include <stdint.h>
void L3_initFSM();
void L3_FSMrun(void);
void blackListAdd(uint8_t *, uint8_t);
int MATCHING(int size);
void cleanMATCH(int num);
void printArr(int len);
int checkMatchCdtion();
int isBlackList(uint8_t user1, uint8_t user2);
