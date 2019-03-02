#include <string>
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "Adafruit_GFX.h"
#include "TFT_ILI9163C.h"
#include "Org_01.h"
#include "events/EventQueue.h"
#include "lorawan/LoRaWANInterface.h"
#include "lorawan/system/lorawan_data_structures.h"
#include "SX1276_LoRaRadio.h"
// #include "karkard.h"
// #include "debit.h"
// #include "bottom.h"
#include "CounterService.h"
#include "ParameterService.h"
#include "Watchdog.h"
#include "I2CEeprom.h"

#define DEBUG 1

#define __MOSI p23
#define __MISO p21
#define __SCLK p25
#define __CS   p5
#define __DC   p11
#define __RST  p9

using namespace std;
using namespace events;
Watchdog wd;
Serial pc(p14, p16);
SX1276_LoRaRadio radio(MBED_CONF_APP_LORA_SPI_MOSI,
                           MBED_CONF_APP_LORA_SPI_MISO,
                           MBED_CONF_APP_LORA_SPI_SCLK,
                           MBED_CONF_APP_LORA_CS,
                           MBED_CONF_APP_LORA_RESET,
                           MBED_CONF_APP_LORA_DIO0,
                           MBED_CONF_APP_LORA_DIO1,
                           MBED_CONF_APP_LORA_DIO2,
                           MBED_CONF_APP_LORA_DIO3,
                           MBED_CONF_APP_LORA_DIO4,
                           MBED_CONF_APP_LORA_DIO5,
                           MBED_CONF_APP_LORA_RF_SWITCH_CTL1,
                           MBED_CONF_APP_LORA_RF_SWITCH_CTL2,
                           MBED_CONF_APP_LORA_TXCTL,
                           MBED_CONF_APP_LORA_RXCTL,
                           MBED_CONF_APP_LORA_ANT_SWITCH,
                           MBED_CONF_APP_LORA_PWR_AMP_CTL,
                           MBED_CONF_APP_LORA_TCXO);

static EventQueue ev_queue(20 * EVENTS_EVENT_SIZE);
static LoRaWANInterface lorawan(radio);
static lorawan_app_callbacks_t callbacks;
lorawan_status_t retcode;

TFT_ILI9163C tft(__MOSI, __MISO, __SCLK, __CS, __DC, __RST);

I2CEeprom eeprom(p30, p7, 0xA0, 64, 0);

DigitalOut nrfLED(p12, 0);
DigitalOut lcdEn(p19, 1);
DigitalOut sensorEn(p22);
AnalogIn sensor(p3);

const static char     DEVICE_NAME[] = "0.5Inch_4";
static const uint16_t uuid16_list[] = {0xA000};
CounterService *counterService;
ParameterService *parameterService;

int f1 = 0, f2 = 0, flow = 0, flowCC = 0, oldFlowCC = -1;
int scanCnt = 0, sampleCount = 10, sampleTime = 10, meanValue = 0, battValue = 100, oldBattValue = -1;
int pin = 0, ready = 0;
bool connected = false, setLowTh = false, setHighTh = false, setSampleTime = false, setSampleCount = false, setCounterParameter = false;
int counter = 0, oldCounter = -1, counterLitr = 0, oldCounterLitr = -1, sensorValue = 0, oldSensorValue = -1;
char str[10];
float counterParameter = 3.46;
int16_t  x1, y1;
uint16_t w, h;
int highTh = 70, lowTh = 30;
int rstCnt = 0;

uint8_t tx_buffer[10];
uint8_t rx_buffer[10];
bool joined = false;
bool joining = false;
bool sending = false;
string output;

void blink(){
  nrfLED = !nrfLED;
}

void readFromEeprom() {

    if (DEBUG) {
        pc.printf("Reading counter from EEPROM...");
    }
    wd.Service();
    if (eeprom.read(0, str, 10) == 10) {
        if (DEBUG) {
            pc.printf("Done.(Value read = %s)\n", str);
            counter = atoi(str);
        }
    } else {
        if (DEBUG) {
            pc.printf("Failed.\n");
        }
    }
}

