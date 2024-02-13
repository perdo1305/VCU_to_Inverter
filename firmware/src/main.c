/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdbool.h>  // Defines true
#include <stddef.h>   // Defines NULL
#include <stdlib.h>   // Defines EXIT_FAILURE

#include "definitions.h"  // SYS function prototypes

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
/*
    code developed by:
     ____  _____ ____  ____  _
    /  __\/  __//  __\/  _ \/ \ /\
    |  \/||  \  |  \/|| | \|| | ||
    |  __/|  /_ |    /| |_/|| \_/|
    \_/   \____\\_/\_\\____/\____/

    special thanks to joao vieira.
*/

// -- Defines -------------------------------------------

// Define a macro DEBUG para ativar ou desativar o debug_printf
#define DEBUG 1
#define LABVIEW 0

// Define o debug_printf para ser ativado ou desativado com base na macro DEBUG
#if DEBUG
#define debug_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...)
#endif

#if LABVIEW
#define labview_printf(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define labview_printf(fmt, ...)
#endif

#define MAX_SAVE_DATA 50

// -- CAN RX --------------------------------------------

static uint8_t rx_message[64];

uint32_t messageID = 0;
uint32_t rx_messageID = 0;
uint32_t status = 0;
uint8_t messageLength = 0;
uint8_t rx_messageLength = 0;
CANFD_MSG_RX_ATTRIBUTE msgAttr = CANFD_MSG_RX_DATA_FRAME;

uint32_t ID = 0;
uint8_t canrx[8];

volatile bool CANRX_ON = 0;

// -- CAN TX --------------------------------------------
volatile bool CANTX_ON = 0;

// -- millis --------------------------------------------

