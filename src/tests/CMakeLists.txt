set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )

find_package(Qt6Test CONFIG REQUIRED)
include(ECMAddTests)

# ECM's install-manifest check skips a fresh build before package(). Validate
# the metadata directly in check(), without network requests or a host install.
find_program(AERO7_APPSTREAMCLI_EXECUTABLE NAMES appstreamcli REQUIRED)
add_test(NAME aero7appstreammetadata
    COMMAND ${AERO7_APPSTREAMCLI_EXECUTABLE} validate --no-net
        ${CMAKE_CURRENT_SOURCE_DIR}/../org.aero7.FileExplorer.metainfo.xml)
set_tests_properties(aero7appstreammetadata PROPERTIES TIMEOUT 20)

ecm_add_test(aero7commondialogtest.cpp LINK_LIBRARIES aero7shellui Qt6::Test)
set_tests_properties(aero7commondialogtest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

include(FindGem)  # For servicemenutest, see bottom of this file

ecm_add_test(aero7storagetest.cpp LINK_LIBRARIES Qt6::Core Qt6::Test)
ecm_add_test(aero7downloadsgroupingtest.cpp LINK_LIBRARIES Qt6::Core Qt6::Test)
target_include_directories(aero7downloadsgroupingtest PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(aero7iconindependencetest aero7iconindependencetest.cpp)
target_link_libraries(aero7iconindependencetest PRIVATE aero7shellui Qt6::Widgets)
add_test(NAME aero7iconindependencetest COMMAND aero7iconindependencetest)
set_tests_properties(aero7iconindependencetest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(aero7librariesdefaultstest aero7librariesdefaultstest.cpp)
target_link_libraries(aero7librariesdefaultstest PRIVATE aero7shellui Qt6::Core)
add_test(NAME aero7librariesfreshprofile COMMAND aero7librariesdefaultstest)
add_test(NAME aero7librariesexistingprofile COMMAND aero7librariesdefaultstest existing)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME aero7iconpolicytest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/aero7iconpolicytest.py)

# KItemSetTest
ecm_add_test(kitemsettest.cpp LINK_LIBRARIES dolphinprivate Qt6::Test)

# KItemRangeTest
ecm_add_test(kitemrangetest.cpp LINK_LIBRARIES dolphinprivate Qt6::Test)


# KItemListSelectionManagerTest
ecm_add_test(kitemlistselectionmanagertest.cpp LINK_LIBRARIES dolphinprivate Qt6::Test)

# KItemListControllerTest
ecm_add_test(kitemlistcontrollertest.cpp testdir.cpp
TEST_NAME kitemlistcontrollertest
LINK_LIBRARIES dolphinprivate Qt6::Test)

# KItemListControllerExpandTest
ecm_add_test(kitemlistcontrollerexpandtest.cpp testdir.cpp
TEST_NAME kitemlistcontrollerexpandtest
LINK_LIBRARIES dolphinprivate Qt6::Test)

# KFileItemListViewTest
ecm_add_test(kfileitemlistviewtest.cpp testdir.cpp
TEST_NAME kfileitemlistviewtest
LINK_LIBRARIES dolphinprivate Qt6::Test)

# KFileItemModelTest
ecm_add_test(kfileitemmodeltest.cpp testdir.cpp
TEST_NAME kfileitemmodeltest
LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test)

# KFileItemModelBenchmark, not run automatically with `ctest` or `make test`
add_executable(kfileitemmodelbenchmark kfileitemmodelbenchmark.cpp testdir.cpp)
target_link_libraries(kfileitemmodelbenchmark dolphinprivate Qt6::Test)

# KItemListKeyboardSearchManagerTest
ecm_add_test(kitemlistkeyboardsearchmanagertest.cpp LINK_LIBRARIES dolphinprivate Qt6::Test)

# DolphinSearchBar
if (KF6Baloo_FOUND)
    ecm_add_test(dolphinsearchbartest.cpp
    TEST_NAME dolphinsearchbartest
    LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test)
endif()

# DolphinQuery
if (KF6Baloo_FOUND)
    ecm_add_test(dolphinquerytest.cpp
    TEST_NAME dolphinquerytest
    LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test)
endif()

# ViewPropertiesTest
ecm_add_test(viewpropertiestest.cpp testdir.cpp
TEST_NAME viewpropertiestest
LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test KF6::FileMetaData)

# DolphinMainWindowTest
ecm_add_test(dolphinmainwindowtest.cpp testdir.cpp ${CMAKE_SOURCE_DIR}/src/dolphin.qrc
TEST_NAME dolphinmainwindowtest
LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test)

# DragAndDropHelperTest
ecm_add_test(draganddrophelpertest.cpp LINK_LIBRARIES dolphinprivate Qt6::Test)

find_gem(test-unit)
set_package_properties(Gem_test-unit PROPERTIES
    TYPE RECOMMENDED
    DESCRIPTION "Ruby gem 'test-unit' required for testing of servicemenu helpers.")
if (Gem_test-unit_FOUND)
    add_test(NAME servicemenutest
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../settings/contextmenu/test/test_run.rb)
endif()
