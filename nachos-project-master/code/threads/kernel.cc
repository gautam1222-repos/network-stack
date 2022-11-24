// kernel.cc
//	Initialization and cleanup routines for the Nachos kernel.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "main.h"
#include "kernel.h"
#include "sysdep.h"
#include "synch.h"
#include "synchlist.h"
#include "libtest.h"
#include "string.h"
#include "synchconsole.h"
#include "synchdisk.h"
#include "post.h"
#include "ethernet.h"
#include "ipv4header.h"
#include "udpheader.h"
#include <cstdlib>
#include <time.h>

#define MAX_PROCESS 10
//----------------------------------------------------------------------
// Kernel::Kernel
// 	Interpret command line arguments in order to determine flags
//	for the initialization (see also comments in main.cc)
//----------------------------------------------------------------------

Kernel::Kernel(int argc, char **argv) {
    randomSlice = FALSE;
    debugUserProg = FALSE;
    consoleIn = NULL;   // default is stdin
    consoleOut = NULL;  // default is stdout
#ifndef FILESYS_STUB
    formatFlag = FALSE;
#endif
    reliability = 1;  // network reliability, default is 1.0
    hostName = 0;     // machine id, also UNIX socket name
                      // 0 is the default machine id
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rs") == 0) {
            ASSERT(i + 1 < argc);
            RandomInit(atoi(argv[i + 1]));  // initialize pseudo-random
                                            // number generator
            randomSlice = TRUE;
            i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            debugUserProg = TRUE;
        } else if (strcmp(argv[i], "-ci") == 0) {
            ASSERT(i + 1 < argc);
            consoleIn = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "-co") == 0) {
            ASSERT(i + 1 < argc);
            consoleOut = argv[i + 1];
            i++;
#ifndef FILESYS_STUB
        } else if (strcmp(argv[i], "-f") == 0) {
            formatFlag = TRUE;
#endif
        } else if (strcmp(argv[i], "-n") == 0) {
            ASSERT(i + 1 < argc);  // next argument is float
            reliability = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-m") == 0) {
            ASSERT(i + 1 < argc);  // next argument is int
            hostName = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-u") == 0) {
            cout << "Partial usage: nachos [-rs randomSeed]\n";
            cout << "Partial usage: nachos [-s]\n";
            cout << "Partial usage: nachos [-ci consoleIn] [-co consoleOut]\n";
#ifndef FILESYS_STUB
            cout << "Partial usage: nachos [-nf]\n";
#endif
            cout << "Partial usage: nachos [-n #] [-m #]\n";
        }
    }
}

//----------------------------------------------------------------------
// Kernel::Initialize
// 	Initialize Nachos global data structures.  Separate from the
//	constructor because some of these refer to earlier initialized
//	data via the "kernel" global variable.
//----------------------------------------------------------------------

void Kernel::Initialize(char *userProgName /*=NULL*/) {
    // We didn't explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a Thread
    // object to save its state.
    currentThread = new Thread(userProgName);
    currentThread->setStatus(RUNNING);

    stats = new Statistics();        // collect statistics
    interrupt = new Interrupt;       // start up interrupt handling
    scheduler = new Scheduler();     // initialize the ready queue
    alarm = new Alarm(randomSlice);  // start up time slicing
    machine = new Machine(debugUserProg);
    synchConsoleIn = new SynchConsoleInput(consoleIn);     // input from stdin
    synchConsoleOut = new SynchConsoleOutput(consoleOut);  // output to stdout
    synchDisk = new SynchDisk();                           //
#ifdef FILESYS_STUB
    fileSystem = new FileSystem();
#else
    fileSystem = new FileSystem(formatFlag);
#endif  // FILESYS_STUB
    postOfficeIn = new PostOfficeInput(10);
    postOfficeOut = new PostOfficeOutput(reliability);

    addrLock = new Semaphore("addrLock", 1);
    gPhysPageBitMap = new Bitmap(128);
    semTab = new STable();
    pTab = new PTable(MAX_PROCESS);

    interrupt->Enable();
}

//----------------------------------------------------------------------
// Kernel::~Kernel
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------

Kernel::~Kernel() {
    delete stats;
    delete interrupt;
    delete scheduler;
    delete alarm;
    delete machine;
    delete synchConsoleIn;
    delete synchConsoleOut;
    delete synchDisk;
    delete fileSystem;
    delete postOfficeIn;
    delete postOfficeOut;
    delete pTab;
    delete gPhysPageBitMap;
    delete semTab;
    delete addrLock;

    Exit(0);
}

//----------------------------------------------------------------------
// Kernel::ThreadSelfTest
//      Test threads, semaphores, synchlists
//----------------------------------------------------------------------

void Kernel::ThreadSelfTest() {
    Semaphore *semaphore;
    SynchList<int> *synchList;

    LibSelfTest();  // test library routines

    currentThread->SelfTest();  // test thread switching

    // test semaphore operation
    semaphore = new Semaphore("test", 0);
    semaphore->SelfTest();
    delete semaphore;

    // test locks, condition variables
    // using synchronized lists
    synchList = new SynchList<int>;
    synchList->SelfTest(9);
    delete synchList;
}

