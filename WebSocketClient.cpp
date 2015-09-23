//#define DEBUGGING

#include "global.h"
#include "WebSocketClient.h"

#include "sha1.h"
#include "base64.h"


bool WebSocketClient::handshake(Client &client) {

    socketClient = &client;

    // If there is a connected client->
    if (socketClient->connected()) {
        // Check request and look for websocket handshake
#ifdef DEBUGGING
            Serial.println(F("Client connected"));
#endif
        if (analyzeRequest()) {
#ifdef DEBUGGING
                Serial.println(F("Websocket established"));
#endif

                return true;

        } else {
            // Might just need to break until out of socketClient loop.
#ifdef DEBUGGING
            Serial.println(F("Invalid handshake"));
#endif
            disconnectStream();

            return false;
        }
    } else {
        return false;
    }
}

bool WebSocketClient::analyzeRequest() {
    char headerBuffer[52];

    int i;
    int bite;
    bool upgradeFound = false;
    unsigned long intkey[2];
    char serverKey[29];
    char keyStart[17];
    char b64Key[25];
    char key[25];

    randomSeed(analogRead(0));

    for (i=0; i<16; ++i) {
        keyStart[i] = (char)random(1, 256);
    }

    base64_encode(b64Key, keyStart, 16);

    for (i=0; i<24; ++i) {
        key[i] = b64Key[i];
    }
    key[24] = '\0';

#ifdef DEBUGGING
    Serial.println(F("Sending websocket upgrade headers"));
#endif    

    socketClient->print(F("GET "));
    socketClient->print(path);
    socketClient->print(F(" HTTP/1.1\r\n"));
    socketClient->print(F("Upgrade: websocket\r\n"));
    socketClient->print(F("Connection: Upgrade\r\n"));
    socketClient->print(F("Host: "));
    socketClient->print(host);
    socketClient->print(CRLF); 
    socketClient->print(F("Sec-WebSocket-Key: "));
    socketClient->print(key);
    socketClient->print(CRLF);
    socketClient->print(F("Sec-WebSocket-Protocol: "));
    socketClient->print(protocol);
    socketClient->print(CRLF);
    socketClient->print(F("Sec-WebSocket-Version: 13\r\n"));
    socketClient->print(CRLF);

#ifdef DEBUGGING
    Serial.println(F("Analyzing response headers"));
#endif    

    while (socketClient->connected() && !socketClient->available()) {
        delay(100);
        Serial.println("Waiting...");
    }

    // TODO: More robust string extraction
    int count = 0;

    while ((bite = socketClient->read()) != -1) {

        headerBuffer[count++] = (char)bite;

        if ((char)bite == '\n') {
            headerBuffer[count] = '\0';
#ifdef DEBUGGING
            Serial.print("Got Header: ");
            Serial.print(headerBuffer);
#endif
            // Check if string starts with any of these markers
            if (!upgradeFound && strncmp("Upgrade: websocket", headerBuffer, 18) == 0) {
                upgradeFound = true;
            } 
            else if (strncmp("Sec-WebSocket-Accept: ", headerBuffer, 22) == 0) {
                for (int i=0; i<28; i++) {
                    serverKey[i] = headerBuffer[i + 22];
                }
                serverKey[28] = '\0';
            }

            count = 0;		
        }

        if (!socketClient->available()) {
          delay(20);
        }
    }

    uint8_t *hash;
    char result[21];
    char b64Result[29];

    Sha1.init();
    Sha1.print(key);
    Sha1.print("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    hash = Sha1.result();

    for (i=0; i<20; i++) {
        result[i] = (char)hash[i];
    }
    result[20] = '\0';

    base64_encode(b64Result, result, 20);

    // if the keys match, good to go
    return strcmp(serverKey, b64Result) == 0;
}


bool WebSocketClient::handleStream(char *data, uint8_t *opcode) {
    uint8_t msgtype;
    uint8_t bite;
    unsigned int length;
    uint8_t mask[4];
    uint8_t index;
    unsigned int i;
    bool hasMask = false;

    if (!socketClient->connected() || !socketClient->available())
    {
        return false;
    }      

    msgtype = timedRead();
    if (!socketClient->connected()) {
        return false;
    }

    length = timedRead();

    if (length & WS_MASK) {
        hasMask = true;
        length = length & ~WS_MASK;
    }


    if (!socketClient->connected()) {
        return false;
    }

    index = 6;

    if (length == WS_SIZE16) {
        length = timedRead() << 8;
        if (!socketClient->connected()) {
            return false;
        }
            
        length |= timedRead();
        if (!socketClient->connected()) {
            return false;
        }   

    } else if (length == WS_SIZE64) {
#ifdef DEBUGGING
        Serial.println(F("No support for over 16 bit sized messages"));
#endif
        return false;
    }

    if (hasMask) {
        // get the mask
        for (i=0; i<4; i++) {
            mask[i] = timedRead();

            if (!socketClient->connected())
                return false;
        }
    }
        
    // Clear garbage in data
    // for (i=0; i<strlen(data); i++) {
    //     data[i] = 0;
    // }
        
    if (opcode != NULL)
      *opcode = msgtype & ~WS_FIN;
        
    for (i=0; i<length; i++) {
        data[i] = hasMask ? 
            (char) (timedRead() ^ mask[i % 4]) : (char) timedRead();
        if (!socketClient->connected()) {
            return false;
        }
    }
    data[i] = '\0';
    
    return true;
}

void WebSocketClient::disconnectStream() {
#ifdef DEBUGGING
    Serial.println(F("Terminating socket"));
#endif
    // Should send 0x8700 to server to tell it I'm quitting here.
    socketClient->write((uint8_t) 0x87);
    socketClient->write((uint8_t) 0x00);
    
    socketClient->flush();
    delay(10);
    socketClient->stop();
}

bool WebSocketClient::getData(char *data, uint8_t *opcode) {
    return handleStream(data, opcode);
}    

void WebSocketClient::sendData(char *data, uint8_t opcode) {
#ifdef DEBUGGING
    Serial.print(F("Sending data: "));
    Serial.println(data);
#endif
    if (socketClient->connected()) {
        sendEncodedData(data, opcode);       
    }
}

// void WebSocketClient::sendData(String data, uint8_t opcode) {
// #ifdef DEBUGGING
//     Serial.print(F("Sending data: "));
//     Serial.println(data);
// #endif
//     if (socketClient->connected()) {
//         int size = data.length() + 1;
//         char cstr[size];

//         data.toCharArray(cstr, size);

//         sendEncodedData(cstr, opcode);
//     }
// }

int WebSocketClient::timedRead() {
  while (!socketClient->available()) {
    delay(20);  
  }

  return socketClient->read();
}

void WebSocketClient::sendEncodedData(char *data, uint8_t opcode) {
    int i;
    uint8_t mask[4];
    int size = strlen(data);

    // Opcode; final fragment
    socketClient->write(opcode | WS_FIN);

    // NOTE: no support for > 16-bit sized messages
    if (size > 125) {
        socketClient->write(WS_SIZE16 | WS_MASK);
        socketClient->write((uint8_t) (size >> 8));
        socketClient->write((uint8_t) (size & 0xFF));
    } else {
        socketClient->write((uint8_t) size | WS_MASK);
    }

    for (i=0; i<4; i++) {
        socketClient->write(mask[i] = random(0, 256));   
    }
     
    for (i=0; i<size; i++) {
        socketClient->write(data[i] ^ mask[i % 4]);
    }
}

// void WebSocketClient::sendEncodedData(String str, uint8_t opcode) {
//     int size = str.length() + 1;
//     char cstr[size];

//     str.toCharArray(cstr, size);

//     sendEncodedData(cstr, opcode);
// }
