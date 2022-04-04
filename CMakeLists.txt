cmake_minimum_required(VERSION 3.11)

project(KLSXX)

kls_add_library_module(kls.journal klsxx::journal)
kls_public_source_directory(kls.journal Published)
kls_module_source_directory(kls.journal Module)
target_link_libraries(kls.journal PUBLIC kls.essential kls.coroutine kls.thread kls.io)