void writeToEeprom() {

    sprintf(str, "%09d", counter);
    if (DEBUG) {
        pc.printf("Writing counter(%d) in EEPROM...", counter);
    }
    wd.Service();
    if (eeprom.write(0, str, sizeof(str)) == sizeof(str)) {
        if (DEBUG) {
            pc.printf("Done.\n");
        }
    } else {
        if (DEBUG) {
            pc.printf("Failed.\n");
        }
    }
}

int joinOTAA(){
    if (DEBUG) {
        pc.printf("Join OTAA...");
    }
    retcode = lorawan.connect();
    if (retcode ==  LORAWAN_STATUS_OK || retcode == LORAWAN_STATUS_CONNECT_IN_PROGRESS) {
        joining = true;
        return 0;
    }
    lorawan.disconnect();
    return -1;
}

int send(int count){
    int16_t result;
    int packetLen;
    rstCnt++;
    packetLen = sprintf((char*)tx_buffer, "W[01]=%d,W[02]=%d,W[03]=0", counterLitr, rstCnt);
    if (DEBUG) {
        pc.printf("Sending Message: count = %d ...", counterLitr);
    }
    result = lorawan.send(MBED_CONF_LORA_APP_PORT, tx_buffer, packetLen, MSG_CONFIRMED_FLAG);
    if (result < 0) {
        return -1;
    }
    sending = true;
    return 0;
}

// int receive(){
//     int16_t retcode;
//     retcode = lorawan.receive(MBED_CONF_LORA_APP_PORT, rx_buffer,
//                               sizeof(rx_buffer),
//                               MSG_CONFIRMED_FLAG|MSG_UNCONFIRMED_FLAG);
//
//     if (retcode < 0) {
//         if(DEBUG){
//             pc.printf("receive() - Error code %d\n", retcode);
//         }
//         return -1;
//     }
//     if(DEBUG){
//         pc.printf("Data:");
//
//         for (uint8_t i = 0; i < retcode; i++) {
//             pc.printf("%x", rx_buffer[i]);
//         }
//         pc.printf("Data Length: %d\n", retcode);
//     }
//
//     memset(rx_buffer, 0, sizeof(rx_buffer));
//     return 0;
// }

void checkJoinAndSend() {
    if (joining) {
        wait(4);
    }
    if (!joined) {
        ev_queue.call_in(1000, joinOTAA);
        return;
    }
    if (sending) {
        wait(4);
    }
    send(counterLitr);
}

static void lora_event_handler(lorawan_event_t event){
    switch (event) {
        case CONNECTED:
            if(DEBUG){
                pc.printf("Done.\n");
            }
            joined = true;
            joining = false;
            ev_queue.call_in(1000, checkJoinAndSend);
            break;

        case DISCONNECTED:
            if(DEBUG){
                pc.printf("Disconnected\n");
            }
            break;

        case TX_DONE:
            if(DEBUG){
                pc.printf("Done\n");
            }
            sending = false;
            break;

        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
        case TX_TIMEOUT:
            if(DEBUG){
                pc.printf("Failed, timeout!\n");
            }
            sending = false;
            ev_queue.call_in(1000, checkJoinAndSend);
            break;

        //
        // case RX_DONE:
        //     if(DEBUG){
        //         pc.printf("Received message from Network Server\n");
        //     }
        //     receive();
        //     break;
        //
        // case RX_TIMEOUT:
        //     if(DEBUG){
        //         pc.printf("Message recieve timeout\n");
        //     }
        //     break;
        //
        // case RX_ERROR:
        //     if(DEBUG){
        //         pc.printf("Error in reception - Code = %d\n", event);
        //     }
        //     break;

        case JOIN_FAILURE:
            if(DEBUG){
                pc.printf("OTAA Failed - Check Keys\n");
            }
            joining = false;
            break;

        case UPLINK_REQUIRED:
            if(DEBUG){
                pc.printf("Uplink required by NS\n");
            }
            break;

        default:
            MBED_ASSERT("Unknown Event");
    }
}

