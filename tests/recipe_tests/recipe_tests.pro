TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += \
    product_recipe_test \
    recipe_store_test

product_recipe_test.subdir = product_recipe_test
recipe_store_test.subdir = recipe_store_test
