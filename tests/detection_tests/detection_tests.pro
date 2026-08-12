TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    tissue_roll_detector_baseline_test \
    ocr_detection_pipeline_test \
    stamp_detection_pipeline_test \
    word_detection_pipeline_test \
    barcode_word_detection_pipeline_test \
    profile_pose_selector_test

tissue_roll_detector_baseline_test.subdir = tissue_roll_detector_baseline_test
ocr_detection_pipeline_test.subdir = ocr_detection_pipeline_test
stamp_detection_pipeline_test.subdir = stamp_detection_pipeline_test
word_detection_pipeline_test.subdir = word_detection_pipeline_test
barcode_word_detection_pipeline_test.subdir = barcode_word_detection_pipeline_test
profile_pose_selector_test.subdir = profile_pose_selector_test
