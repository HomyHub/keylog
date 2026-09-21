#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "keylogger.h"

#define BUFFER_SIZE 100
#define NUM_KEYCODES 71

const char *keycodes[] = {
    /* The QR scanner is a HID keyboard. Only the characters that can appear in
       a MAC address are emitted; every other key maps to "" and is skipped.

       Shift is not tracked, so a key maps to its SHIFTED character. ':' is
       Shift+KEY_SEMICOLON on a US layout and Shift+KEY_DOT on a Spanish one,
       so both map to ':' -- neither '.' nor ';' ever appears in a MAC. */
    "",         /*  0 RESERVED   */
    "",         /*  1 ESC        */
    "1",        /*  2            */
    "2",        /*  3            */
    "3",        /*  4            */
    "4",        /*  5            */
    "5",        /*  6            */
    "6",        /*  7            */
    "7",        /*  8            */
    "8",        /*  9            */
    "9",        /* 10            */
    "0",        /* 11            */
    "",         /* 12 MINUS      */
    "",         /* 13 EQUAL      */
    "",         /* 14 BACKSPACE  */
    "",         /* 15 TAB        */
    "Q",        /* 16            */
    "W",        /* 17            */
    "E",        /* 18            */
    "R",        /* 19            */
    "T",        /* 20            */
    "Y",        /* 21            */
    "U",        /* 22            */
    "I",        /* 23            */
    "O",        /* 24            */
    "P",        /* 25            */
    "",         /* 26 LEFTBRACE  */
    "",         /* 27 RIGHTBRACE */
    "\n",       /* 28 ENTER      */
    "",         /* 29 LEFTCTRL   */
    "A",        /* 30            */
    "S",        /* 31            */
    "D",        /* 32            */
    "F",        /* 33            */
    "G",        /* 34            */
    "H",        /* 35            */
    "J",        /* 36            */
    "K",        /* 37            */
    "L",        /* 38            */
    ":",        /* 39 SEMICOLON  US layout: Shift+; */
    "",         /* 40 APOSTROPHE */
    "",         /* 41 GRAVE      */
    "",         /* 42 LEFTSHIFT  */
    "",         /* 43 BACKSLASH  */
    "Z",        /* 44            */
    "X",        /* 45            */
    "C",        /* 46            */
    "V",        /* 47            */
    "B",        /* 48            */
    "N",        /* 49            */
    "M",        /* 50            */
    "",         /* 51 COMMA      */
    ":",        /* 52 DOT        ES layout: Shift+. */
    "",         /* 53 SLASH      */
    "",         /* 54 RIGHTSHIFT */
    "",         /* 55 KPASTERISK */
    "",         /* 56 LEFTALT    */
    "",         /* 57 SPACE      */
    "",         /* 58 CAPSLOCK   */
    "",         /* 59 F1         */
    "",         /* 60 F2         */
    "",         /* 61 F3         */
    "",         /* 62 F4         */
    "",         /* 63 F5         */
    "",         /* 64 F6         */
    "",         /* 65 F7         */
    "",         /* 66 F8         */
    "",         /* 67 F9         */
    "",         /* 68 F10        */
    "",         /* 69 NUMLOCK    sent by the scanner around each read */
    ""          /* 70 SCROLLLOCK */
};

int loop = 1;

void sigint_handler(int sig){
    loop = 0;
    exit(0);
}

/**
 * Ensures that the string pointed to by str is written to the file with file
 * descriptor file_desc.
 *
 * \returns 1 if writing completes succesfully, else 0
 */
int write_all(int file_desc, const char *str){
    int bytesWritten = 0;
    int bytesToWrite = strlen(str);

    if(bytesToWrite == 0){
        return 1;
    }

    do {
        bytesWritten = write(file_desc, str, bytesToWrite);

        if(bytesWritten == -1){
            return 0;
        }
        bytesToWrite -= bytesWritten;
        str += bytesWritten;
    } while(bytesToWrite > 0);

    return 1;
}


/**
 * Wrapper around write_all which exits safely if the write fails, without
 * the SIGPIPE terminating the program abruptly.
 */
void safe_write_all(int file_desc, const char *str, int keyboard){
    struct sigaction new_actn, old_actn;
    new_actn.sa_handler = SIG_IGN;
    sigemptyset(&new_actn.sa_mask);
    new_actn.sa_flags = 0;

    sigaction(SIGPIPE, &new_actn, &old_actn);

    if(!write_all(file_desc, str)){
        close(file_desc);
        close(keyboard);
        perror("\nwriting");
        exit(1);
    }

    sigaction(SIGPIPE, &old_actn, NULL);
}

void keylogger(int keyboard, int writeout){
    int eventSize = sizeof(struct input_event);
    int bytesRead = 0;
    struct input_event events[NUM_EVENTS];
    int i;

    signal(SIGINT, sigint_handler);

    while(loop){
        bytesRead = read(keyboard, events, eventSize * NUM_EVENTS);

        /* read() returns <0 (ENODEV) or 0 (EOF) when the USB scanner is
         * unplugged / re-enumerates. Without this the loop would busy-spin
         * forever on the dead fd and keylog_watch would never restart us to
         * re-detect the reconnected device. Exit cleanly so it does. */
        if(bytesRead < 0){
            if(errno == EINTR){
                continue;
            }
            break; /* device unplugged (ENODEV) */
        }
        if(bytesRead == 0){
            break; /* EOF — device removed */
        }

        for(i = 0; i < (bytesRead / eventSize); ++i){
            if(events[i].type == EV_KEY){
                if(events[i].value == 1){
                    if(events[i].code > 0 && events[i].code < NUM_KEYCODES){
                        safe_write_all(writeout, keycodes[events[i].code], keyboard);
                    }
                }
            }
        }
    }
}
