#include <QtTest>

#include "devices/plc/snap7_plc_device.h"

class PlcDeviceAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void connectForwardsEndpointAndResult();
    void disconnectForwardsResult();
    void connectionStateUsesBackend();
    void byteWriteUsesDbAreaAndByteWidth();
    void wordWriteUsesDbAreaAndWordWidth();
    void dwordWriteUsesDbAreaAndDwordWidth();
    void missingBackendOperationsFailSafely();
};

void PlcDeviceAdapterTest::connectForwardsEndpointAndResult()
{
    QByteArray capturedAddress;
    int capturedRack = -1;
    int capturedSlot = -1;

    Snap7PlcFunctions functions;
    functions.connectTo = [&](const char *address, int rack, int slot) {
        capturedAddress = address;
        capturedRack = rack;
        capturedSlot = slot;
        return 23;
    };

    Snap7PlcDevice device(functions);
    const PlcOperationResult result =
            device.connectTo("192.168.10.10", 2, 3);
    QCOMPARE(result.nativeErrorCode, 23);
    QVERIFY(!result.isSuccess());
    QCOMPARE(capturedAddress, QByteArray("192.168.10.10"));
    QCOMPARE(capturedRack, 2);
    QCOMPARE(capturedSlot, 3);
}

void PlcDeviceAdapterTest::disconnectForwardsResult()
{
    int callCount = 0;
    Snap7PlcFunctions functions;
    functions.disconnect = [&]() {
        ++callCount;
        return 31;
    };

    Snap7PlcDevice device(functions);
    const PlcOperationResult result = device.disconnect();
    QCOMPARE(result.nativeErrorCode, 31);
    QVERIFY(!result.isSuccess());
    QCOMPARE(callCount, 1);
}

void PlcDeviceAdapterTest::connectionStateUsesBackend()
{
    bool connected = false;
    Snap7PlcFunctions functions;
    functions.isConnected = [&]() {
        return connected;
    };

    Snap7PlcDevice device(functions);
    QVERIFY(!device.isConnected());
    connected = true;
    QVERIFY(device.isConnected());
}

void PlcDeviceAdapterTest::byteWriteUsesDbAreaAndByteWidth()
{
    int capturedArea = -1;
    int capturedDb = -1;
    int capturedStart = -1;
    int capturedAmount = -1;
    int capturedWordLength = -1;
    void *capturedData = nullptr;

    Snap7PlcFunctions functions;
    functions.writeArea = [&](int area,
                              int dbNumber,
                              int start,
                              int amount,
                              int wordLength,
                              void *data) {
        capturedArea = area;
        capturedDb = dbNumber;
        capturedStart = start;
        capturedAmount = amount;
        capturedWordLength = wordLength;
        capturedData = data;
        return 41;
    };

    unsigned char data[1] = {49};
    Snap7PlcDevice device(functions);
    const PlcOperationResult result = device.writeDbArea(
                1, 1033, 1, PlcDataWidth::Byte, data);
    QCOMPARE(result.nativeErrorCode, 41);
    QVERIFY(!result.isSuccess());
    QCOMPARE(capturedArea, 0x84);
    QCOMPARE(capturedDb, 1);
    QCOMPARE(capturedStart, 1033);
    QCOMPARE(capturedAmount, 1);
    QCOMPARE(capturedWordLength, 0x02);
    QVERIFY(capturedData == data);
    QCOMPARE(static_cast<int>(data[0]), 49);
}

void PlcDeviceAdapterTest::wordWriteUsesDbAreaAndWordWidth()
{
    int capturedArea = -1;
    int capturedStart = -1;
    int capturedAmount = -1;
    int capturedWordLength = -1;

    Snap7PlcFunctions functions;
    functions.writeArea = [&](int area,
                              int,
                              int start,
                              int amount,
                              int wordLength,
                              void *) {
        capturedArea = area;
        capturedStart = start;
        capturedAmount = amount;
        capturedWordLength = wordLength;
        return 0;
    };

    unsigned char data[2] = {0x12, 0x34};
    Snap7PlcDevice device(functions);
    const PlcOperationResult result = device.writeDbArea(
                1, 980, 2, PlcDataWidth::Word, data);
    QCOMPARE(result.nativeErrorCode, 0);
    QVERIFY(result.isSuccess());
    QCOMPARE(capturedArea, 0x84);
    QCOMPARE(capturedStart, 980);
    QCOMPARE(capturedAmount, 2);
    QCOMPARE(capturedWordLength, 0x04);
}

void PlcDeviceAdapterTest::dwordWriteUsesDbAreaAndDwordWidth()
{
    int capturedArea = -1;
    int capturedStart = -1;
    int capturedAmount = -1;
    int capturedWordLength = -1;

    Snap7PlcFunctions functions;
    functions.writeArea = [&](int area,
                              int,
                              int start,
                              int amount,
                              int wordLength,
                              void *) {
        capturedArea = area;
        capturedStart = start;
        capturedAmount = amount;
        capturedWordLength = wordLength;
        return 47;
    };

    unsigned char data[4] = {0x01, 0x02, 0x03, 0x04};
    Snap7PlcDevice device(functions);
    const PlcOperationResult result = device.writeDbArea(
                1, 924, 4, PlcDataWidth::DWord, data);
    QCOMPARE(result.nativeErrorCode, 47);
    QCOMPARE(capturedArea, 0x84);
    QCOMPARE(capturedStart, 924);
    QCOMPARE(capturedAmount, 4);
    QCOMPARE(capturedWordLength, 0x06);
}

void PlcDeviceAdapterTest::missingBackendOperationsFailSafely()
{
    const Snap7PlcFunctions functions;
    Snap7PlcDevice device(functions);
    unsigned char data[1] = {0};

    QVERIFY(!device.isConnected());
    QCOMPARE(device.connectTo(
                 "127.0.0.1", 0, 1).nativeErrorCode, -1);
    QCOMPARE(device.disconnect().nativeErrorCode, -1);
    QCOMPARE(device.writeDbArea(
                 1, 1033, 1, PlcDataWidth::Byte, data)
             .nativeErrorCode, -1);
}

QTEST_APPLESS_MAIN(PlcDeviceAdapterTest)

#include "plc_device_adapter_test.moc"
