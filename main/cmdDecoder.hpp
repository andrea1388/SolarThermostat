//
//  cmdDecoder.hpp
//  cmdDecoder++
//
//  Created by Andrea Carrara on 29/01/21.
//  Copyright © 2021 Andrea Carrara. All rights reserved.
//

#ifndef cmdDecoder_hpp
#define cmdDecoder_hpp

//#include <stdio.h>
#include <string>
#include <vector>
using namespace std;

class cmdDecoder {
public:
    cmdDecoder();
    bool parse(char *);
    int cmd;
    vector<string> params;
    string err;
private:
    int cmdlen;
    char delim[1];
    void findCmd(char*);
};
#endif /* cmdDecoder_hpp */