//----------------------------------------------------------------------
// Kernel::ConsoleTest
//      Test the synchconsole
//----------------------------------------------------------------------

void Kernel::ConsoleTest() {
    char ch;

    cout << "Testing the console device.\n"
         << "Typed characters will be echoed, until ^D is typed.\n"
         << "Note newlines are needed to flush input through UNIX.\n";
    cout.flush();

    do {
        ch = synchConsoleIn->GetChar();
        if (ch != EOF) synchConsoleOut->PutChar(ch);  // echo it!
    } while (ch != EOF);

    cout << "\n";
}

//----------------------------------------------------------------------
// Kernel::NetworkTest
//      Test whether the post office is working. On machines #0 and #1, do:
//
//      1. send a message to the other machine at mail box #0
//      2. wait for the other machine's message to arrive (in our mailbox #0)
//      3. send an acknowledgment for the other machine's message
//      4. wait for an acknowledgement from the other machine to our
//          original message
//
//  This test works best if each Nachos machine has its own window
//----------------------------------------------------------------------

void Kernel::NetworkTest() {
    if (hostName ==1 || hostName == 0) {
        
        cout<< "\\Host Machine :" <<hostName << "\\"<<endl;


        //Application Layer
        //Sender Address determination
        unsigned char* farHost = MacPool[1-hostName];
        //Transmitting Data for Communication
        char data[10000];
        for(int i=0; i<10000; i++){
            data[i] = 'a'+i%26;
        }
        int datasize = strlen(data);
        //Sending port layers
        unsigned short int srcPort = 443;
        unsigned short int destPort = 445;
        

        //Transport Layer
        UdpHeader UdpHdr;
        UdpHdr.srcPort = srcPort;
        UdpHdr.destPort = destPort;
        UdpHdr.checksum = 0;
        UdpHdr.length = datasize+8;

        UdpPacket udpPkt;
        udpPkt.UdpHdr = UdpHdr;
        udpPkt.data = data; 


        //Network Layer
        struct ipv4Header ipHdr;
        memset(&ipHdr, 0, sizeof(ipHdr));
        ipHdr.version = 4;
        ipHdr.ihl = 20;
        //ip.len;
        srand(time(0));
        ipHdr.id = rand()&(1<<16-1);
        ipHdr.flags = 0;
        ipHdr.frag_offset = 0;
        ipHdr.check_sum = 0;
        ipHdr.ttl = 255;
        ipHdr.protocol = 17; //UDP protocol No. = 17
        ipHdr.src_addr = IPpool[hostName];
        ipHdr.dest_addr = IPpool[1-hostName];
        
        // TODO : Insert ipHdr.len, ipHdr.tos
        // TODO : Covert each Fragment into packet and insert into Payload of Ethernet Frame
        
        //fragmentation
        int numFrag = (udpPkt.UdpHdr.length-8)/1472 +1;
        for(int i=0; i<numFrag; i++)
        {

            cout << "Fragmenting ip packet no." << i << "\n";
            ipHdr.frag_offset = i*1472/8;
            if(i!= numFrag-1)
            {
                cout << "NumFrag : " << i << ",\t" << numFrag << "\n";
                ipHdr.flags = 7;
            }
            else
            {
                ipHdr.flags = 0;
            }

            cout<<"........Ip details........"<<i<<"\n";
            cout<<"Id -> "<<ipHdr.id<<endl;
            cout<<"Flag -> "<<ipHdr.flags<<endl;
            cout<<"Offset -> " << ipHdr.frag_offset<<endl;
            struct ethernetHeader ethHdr;


            char packetBuffer[1500];
            memset(&ethHdr, 0, sizeof(ethHdr));
            memcpy(packetBuffer, &ipHdr, sizeof(ipHdr));
            int sizeLeft = i!=numFrag? 1472 : datasize%1472;
            memcpy(packetBuffer+sizeof(ipHdr), data+i*1472, sizeLeft);
            
            // To: destination machine, mailbox 0
            // From: our machine, reply to: mailbox 1
            
            memcpy(ethHdr.destMAC, farHost, 6);
            memcpy(ethHdr.srcMAC, MacPool[hostName], 6);
            memcpy(ethHdr.payload,packetBuffer,1500);
            char buf[sizeof(ipHdr)];
            memcpy(buf, &ipHdr, sizeof(ipHdr));
            postOfficeOut->Send(ethHdr); 
        }

        // ip.len = udpPkt.UdpHdr.length +ip.ihl;
        
        


        // if we're machine 1, send to 0 and vice versa
        /*farHost => our listener*/
        //generating packet for to and from addresses
        
        //generating ethernet Header
        
        // PacketHeader pkthdr;
        //setting all values in ethernet Header

        // what does this buffer store
        // char buffer[MaxMailSize];
        

        //generating IP header
        


        

    }

    // Then we're done!
}