int initLora(){
    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        if(DEBUG){
            pc.printf("LoRa initialization failed!\n");
        }
        return -1;
    }
    // lorawan.enable_adaptive_datarate();
    lorawan.set_confirmed_msg_retries(3);
    int version = radio.read_register(0x42);
    if (version == 0) {
        if (DEBUG) {
            pc.printf("Radio initialization failed\n");
        }
        return -1;
    }

    callbacks.events = mbed::callback(lora_event_handler);
    lorawan.add_app_callbacks(&callbacks);

    // if (lorawan.set_confirmed_msg_retries(3) != LORAWAN_STATUS_OK) {
    //     if (DEBUG) {
    //         pc.printf("set_confirmed_msg_retries failed!\n");
    //     }
    //     return -1;
    // }
//
    // if (lorawan.set_device_class(CLASS_A) != LORAWAN_STATUS_OK) {
    //     if (DEBUG) {
    //         pc.printf("set_device_class class_a failed!\n");
    //     }
    //     return -1;
    // }
    //
    // if (lorawan.enable_adaptive_datarate() != LORAWAN_STATUS_OK) {
    //     if (DEBUG) {
    //         pc.printf("enable_adaptive_datarate failed!\n");
    //     }
    //     return -1;
    // }

    return 0;
}

void resetParams(){
    counter = 0;
    counterLitr = 0;
    oldCounterLitr = -9999;
    flow = 0;
    flowCC = 0;
    oldFlowCC = -9999;
    scanCnt = 0;
    writeToEeprom();
}

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params){
    (void) params;
    BLE::Instance().gap().startAdvertising();
    connected = false;
    if (DEBUG) {
        pc.printf("BLE disconnected\n");
    }
}

void connectionCallback(const Gap::ConnectionCallbackParams_t *params) {
    BLE::Instance().gap().stopAdvertising();
    connected = true;
    if (DEBUG) {
        pc.printf("BLE connected\n");
    }
}

void onDataWrittenCallback(const GattWriteCallbackParams *params) {
    if ((params->handle == parameterService->getValueHandle()) && (params->len == 4)) {
        int temp = 0;
        for (size_t i = 0; i < 4; i++) {
            temp += (params->data[i] << (8*i));
        }
        if (temp == 9999) {
            // writeToEeprom();
            NVIC_SystemReset();
        }

        if (temp == 1000) {
            resetParams();
            return;
        }

        if (temp == 1001) {
            setLowTh = true;
            return;
        }

        if (temp == 1002) {
            setHighTh = true;
            return;
        }

        // if (temp == 1003) {
        //     setSampleCount = true;
        //     return;
        // }
        //
        // if (temp == 1004) {
        //     setSampleTime = true;
        //     return;
        // }

        if (temp == 1005) {
            setCounterParameter = true;
            return;
        }

        if (setLowTh) {
            lowTh = temp;
            setLowTh = false;
            return;
        }

        if (setHighTh) {
            highTh = temp;
            setHighTh = false;
            return;
        }

        // if (setSampleCount) {
        //     sampleCount = temp;
        //     setSampleCount = false;
        //     return;
        // }
        //
        // if (setSampleTime) {
        //     sampleTime = temp;
        //     setSampleTime = false;
        //     return;
        // }
        if(setCounterParameter){
            counterParameter = (float)(temp / 100.0);
        }
    }
}

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params){
    BLE&        ble   = params->ble;
    // ble_error_t error = params->error;

    /* Ensure that it is the default instance of BLE */
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);
    ble.gap().onConnection(connectionCallback);

    counterService = new CounterService(ble, counter);
    parameterService = new ParameterService(ble, counterParameter);

    ble.gattServer().onDataWritten(onDataWrittenCallback);

    /* setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* 1s. */
    ble.gap().startAdvertising();
}

void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext* context) {
    BLE &ble = BLE::Instance();
    ev_queue.call(Callback<void()>(&ble, &BLE::processEvents));
}

