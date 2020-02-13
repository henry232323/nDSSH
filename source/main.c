#include <nds.h>
#include <dswifi9.h>
#include <netinet/in.h>
#include <nds/arm9/console.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <sys/select.h>

Wifi_AccessPoint *findAP();


//---------------------------------------------------------------------------------
void keyPressed(int c) {
//---------------------------------------------------------------------------------
    if (c > 0) iprintf("%c", c);
}

Wifi_AccessPoint *init_wifi() {
    Wifi_InitDefault(false);

    consoleDemoInit();

    Keyboard *kb = keyboardDemoInit();
    kb->OnKeyPressed = keyPressed;

    int status = ASSOCSTATUS_DISCONNECTED;

    consoleClear();
    consoleSetWindow(NULL, 0, 0, 32, 24);

    Wifi_AccessPoint *ap = findAP();

    consoleClear();
    consoleSetWindow(NULL, 0, 0, 32, 10);

    iprintf("Connecting to %s\n", ap->ssid);

    //this tells the wifi lib to use dhcp for everything
    Wifi_SetIP(0, 0, 0, 0, 0);
    char wepkey[64];
    int wepmode = WEPMODE_NONE;
    if (ap->flags & WFLAG_APDATA_WEP) {
        iprintf("Enter Wep Key\n");
        while (wepmode == WEPMODE_NONE) {
            scanf("%s", wepkey);
            if (strlen(wepkey) <= 13) {
                wepmode = WEPMODE_128BIT;
            } else if (strlen(wepkey) == 5) {
                wepmode = WEPMODE_40BIT;
            } else {
                iprintf("Invalid key!\n");
            }
        }
        Wifi_ConnectAP(ap, wepmode, 0, (u8 *) wepkey);
    } else {
        Wifi_ConnectAP(ap, WEPMODE_NONE, 0, 0);
    }
    consoleClear();
    while (status != ASSOCSTATUS_ASSOCIATED && status != ASSOCSTATUS_CANNOTCONNECT) {

        status = Wifi_AssocStatus();
        int len = strlen(ASSOCSTATUS_STRINGS[status]);
        iprintf("\x1b[0;0H\x1b[K");
        iprintf("\x1b[0;%dH%s", (32 - len) / 2, ASSOCSTATUS_STRINGS[status]);

        scanKeys();

        if (keysDown() & KEY_B) break;

        swiWaitForVBlank();
    }


    if (status == ASSOCSTATUS_ASSOCIATED) {
        u32 ip = Wifi_GetIP();

        iprintf("\nip: [%li.%li.%li.%li]\n", (ip) & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    }
    return ap;
}

Wifi_AccessPoint *findAP() {
    int selected = 0;
    int i;
    int count = 0, displaytop = 0;

    static Wifi_AccessPoint ap;

    Wifi_ScanMode(); //this allows us to search for APs

    int pressed = 0;
    do {

        scanKeys();
        pressed = keysDown();

        if (pressed & KEY_START) exit(0);

        //find out how many APs there are in the area
        count = Wifi_GetNumAP();


        consoleClear();

        iprintf("%d APs detected\n\n", count);

        int displayend = displaytop + 10;
        if (displayend > count) displayend = count;

        //display the APs to the user
        for (i = displaytop; i < displayend; i++) {
            Wifi_AccessPoint ap;

            Wifi_GetAPData(i, &ap);

            // display the name of the AP
            iprintf("%s %.29s\n  Wep:%s Sig:%i\n",
                    i == selected ? "*" : " ",
                    ap.ssid,
                    ap.flags & WFLAG_APDATA_WEP ? "Yes " : "No ",
                    ap.rssi * 100 / 0xD0);

        }

        //move the selection asterick
        if (pressed & KEY_UP) {
            selected--;
            if (selected < 0) {
                selected = 0;
            }
            if (selected < displaytop) displaytop = selected;
        }

        if (pressed & KEY_DOWN) {
            selected++;
            if (selected >= count) {
                selected = count - 1;
            }
            displaytop = selected - 9;
            if (displaytop < 0) displaytop = 0;
        }
        swiWaitForVBlank();
    } while (!(pressed & KEY_A));

    //user has made a choice so grab the ap and return it
    Wifi_GetAPData(selected, &ap);

    return &ap;
}

void read_line(int my_socket) {
    int recvd_len;
    char incoming_buffer[256];

    while ((recvd_len = recv(my_socket, incoming_buffer, 255, 0)) !=
           0) { // if recv returns 0, the socket has been closed.
        if (recvd_len > 0) { // data was received!
            incoming_buffer[recvd_len] = 0; // null-terminate
            iprintf(incoming_buffer);
        }
    }
}

int get_input(int my_socket) {
    iprintf(">>> ");
    char data[64];
    scanf("%s\n", data);
    if (strcmp(data, "quit") == 0) {
        return 1;
    }

    int length = 64;
    for (int i = 0; i < 64; i++) {
        if (data[i] == 0) {
            length = i + 1;
            break;
        }
    }
    if (length < 64) {
        data[length] = '\n';
        length++;
    }

    send(my_socket, data, length, 0);
    return 0;
}

void connect_app() {

    PrintConsole topScreen;
    PrintConsole bottomScreen;

    Keyboard *kb = keyboardDemoInit();
    kb->OnKeyPressed = keyPressed;

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

//    consoleSetWindow(&topScreen, 0, 0, 256, 192);
//    consoleSetWindow(&bottomScreen, 0, 0, 256, 96);

    consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);


    consoleSelect(&topScreen);

    char ip[11] = "10.3.141.1";
    // Find the IP address of the server, with gethostbyname
    iprintf("Connecting to %s\n", ip);
    struct hostent *myhost = gethostbyname(ip);
    iprintf("Found IP Address!\n");

    // Create a TCP socket
    int my_socket;
    my_socket = socket(AF_INET, SOCK_STREAM, 0);
    iprintf("Created Socket!\n");

    // Tell the socket to connect to the IP address we found, on port 80 (HTTP)
    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    sain.sin_port = htons(5678);
    sain.sin_addr.s_addr = *((unsigned long *) (myhost->h_addr_list[0]));
    connect(my_socket, (struct sockaddr *) &sain, sizeof(sain));
    iprintf("Connected to server!\n");

    while (1) {
        consoleSelect(&bottomScreen);
        int _quit = get_input(my_socket);
        if (_quit) break;
        consoleSelect(&topScreen);
        read_line(my_socket);
    }


    shutdown(my_socket, 0); // good practice to shutdown the socket
    closesocket(my_socket); // remove the socket.
}

int main(void) {
    init_wifi();
    connect_app();


    while (1) {
        swiWaitForVBlank();
        int keys = keysDown();
        if (keys & KEY_START) break;
    }

    return 0;
}
