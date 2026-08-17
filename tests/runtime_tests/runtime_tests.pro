TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    application_service_test \
    barcode_decoder_adapter_test \
    plc_device_adapter_test \
    camera_session_test \
    detection_completion_test \
    system_support_test

application_service_test.subdir = application_service_test
barcode_decoder_adapter_test.subdir = barcode_decoder_adapter_test
plc_device_adapter_test.subdir = plc_device_adapter_test
camera_session_test.subdir = camera_session_test
detection_completion_test.subdir = detection_completion_test
system_support_test.subdir = system_support_test
