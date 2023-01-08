#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>

time_t now;
time_t start,tlastchange;
bool inp,previnp,out=false,toggle=false;
int ton=5,toff=5;


static struct termios oldTermios, newTermios;



void changestate(bool s) {
    out=s;
    tlastchange=now;
}

int main() {
    char c;
    bool on=false,off=false;

    start = time(NULL);

    tcgetattr(STDIN_FILENO, &oldTermios);
    newTermios = oldTermios;
    cfmakeraw(&newTermios);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);

    int modo;
    if(toggleMode) modo=1; else if(!tOn) modo=2; else modo=3;

    while(true) {
        now = time(NULL)-start;

        if(!feof(stdin)) fread(&c,1,1,stdin);

        if(c==13) break;
        
        inp=(c==105); // i
        on=(c==111); // k
        off=(c==112); // p

        printf("\rmodo: %d input: %d previnp: %d out: %d time: %ld lastchange: %ld", modo, inp,previnp,out,now,tlastchange);
        fflush(stdout);
        
        if(!out) {
            if(on) { on=false;changestate(true);continue;}
            switch (modo)
            {
            case 1:
            case 2:
                if(inp && !previnp) {changestate(true);break;}
                break;
            case 3:
                if((inp && !previnp) || (inp && (now-tlastchange)>toff)) { changestate(true);break;}
                break;
            default:
                assert(0);
                break;
            }
            //if(!toggle && inp && toff && (now-tlastchange)>toff) {changestate(true);continue;}
            //if(!toggle && inp) { changestate(true);continue;}
        } 
        else {
            if(off) { off=false;changestate(false);continue;}
            switch (modo)
            {
            case 1:
                if(inp && !previnp) { changestate(false);break;}
                if(ton && (now-tlastchange)>ton) { changestate(false);break;}
                break;
            case 2:
            case 3:
                if(!inp && previnp) { changestate(false);break;}
                if(ton && (now-tlastchange)>ton) { changestate(false);break;}
                break;
            
            default:
                assert(0);
                break;
            }
            // if(toggle && (inp && !previnp)) {changestate(false);continue;}
            // if(toggle && ton && (now-tlastchange)>ton) {changestate(false);continue;}
            // if(!toggle && inp && ton && (now-tlastchange)>ton) {changestate(false);continue;}
            // if(!toggle && !inp) { changestate(false);continue;}
        }

        previnp=inp;
     

        //sleep(1);

    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);

}

