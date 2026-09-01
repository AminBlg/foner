# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fonerprobe probe.cpp)
target_link_libraries(fonerprobe PRIVATE fonercore Qt6::Core Qt6::DBus)
target_compile_definitions(fonerprobe PRIVATE
    QC_PHONEBOOK_VCF="${CMAKE_CURRENT_SOURCE_DIR}/sample.vcf")

add_executable(test_callend test_callend.cpp)
target_link_libraries(test_callend PRIVATE fonercore Qt6::Test Qt6::DBus)
add_test(NAME callend COMMAND test_callend)

add_executable(test_contacts test_contacts.cpp)
target_link_libraries(test_contacts PRIVATE fonercore Qt6::Test)
add_test(NAME contacts COMMAND test_contacts)
