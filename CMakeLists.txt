cmake_minimum_required (VERSION 3.10)
                
project (buddy_allocator)

add_executable(buddy_allocator test.c buddy_allocator.c)
