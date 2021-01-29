//
//  cmdDecoder.cpp
//  cmdDecoder++
//
//  Created by Andrea Carrara on 29/01/21.
//  Copyright © 2021 Andrea Carrara. All rights reserved.
//

#include "cmdDecoder.hpp"
#include "string.h"
void cmdDecoder::findCmd(char *s) {
    if(strcmp(s,"cmd1")==0) {
        cmd=1;
        cmdlen=1;
        return;
    }
    if(strcmp(s,"otaaddress")==0) {
        cmd=2;
        cmdlen=2;
        return;
    }
    if(strcmp(s,"cmd3")==0) {
        cmd=3;
        cmdlen=3;
        return;
    }
}

cmdDecoder::cmdDecoder() {
    strcpy(delim," ");
}
bool cmdDecoder::parse(char *s) {
    char* token = strtok(s, delim);
    int toknum=0;
    
    while (token)
    {
        if(toknum==0) findCmd(token);
        if(toknum==cmdlen) {
            err="too much params";
            return false;
        }
        if(toknum>0) params.push_back(token);
        token = strtok(NULL, delim);
        toknum++;
    }
    if(toknum<cmdlen) {
        err="too few params";
        return false;
    }
    return true;
}