unsigned int previousMillis[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned int currentMillis[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// -- variables -----------------------------------------

//static uint8_t message[64];

// -- APPS ----------------------------------------------

//static uint8_t message_ADC[64];
static uint16_t ADC[64];
uint16_t APPS_average = 0;
uint16_t APPS_percent = 0;
bool error_flag = 0;

void Read_ADC(ADCHS_CHANNEL_NUM channel);
void Can_transmit(uint32_t id, uint8_t* message, uint8_t size);
bool APPS_Function(uint16_t APPS1, uint16_t APPS2);
bool R2D_sound();

unsigned int millis(void) {
    return (unsigned int)(CORETIMER_CounterGet() / (CORE_TIMER_FREQUENCY / 1000));
}

int main(void) {
    /* Initialize all modules */
    SYS_Initialize(NULL);
    UART1_Initialize();
    CAN1_Initialize();
    EEPROM_Initialize();

    // ADCGIRQEN2bits.AGIEN53 = 1;  // Enable for ADC53

    ADCHS_ModulesEnable(ADCHS_MODULE0_MASK);
    ADCHS_ModulesEnable(ADCHS_MODULE3_MASK);
    ADCHS_ModulesEnable(ADCHS_MODULE7_MASK);

    printf("██████╗░███████╗░██████╗███████╗████████╗\r\n");
    printf("██╔══██╗██╔════╝██╔════╝██╔════╝╚══██╔══╝\r\n");
    printf("██████╔╝█████╗░░╚█████╗░█████╗░░░░░██║░░░\r\n");
    printf("██╔══██╗██╔══╝░░░╚═══██╗██╔══╝░░░░░██║░░░\r\n");
    printf("██║░░██║███████╗██████╔╝███████╗░░░██║░░░\r\n");
    printf("╚═╝░░╚═╝╚══════╝╚═════╝░╚══════╝░░░╚═╝░░░\r\n");
    printf("\n\n");
    fflush(stdout);

    // startup sequence with leds

    GPIO_RC11_Set();
    CORETIMER_DelayMs(75);
    GPIO_RC2_Set();
    CORETIMER_DelayMs(75);
    GPIO_RC11_Clear();
    CORETIMER_DelayMs(75);
    GPIO_RC2_Clear();
    CORETIMER_DelayMs(75);
    GPIO_RC11_Set();
    CORETIMER_DelayMs(75);
    GPIO_RC2_Set();
    CORETIMER_DelayMs(75);
    GPIO_RC11_Clear();
    CORETIMER_DelayMs(75);
    GPIO_RC2_Clear();

    while (true) {
        SYS_Tasks(); /* Maintain state machines of all polled MPLAB Harmony modules. */
//
//        currentMillis[3] = millis();
//        if (currentMillis[3] - previousMillis[3] >= 20) {
//            // static uint8_t ADCres = 10;
//            // static uint8_t ADCmode = 2;  // 2 or 4
//            // float tempA = (float)(0.659 - (3.3 / ADCmode) * (1 - (ADC[0] / (2 ^ ADCres) - 1)) - 40) / 0.00132;
//            labview_printf("APPSA%dAPPSB%dAPPST%dAPPS_ERROR%dCAN_ERROR%dVCU_TEMP%dCANTX_ON%dCANRX_ON%dcanrx%d %d %d %d %d %d %d %d %d\r\n", ADC[0], ADC[3], APPS_percent, error_flag, status, ADC[53], CANTX_ON, CANRX_ON, ID, canrx[0], canrx[1], canrx[2], canrx[3], canrx[4], canrx[5], canrx[6], canrx[7]);
//            previousMillis[3] = currentMillis[3];
//
//            if (CANTX_ON || CANRX_ON) {
//                GPIO_RC2_Toggle();
//            }
//        }
//
//        // R2D_sound just 2 seconds of sound
//        static bool R2D_flag = 0;
//        static unsigned long R2D_start_time;
//        unsigned long current_time = millis();
//
//        if (current_time - R2D_start_time > 2000) {
//            R2D_flag = 0;
//        }
//        // R2D_sound(R2D_flag);
//
//        // toggle RC11 every 300ms
//        currentMillis[2] = millis();
//        if (currentMillis[2] - previousMillis[2] >= 300) {
//            GPIO_RC11_Toggle();
//            previousMillis[2] = currentMillis[2];
//        }
//
//        bool apps_error_flag;
//        if (apps_error_flag == 1) {
//            // transmit to can
//        }
//
//        for (int i = 0; i < 64; i++) {
//            message[i] += 1;
//        }
//
//        //-- TRANSMIT CAN FRAME -----------------------------
//
//        currentMillis[0] = millis();
//        if (currentMillis[0] - previousMillis[0] >= 100) {
//            // status = CAN1_ErrorGet();
//            //  debug_printf("C_STAT: %d\r\n", status);
//
//            // Can_transmit(0x123, message, 8);
//            /*
//                        static uint8_t message_420[8];
//                        message_420[0] = 0x00;
//                        message_420[1] = 0x10;
//                        message_420[2] = 0x20;
//                        message_420[3] = 0x30;
//                        message_420[4] = 0x40;
//                        message_420[5] = 0x50;
//                        message_420[6] = 0x60;
//                        message_420[7] = 0x70;
//                        */
//            // Can_transmit(0x420, message_420, 8);
//
//            message_ADC[0] = ADC[0] >> 8;  // APPS1
//            message_ADC[1] = ADC[0];
//
//            message_ADC[2] = ADC[3] >> 8;  // APPS2
//            message_ADC[3] = ADC[3];
//
//            // Can_transmit(0x69, message_ADC, 8);
//
//            previousMillis[0] = currentMillis[0];
//        }
//
//        //-- RECEIVE CAN FRAME -----------------------------
//
        status = CAN1_ErrorGet();

        if (status == CANFD_ERROR_NONE) {
            // printf("BUStat: %d", status);
            //  TODO memset
            memset(rx_message, 0x00, sizeof(rx_message));
            /*
            for (int i = 0; i < 64; i++) {
                rx_message[i] = 0x00;
            }*/

            /* Receive New Message */

            if (CAN1_MessageReceive(&rx_messageID, &rx_messageLength, rx_message, 0, 2, &msgAttr)) {
                debug_printf(" New Message Received    \r\n");
                /* Print message to Console */
                uint8_t length = rx_messageLength;
                debug_printf("ID: 0x%x Len: 0x%x ", (unsigned int)rx_messageID, (unsigned int)rx_messageLength);
                debug_printf("Msg: ");

                // pass the value to be written on labview
                /*
                ID = rx_messageID;
                for (int i = 0; i < 8; i++) {
                    canrx[i] = rx_message[i];
                }*/

                while (length) {
                    debug_printf("%d ", rx_message[rx_messageLength - length--]);
                }

                debug_printf("\r\n");
//
//                // GPIO_RC2_Toggle();
//                CANRX_ON = 1;
                } else {
                    debug_printf("No new message\r\n");
//                CANRX_ON = 0;
                }
               } else {
                debug_printf("BUStat: %d\r\n", status);
//              CANRX_ON = 0;
            }
//
//        // -- read ADC0 and ADC3 ----------------------------
//        /*
//        currentMillis[1] = millis();
//        if (currentMillis[1] - previousMillis[1] >= 100) {
//            Read_ADC(ADCHS_CH53);
//            Read_ADC(ADCHS_CH0);  // APPS1
//            // debug_printf("APPS1: %d\r\n", ADC[0]);
//            Read_ADC(ADCHS_CH3);  // APPS2
//            // debug_printf("APPS2: %d\r\n", ADC[3]);
//
//            apps_error_flag = APPS_Function(ADC[0], ADC[3]);
//
//            previousMillis[1] = currentMillis[1];
//        }
//        */
    }
    /* Execution should not come here during normal operation */
    return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/

void Read_ADC(ADCHS_CHANNEL_NUM channel) {
    ADCHS_ChannelConversionStart(channel);

    if (ADCHS_ChannelResultIsReady(channel)) {
        ADC[channel] = ADCHS_ChannelResultGet(channel);
        // debug_printf("ADC%d: %d\r\n", channel, ADC[channel]);
    }
}

void Can_transmit(uint32_t id, uint8_t* message, uint8_t size) {
    if (CAN1_TxFIFOQueueIsFull(0)) {
        // debug_printf("CAN1_TxFIFOQueueIsFull\r\n");
        CANTX_ON = 0;
    } else {
        if (CAN1_MessageTransmit(id, size, message, 0, CANFD_MODE_NORMAL, CANFD_MSG_TX_DATA_FRAME)) {
            // debug_printf("id 0x%x sent successfully \r\n", id);
            //  debug_printf("ID: 0x%x Len: 0x%x DATA:", (unsigned int)id, (unsigned int)size);
            for (int i = 0; i < size; i++) {
                // debug_printf("%d ", message[i]);
            }
            // debug_printf("\r\n");
            //  GPIO_RC2_Toggle();
            CANTX_ON = 1;
        } else {
            // debug_printf("id 0x%x not sent\r\n", id);
            CANTX_ON = 0;
        }
    }
}

bool APPS_Function(uint16_t APPS1, uint16_t APPS2) {
    // APPS1 0-4095
    // APPS2 4095-0

    static unsigned long error_start_time;

    // invert APPS2
    // TODO macro bellow
    APPS2 = 4095 - APPS2;

    // Check if APPS1 and APPS2 are too far apart
    int diff = abs(APPS1 - APPS2);
    // TODO make this into variables to make it easier to debug

    if (diff > 409 || (APPS1 < 5 || APPS2 < 5) || (APPS1 > 3995 || APPS2 > 3995)) {
        if (error_flag == 0) {
            // Error detected, start timer
            error_start_time = millis();
            error_flag = 1;
        } else {
            // Error already detected, check if timer has expired
            unsigned long current_time = millis();
            if (current_time - error_start_time > 100) {
                // Error has persisted for more than 100 ms, set error flag
                error_flag = 1;
            }
        }
    } else {
        // No error, reset error flag and timer
        error_flag = 0;
        error_start_time = 0;
    }

    // calculates the average and percentage of the two APPS
    APPS_average = (APPS1 + APPS2) / 2;
    // debug_printf("APPS_average: %d\r\n", APPS_average);
    APPS_percent = (APPS_average * 100) / 4095;

    // debug_printf("APPSA%d APPSB%d APPST%d APPS_ERROR%d\r", APPS1, APPS2, APPS_percent, error_flag);
    //  debug_printf("APPS1:10\n");
    return error_flag;
}

/*
    CODE IN LOOP FUNCTION:

    bool R2D_flag = 0;
    if (flag_TCU){
        R2D_flag = R2D_sound();
    }
*/

bool R2D_sound() {
    static bool Sound_repeat = 0;  // Should be 1 if sound was played and never 0 again
    static uint32_t time = 0;      // Time when sound was played

    if (Sound_repeat == 0) {
        MCPWM_Start();
        Sound_repeat = 1;
        time = millis();
    } else {
        if (millis() - time > 3000) {
            MCPWM_Stop();
            return true;
        }
    }
    return false;
}

/*
The status  ??AS Emergency ?? has to be indicated by an intermittent sound with the following
parameters:
 ?  on-/off-frequency: 1 Hz to 5 Hz
 ?  duty cycle 50 %
 ?  sound level between 80 dBA and 90 dBA, fast weighting in a radius of 2 m around the
vehicle.
 ?  duration between 8 s and 10 s after entering  ??AS Emergency ??🥹
*/
