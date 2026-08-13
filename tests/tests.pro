TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    recipe_tests \
    detection_tests \
    runtime_tests

recipe_tests.file = recipe_tests/recipe_tests.pro
detection_tests.file = detection_tests/detection_tests.pro
runtime_tests.file = runtime_tests/runtime_tests.pro
