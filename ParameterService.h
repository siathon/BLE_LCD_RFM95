#include "ble/BLE.h"

class ParameterService{
public:
    ParameterService(BLEDevice &_ble, int param):
    ble(_ble),
    valueBytes(param),
    parameter(0x1001, valueBytes.getPointer(),
         sizeof(int), sizeof(int), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE){
             GattCharacteristic *charTable[] = {&parameter};
             GattService         parameterService(0xF000, charTable, 1);
             ble.addService(parameterService);
    }

    GattAttribute::Handle_t getValueHandle() const{
        return parameter.getValueHandle();
    }

    struct ParamValueBytes {
        ParamValueBytes(int prm) : valueBytes() {
            updateParam(prm);
        }

        void updateParam(int prm) {
            memcpy(&valueBytes, &prm, sizeof(int));
        }

        uint8_t *getPointer(void) {
            return valueBytes;
        }

    private:
        uint8_t valueBytes[4];
    };
    BLEDevice &ble;
    ParamValueBytes valueBytes;
    GattCharacteristic parameter;
};