void LCDTextInit(){
    tft.sleepMode(false);

    //tft.drawRGBBitmap(0, 0, menu128, 128, 128);
    //tft.drawRGBBitmap(0, 0, karkard, 69, 17);
    //tft.drawRGBBitmap(0, 50, debit, 85, 13);
    tft.clearScreen();
    // tft.drawRGBBitmap(0, 100, bottom, 128, 26);
    tft.setFont(&Org_01);
    tft.setTextSize(2);
    sprintf(str, "Count:");
    tft.setCursor(5, 16);
    tft.print(str);

    sprintf(str, "Flow:");
    tft.setCursor(5, 52);
    tft.print(str);

    tft.setTextSize(1);
    tft.setTextColor(WHITE);

    tft.setCursor(5, 95);
    sprintf(str, "B:");
    tft.print(str);

    tft.setCursor(60, 95);
    sprintf(str, "| IR:");
    tft.print(str);

    tft.setCursor(5, 87);
    sprintf(str, "P:");
    tft.print(str);
}

void showCounter(){
    tft.setTextSize(1);
    sprintf(str, "%d", oldCounter);
    tft.getTextBounds(str, 15, 87, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w, h, BLACK);
    sprintf(str, "%d", counter);
    tft.setCursor(15, 87);
    tft.setTextColor(WHITE);
    tft.print(str);
}

void showFlow(){
    tft.setTextSize(2);
    sprintf(str, "%d", oldFlowCC);
    tft.getTextBounds(str, 40, 70, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w, h, BLACK);
    sprintf(str, "%d", flowCC);
    tft.setCursor(40, 70);
    tft.setTextColor(YELLOW);
    tft.print(str);
}

void showBatt() {
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    sprintf(str, "%3d%%", oldBattValue);
    tft.getTextBounds(str, 15, 95, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w, h, BLACK);
    sprintf(str, "%3d%%", battValue);
    tft.setCursor(15, 95);
    tft.print(str);
}

void showSensorValue(){
    tft.setTextSize(1);
    sprintf(str, "%3d", oldSensorValue);
    tft.getTextBounds(str, 90, 95, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w, h, BLACK);
    sprintf(str, "%3d", sensorValue);
    tft.setCursor(90, 95);
    tft.setTextColor(WHITE);
    tft.print(str);
}

void showLitr(){
    tft.setTextSize(2);
    sprintf(str, "%d", oldCounterLitr);
    tft.getTextBounds(str, 10, 36, &x1, &y1, &w, &h);
    tft.fillRect(x1, y1, w, h, BLACK);
    sprintf(str, "%d", counterLitr);
    tft.setCursor(10, 36);
    tft.setTextColor(YELLOW);
    tft.print(str);
}

void scanPin(){
    wd.Service();
    scanCnt++;
    sensorEn = 1;
    uint16_t sum = 0;
    for (size_t i = 0; i < sampleCount; i++) {
        sum += sensor.read_u16();
    }
    sensorEn = 0;
    meanValue = sum / sampleCount;

    // pc.printf("Mean Value = %d\n", meanValue);
    // if (connected) {
    //     barService->updateBar(meanValue);
    // }
    if (pin == 1){
        ready = 1;
    }
    if (meanValue < lowTh) {
        pin = 0;
    }
    else if(meanValue > highTh){
        pin = 1;
    }
    // pc.printf("%d\n", pin);
    if((!pin) && ready){
        counter++;
        ready = 0;
    }
    if (scanCnt == 1500) {
        f1 = f2;
        f2 = counter;
        flow = (f2 - f1) * 4;
        scanCnt = 0;

    }
    if (scanCnt % 50 == 0) {
        sensorValue = meanValue;
        nrfLED = !nrfLED;
    }
}

void update(){
    if(connected){
        tft.sleepMode(false);
        lcdEn = 0;
        if(sensorValue != oldSensorValue){
            showSensorValue();
            oldSensorValue = sensorValue;
        }

        if (counter != oldCounter) {
            showCounter();
            oldCounter = counter;
        }

        counterLitr = (int)((counter / counterParameter) + 0.5);
        if(counterLitr != oldCounterLitr){
            showLitr();
            oldCounterLitr = counterLitr;
            // pc.printf("%d\n", counter);
            counterService->updateCounter(counterLitr);
        }

        flowCC = (int)((flow / counterParameter));
        if(flowCC != oldFlowCC && flowCC >= 0){
            showFlow();
            oldFlowCC = flowCC;
        }

        if (battValue != oldBattValue) {
            showBatt();
            oldBattValue = battValue;
        }
    }
    else{
        tft.sleepMode(true);
        lcdEn = 1;
    }
}
