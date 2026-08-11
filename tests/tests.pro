TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    recipe_tests \
    detection_tests

recipe_tests.file = recipe_tests/recipe_tests.pro
detection_tests.file = detection_tests/detection_tests.pro
