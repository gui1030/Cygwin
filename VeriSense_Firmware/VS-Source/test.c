#include "hub-fw-upload.h"
#include <time.h>

struct FWImageHeader_t {
    enum SensorType_t   SensorType;
    time_t              Boottime;
    uint64_t            Password;
    uint32_t            ByteLength;
};


int test () {
    extern struct FWImageHeader_t  thing;
    thing.SensorType = 5;
    thing.ByteLength = 4;
    thing.Boottime = 3;
    thing.Password = 2;
    return sizeof(thing);
}

