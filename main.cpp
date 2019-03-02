#include "main.h"

int main() {
    pc.baud(9600);
    pc.set_flow_control(Serial::Disabled);

    readFromEeprom();

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);

    if (wd.WatchdogCausedReset()) {
        pc.printf("Watchdog caused reset\n");
    }

    tft.begin();
    tft.setBitrate(500000000);
    tft.setRotation(2);
    LCDTextInit();

    while (1) {
        if (DEBUG) {
            pc.printf("Initializing lora...");
        }
        if (initLora() == 0) {
            if (DEBUG) {
                pc.printf("Done.\n");
            }
            break;
        }
        else{
            if (DEBUG) {
                pc.printf("Failed!.");
            }
        }
    }
    wd.Configured(3);
    wd.Service();
    ev_queue.call_every(300000, checkJoinAndSend);
    ev_queue.call_in(1000, joinOTAA);
    ev_queue.call_every(10, scanPin);
    ev_queue.call_every(100, update);
    ev_queue.call_every(900000, writeToEeprom);
    // ev_queue.call_every(500, blink);
    ev_queue.dispatch_forever();
    return 0;
}
