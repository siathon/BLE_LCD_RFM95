#include "ble/BLE.h"

class CounterService{
public:
    CounterService(BLEDevice &_ble, int cntr):
    ble(_ble),
    valueBytes(cntr),
    counter(0x1001, valueBytes.getPointer(),
         sizeof(float), sizeof(float), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY){
             GattCharacteristic *charTable[] = {&counter};
             GattService         counterService(0xA000, charTable, 1);
             ble.addService(counterService);
    }

    void updateCounter(int cntr){
        valueBytes.updateCounter(cntr);
        ble.gattServer().write(counter.getValueHandle(), valueBytes.getPointer(), sizeof(CounterValueBytes));
    }

    struct CounterValueBytes {
        static const unsigned MAX_VALUE_BYTES  = sizeof(int);

        CounterValueBytes(int cntr) : valueBytes() {
            updateCounter(cntr);
        }

        void updateCounter(int cntr) {
            memcpy(&valueBytes, &cntr, sizeof(int));
        }

        uint8_t *getPointer(void) {
            return valueBytes;
        }

    private:
       uint8_t valueBytes[MAX_VALUE_BYTES];
    };

    BLEDevice &ble;
    CounterValueBytes valueBytes;
    GattCharacteristic counter;
};
