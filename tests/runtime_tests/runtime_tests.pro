TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    barcode_decoder_adapter_test \
    plc_device_adapter_test

barcode_decoder_adapter_test.subdir = barcode_decoder_adapter_test
plc_device_adapter_test.subdir = plc_device_adapter_test
